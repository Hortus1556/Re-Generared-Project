"""Decode a compiled iOS nib (NIBArchive) into JSON.

usage: nib2json.py <file.nib> [out.json]      (stdout without out.json)
       nib2json.py --raw <file.nib>           the archive's objects as they are

A compiled nib is a keyed archive in Apple's NIBArchive format:
    "NIBArchive", uint32 1, uint32 9|10,
    then counts and offsets (uint32 each) of objects, keys, values, classes.
    object: varint class, varint first value, varint value count
    key:    varint length, bytes
    value:  varint key, uint8 type, payload
            0 int8  1 int16  2 int32  3 int64  4 false  5 true
            6 float 7 double 8 data (varint length, bytes)  9 nil
            10 object (uint32 index)
    class:  varint length, varint extra count, int32 extras, name (NUL ended)
Varints are 7 bits a byte, low first; the byte with its top bit SET is the
last one.

The JSON is what a nib means to UIKit, for the engine's view loader
(docs/engine.md, "Views from nibs"):
    {"views": [<top-level views>], "outlets": [...], "actions": [...]}
    view:    {"id", "class" (custom class if any), "uiClass" (its UIKit class),
              "frame" [x,y,w,h],
              "hidden", "alpha", "tag", "userInteraction", "contentMode",
              "backgroundColor", "clipsSubviews", "subviews": [...],
              UIImageView: "image";  UIButton: "states" {normal|highlighted|
              disabled|selected: {"image", "background", "title",
              "titleColor"}}, "enabled", "selected", "adjustsImageWhenHighlighted",
              "adjustsImageWhenDisabled";
              UILabel, UITextView: "text", "font" [name, size], "textColor",
              "textAlignment", "lines"}
    outlet:  {"source" id, "destination" id, "name"}   (File's Owner is "owner")
    action:  {"source" id, "destination" id, "selector", "events" mask}
"""
import json, struct, sys


def varint(d, p):
    r = s = 0
    while True:
        b = d[p]; p += 1
        r |= (b & 0x7f) << s; s += 7
        if b & 0x80:
            return r, p


def read_archive(path):
    return parse(open(path, 'rb').read(), path)


def parse(d, path='nib'):
    """The archive's objects: (class name, [(key, value)...]) each."""
    if d[:10] != b'NIBArchive':
        raise SystemExit('%s: not a NIBArchive' % path)
    (no, oo, nk, ok, nv, ov, nc, oc) = struct.unpack_from('<8I', d, 18)
    keys, p = [], ok
    for _ in range(nk):
        n, p = varint(d, p)
        keys.append(d[p:p + n].decode('utf-8')); p += n
    classes, p = [], oc
    for _ in range(nc):
        n, p = varint(d, p)
        extra, p = varint(d, p)
        p += 4 * extra
        classes.append(d[p:p + n].rstrip(b'\0').decode('utf-8')); p += n
    values, p = [], ov
    for _ in range(nv):
        k, p = varint(d, p)
        t = d[p]; p += 1
        if t == 0: v = struct.unpack_from('<b', d, p)[0]; p += 1
        elif t == 1: v = struct.unpack_from('<h', d, p)[0]; p += 2
        elif t == 2: v = struct.unpack_from('<i', d, p)[0]; p += 4
        elif t == 3: v = struct.unpack_from('<q', d, p)[0]; p += 8
        elif t == 4: v = False
        elif t == 5: v = True
        elif t == 6: v = struct.unpack_from('<f', d, p)[0]; p += 4
        elif t == 7: v = struct.unpack_from('<d', d, p)[0]; p += 8
        elif t == 8:
            n, p = varint(d, p); v = bytes(d[p:p + n]); p += n
        elif t == 9: v = None
        elif t == 10: v = ('ref', struct.unpack_from('<I', d, p)[0]); p += 4
        else: raise SystemExit('%s: unknown value type %d' % (path, t))
        values.append((keys[k], v))
    objects, p = [], oo
    for _ in range(no):
        c, p = varint(d, p)
        first, p = varint(d, p)
        n, p = varint(d, p)
        objects.append((classes[c], values[first:first + n]))
    return objects


class Nib:
    def __init__(self, objects):
        self.o = objects

    def cls(self, i):
        return self.o[i][0]

    def get(self, i, key, default=None):
        for k, v in self.o[i][1]:
            if k == key:
                return v
        return default

    def all(self, i, key):
        return [v for k, v in self.o[i][1] if k == key]

    def deref(self, v):
        return v[1] if isinstance(v, tuple) and v[0] == 'ref' else None

    def string(self, v):
        """An NSString archived as an object (NS.bytes), or a plain value."""
        i = self.deref(v)
        if i is None:
            return v.decode('utf-8') if isinstance(v, bytes) else v
        b = self.get(i, 'NS.bytes')
        if b is None:
            return None
        return b.decode('utf-8') if isinstance(b, bytes) else b

    def array(self, v):
        i = self.deref(v)
        if i is None:
            return []
        return [x for x in self.all(i, 'UINibEncoderEmptyKey')]

    def color(self, v):
        i = self.deref(v)
        if i is None:
            return None
        c = self.cls(i)
        rgb = self.get(i, 'UIRed')
        if rgb is not None:
            return [self.get(i, 'UIRed', 0), self.get(i, 'UIGreen', 0), self.get(i, 'UIBlue', 0),
                    self.get(i, 'UIAlpha', 1)]
        w = self.get(i, 'UIWhite')
        if w is not None:
            return [w, w, w, self.get(i, 'UIAlpha', 1)]
        sys_name = self.string(self.get(i, 'UISystemColorName'))
        return {'system': sys_name} if sys_name else {'class': c}

    def image(self, v):
        i = self.deref(v)
        if i is None:
            return None
        return self.string(self.get(i, 'UIResourceName'))

    def floats(self, v):
        """CGRect/CGPoint/CGSize values: data of 0x06-prefixed floats, or a
        string '{{x, y}, {w, h}}'."""
        if isinstance(v, bytes):
            if v[:1] == b'\x06':
                return list(struct.unpack_from('<%df' % ((len(v) - 1) // 4), v, 1))
            if v[:1] == b'\x07':
                return list(struct.unpack_from('<%dd' % ((len(v) - 1) // 8), v, 1))
            s = v.decode('latin1')
        elif isinstance(v, str):
            s = v
        else:
            s = self.string(v)
            if s is None:
                return None
        return [float(x) for x in s.replace('{', ' ').replace('}', ' ').replace(',', ' ').split()]


STATES = {0: 'normal', 1: 'highlighted', 2: 'disabled', 4: 'selected'}


def view(nib, i, ids):
    c = nib.cls(i)
    # A view of a custom class is archived as a UIClassSwapper naming both.
    original = nib.string(nib.get(i, 'UIOriginalClassName'))
    if original:
        c = original
    out = {'id': ids.setdefault(i, i), 'class': c, 'uiClass': c}
    custom = nib.string(nib.get(i, 'UIClassName'))
    if custom:
        out['class'] = custom
    bounds = nib.floats(nib.get(i, 'UIBounds'))
    center = nib.floats(nib.get(i, 'UICenter'))
    if bounds and center:
        w, h = bounds[2], bounds[3]
        out['frame'] = [center[0] - w / 2, center[1] - h / 2, w, h]
    for key, name in (('UIHidden', 'hidden'), ('UIAlpha', 'alpha'), ('UITag', 'tag'),
                      ('UIContentMode', 'contentMode'), ('UIClipsToBounds', 'clipsSubviews'),
                      ('UIOpaque', 'opaque')):
        v = nib.get(i, key)
        if v is not None:
            out[name] = v
    ui = nib.get(i, 'UIUserInteractionDisabled')
    if ui is not None:
        out['userInteraction'] = not ui
    bg = nib.color(nib.get(i, 'UIBackgroundColor'))
    if bg is not None:
        out['backgroundColor'] = bg
    if c == 'UIImageView':
        out['image'] = nib.image(nib.get(i, 'UIImage'))
        hl = nib.image(nib.get(i, 'UIHighlightedImage'))
        if hl:
            out['highlightedImage'] = hl
    elif c == 'UIButton' or nib.get(i, 'UIButtonStatefulContent') is not None:
        states = {}
        sc = nib.deref(nib.get(i, 'UIButtonStatefulContent'))
        if sc is not None:
            ks = nib.all(sc, 'UINibEncoderEmptyKey')
            # NSDictionary: key, value, key, value...
            for kv, vv in zip(ks[0::2], ks[1::2]):
                ki = nib.deref(kv)
                state = nib.get(ki, 'NS.intval') if ki is not None else kv
                ci = nib.deref(vv)
                if ci is None:
                    continue
                st = {}
                for key, name in (('UIImage', 'image'), ('UIBackgroundImage', 'background')):
                    im = nib.image(nib.get(ci, key))
                    if im:
                        st[name] = im
                t = nib.string(nib.get(ci, 'UITitle'))
                if t:
                    st['title'] = t
                tc = nib.color(nib.get(ci, 'UITitleColor'))
                if tc is not None:
                    st['titleColor'] = tc
                states[STATES.get(state, str(state))] = st
        out['states'] = states
        for key, name in (('UIDisabled', 'disabled'), ('UISelected', 'selected'),
                          ('UIAdjustsImageWhenHighlighted', 'adjustsImageWhenHighlighted'),
                          ('UIAdjustsImageWhenDisabled', 'adjustsImageWhenDisabled'),
                          ('UIButtonType', 'buttonType')):
            v = nib.get(i, key)
            if v is not None:
                out[name] = v
    elif c in ('UILabel', 'UITextView') or nib.get(i, 'UIText') is not None:
        out['text'] = nib.string(nib.get(i, 'UIText'))
        fi = nib.deref(nib.get(i, 'UIFont'))
        if fi is not None:
            out['font'] = [nib.string(nib.get(fi, 'UIFontName')), nib.get(fi, 'UIFontPointSize')]
        tc = nib.color(nib.get(i, 'UITextColor'))
        if tc is not None:
            out['textColor'] = tc
        for key, name in (('UITextAlignment', 'textAlignment'), ('UINumberOfLines', 'lines')):
            v = nib.get(i, key)
            if v is not None:
                out[name] = v
    subs = [nib.deref(v) for v in nib.array(nib.get(i, 'UISubviews'))]
    if subs:
        out['subviews'] = [view(nib, s, ids) for s in subs if s is not None]
    return out


def convert(objects):
    nib = Nib(objects)
    root = 0
    top = [nib.deref(v) for v in nib.array(nib.get(root, 'UINibTopLevelObjectsKey'))]
    objs = [nib.deref(v) for v in nib.array(nib.get(root, 'UINibObjectsKey'))]
    owner = None
    names = {}
    # The File's Owner is the placeholder proxy named IBFilesOwner.
    for i in objs + top:
        if i is not None and nib.cls(i) == 'UIProxyObject':
            ident = nib.string(nib.get(i, 'UIProxiedObjectIdentifier'))
            names[i] = 'owner' if ident == 'IBFilesOwner' else ident
    ids = {}
    views = []
    for i in top:
        if i is None or i in names:
            continue
        if nib.get(i, 'UIBounds') is not None or nib.cls(i).startswith('UI') and nib.cls(i).endswith('View'):
            views.append(view(nib, i, ids))
    outlets, actions = [], []
    for v in nib.array(nib.get(root, 'UINibConnectionsKey')):
        ci = nib.deref(v)
        if ci is None:
            continue
        src = nib.deref(nib.get(ci, 'UISource'))
        dst = nib.deref(nib.get(ci, 'UIDestination'))
        label = nib.string(nib.get(ci, 'UILabel'))
        ref = lambda x: names.get(x, ids.get(x, x))
        if nib.cls(ci) == 'UIRuntimeOutletConnection':
            outlets.append({'source': ref(src), 'destination': ref(dst), 'name': label})
        elif nib.cls(ci) == 'UIRuntimeEventConnection':
            actions.append({'source': ref(src), 'destination': ref(dst), 'selector': label,
                            'events': nib.get(ci, 'UIEventMask')})
    return {'views': views, 'outlets': outlets, 'actions': actions}


def main():
    args = sys.argv[1:]
    if args and args[0] == '--raw':
        for n, (c, vals) in enumerate(read_archive(args[1])):
            print(n, c)
            for k, v in vals:
                print('   ', k, repr(v)[:120])
        return
    data = convert(read_archive(args[0]))
    text = json.dumps(data, indent=1)
    if len(args) > 1:
        open(args[1], 'w', encoding='utf-8').write(text + '\n')
    else:
        print(text)


if __name__ == '__main__':
    main()
