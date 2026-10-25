"""ActionScript 1/2 as the decompiler (JPEXS FFDec) writes it: a lexer and a
parser to a small syntax tree, for port/tools/index_scripts.py.

Nodes are tuples, the first item the kind:

  expressions
    ("lit", value)            number, string, bool; ("undef",) undefined/null
    ("var", name)             a name (this, _root, _parent, _global included)
    ("get", obj, name)        obj.name        ("index", obj, key)  obj[key]
    ("call", callee, [args])  ("new", callee, [args])
    ("op", op, a, b)          binary (and/or as && ||)  ("unary", op, a)
    ("cond", c, a, b)         c ? a : b
    ("array", [items])        ("object", [(key, value)])
    ("func", name, [params], [body])
    ("assign", op, target, value)  = += ... (also an expression)
    ("incdec", op, prefix, target) ++ --

  statements
    ("expr", e)  ("vardecl", [(name, init)])  ("if", c, [then], [else])
    ("while", c, [body])  ("dowhile", [body], c)  ("for", init, c, step, [body])
    ("forin", name, declared, obj, [body])  ("switch", e, [(case or None, [body])])
    ("return", e)  ("break",)  ("continue",)  ("block", [body])
    ("function", name, [params], [body])  ("delete", e)  ("with", e, [body])
    ("try", [body], name, [catch], [finally])  ("throw", e)
    ("on", kind, [events], [body])     on(...) / onClipEvent(...)
    ("class", name, extends, [members])  members: ("vardecl", ...) / ("function", ...)

parse(text) -> [statements]; raises ParseError.
"""
import re

KEYWORDS = {"var", "if", "else", "while", "do", "for", "in", "function", "return", "break", "continue", "switch",
            "case", "default", "new", "delete", "typeof", "instanceof", "with", "try", "catch", "finally", "throw",
            "true", "false", "undefined", "null", "and", "or", "not", "class", "extends", "implements", "interface",
            "import", "static", "public", "private", "dynamic", "intrinsic", "void", "on", "onClipEvent", "tellTarget"}

PUNCT = sorted("""=== !== >>>= <<= >>= >>> == != <= >= && || ++ -- += -= *= /= %= &= |= ^= << >>
               + - * / % < > ! ~ & | ^ ? : = . , ; ( ) [ ] { }""".split(), key=len, reverse=True)

TOKEN = re.compile(r"""
    (?P<space>\s+|//[^\n]*|/\*.*?\*/)
  | (?P<num>0[xX][0-9A-Fa-f]+|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)
  | (?P<str>"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')
  | (?P<name>[A-Za-z_$][\w$]*)
  | (?P<punct>""" + "|".join(re.escape(p) for p in PUNCT) + r""")
""", re.S | re.X)

ESCAPES = {"n": "\n", "r": "\r", "t": "\t", "b": "\b", "f": "\f", "\\": "\\", '"': '"', "'": "'"}


class ParseError(Exception):
    pass


def unquote(s):
    body = s[1:-1]
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c == "\\" and i + 1 < len(body):
            e = body[i + 1]
            if e == "u" and i + 5 < len(body):
                out.append(chr(int(body[i + 2:i + 6], 16)))
                i += 6
                continue
            if e == "x" and i + 3 < len(body):
                out.append(chr(int(body[i + 2:i + 4], 16)))
                i += 4
                continue
            out.append(ESCAPES.get(e, e))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def lex(text):
    tokens = []
    pos = 0
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            raise ParseError("bad character %r at %d" % (text[pos], pos))
        pos = m.end()
        kind = m.lastgroup
        if kind == "space":
            continue
        v = m.group(kind)
        if kind == "num":
            v = int(v, 16) if v[:2].lower() == "0x" else (float(v) if re.search(r"[.eE]", v) else int(v))
            tokens.append(("num", v))
        elif kind == "str":
            tokens.append(("str", unquote(v)))
        elif kind == "name":
            tokens.append(("kw" if v in KEYWORDS else "name", v))
        else:
            tokens.append(("p", v))
    tokens.append(("eof", None))
    return tokens


BINARY = [  # lowest first
    {"||", "or"}, {"&&", "and"}, {"|"}, {"^"}, {"&"}, {"==", "!=", "===", "!=="},
    {"<", ">", "<=", ">=", "instanceof", "in"}, {"<<", ">>", ">>>"}, {"+", "-"}, {"*", "/", "%"},
]
ASSIGN = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", ">>>="}


class Parser:
    def __init__(self, text):
        self.t = lex(text)
        self.i = 0

    # ---- tokens
    def peek(self, k=0):
        return self.t[min(self.i + k, len(self.t) - 1)]

    def at(self, *values):
        tok = self.peek()
        return tok[0] in ("p", "kw") and tok[1] in values

    def take(self, *values):
        if self.at(*values):
            self.i += 1
            return True
        return False

    def need(self, value):
        if not self.take(value):
            raise ParseError("expected %r, got %r (token %d)" % (value, self.peek()[1], self.i))

    def name(self):
        tok = self.peek()
        if tok[0] == "name" or (tok[0] == "kw" and tok[1] not in ("var", "function")):
            self.i += 1
            return tok[1]
        raise ParseError("expected a name, got %r" % (tok[1],))

    # ---- statements
    def program(self):
        body = []
        while self.peek()[0] != "eof":
            body.append(self.statement())
        return body

    def block(self):
        if self.take("{"):
            body = []
            while not self.take("}"):
                body.append(self.statement())
            return body
        return [self.statement()]

    def statement(self):
        if self.take(";"):
            return ("block", [])
        if self.at("{"):
            return ("block", self.block())
        if self.take("var"):
            decls = []
            while True:
                n = self.name()
                if self.take(":"):
                    self.name()  # AS2 type
                decls.append((n, self.assignment() if self.take("=") else None))
                if not self.take(","):
                    break
            self.take(";")
            return ("vardecl", decls)
        if self.take("if"):
            self.need("(")
            c = self.expression()
            self.need(")")
            then = self.block()
            other = self.block() if self.take("else") else []
            return ("if", c, then, other)
        if self.take("while"):
            self.need("(")
            c = self.expression()
            self.need(")")
            return ("while", c, self.block())
        if self.take("do"):
            body = self.block()
            self.need("while")
            self.need("(")
            c = self.expression()
            self.need(")")
            self.take(";")
            return ("dowhile", body, c)
        if self.take("for"):
            self.need("(")
            declared = self.take("var")
            if self.peek()[0] in ("name", "kw") and self.peek(1) == ("kw", "in"):
                n = self.name()
                self.need("in")
                obj = self.expression()
                self.need(")")
                return ("forin", n, declared, obj, self.block())
            init = None
            if not self.at(";"):
                if declared:
                    decls = []
                    while True:
                        n = self.name()
                        decls.append((n, self.assignment(no_in=True) if self.take("=") else None))
                        if not self.take(","):
                            break
                    init = ("vardecl", decls)
                else:
                    init = ("expr", self.expression())
            self.need(";")
            c = None if self.at(";") else self.expression()
            self.need(";")
            step = None if self.at(")") else self.expression()
            self.need(")")
            return ("for", init, c, step, self.block())
        if self.take("switch"):
            self.need("(")
            e = self.expression()
            self.need(")")
            self.need("{")
            cases = []
            while not self.take("}"):
                if self.take("default"):
                    test = None
                else:
                    self.need("case")
                    test = self.expression()
                self.need(":")
                body = []
                while not self.at("case", "default", "}"):
                    body.append(self.statement())
                cases.append((test, body))
            return ("switch", e, cases)
        if self.take("return"):
            e = None if self.at(";", "}") else self.expression()
            self.take(";")
            return ("return", e)
        if self.take("break"):
            self.take(";")
            return ("break",)
        if self.take("continue"):
            self.take(";")
            return ("continue",)
        if self.take("throw"):
            e = self.expression()
            self.take(";")
            return ("throw", e)
        if self.take("delete"):
            e = self.expression()
            self.take(";")
            return ("delete", e)
        if self.take("with"):
            self.need("(")
            e = self.expression()
            self.need(")")
            return ("with", e, self.block())
        if self.take("tellTarget"):
            self.need("(")
            e = self.expression()
            self.need(")")
            return ("with", e, self.block())
        if self.take("try"):
            body = self.block()
            n, catch, fin = None, [], []
            if self.take("catch"):
                self.need("(")
                n = self.name()
                if self.take(":"):
                    self.name()
                self.need(")")
                catch = self.block()
            if self.take("finally"):
                fin = self.block()
            return ("try", body, n, catch, fin)
        if self.at("on", "onClipEvent") and self.peek(1) == ("p", "("):
            kind = self.peek()[1]
            self.i += 1
            self.need("(")
            events = []
            while not self.take(")"):
                tok = self.peek()
                self.i += 1
                if tok[0] in ("name", "kw"):
                    events.append(tok[1])
                elif tok[0] == "str":
                    events.append("key:" + tok[1])
                elif tok == ("p", "<"):  # keyPress "<Left>"
                    parts = []
                    while not self.take(">"):
                        parts.append(str(self.peek()[1]))
                        self.i += 1
                    events.append("key:<" + "".join(parts) + ">")
                self.take(",")
            return ("on", kind, events, self.block())
        if self.at("function") and self.peek(1)[0] in ("name", "kw") and self.peek(1)[1] != "(":
            self.i += 1
            n = self.name()
            params, body = self.function_rest()
            return ("function", n, params, body)
        if self.take("class", "interface"):
            n = self.dotted()
            ext = self.dotted() if self.take("extends") else None
            if self.take("implements"):
                self.dotted()
                while self.take(","):
                    self.dotted()
            self.need("{")
            members = []
            while not self.take("}"):
                while self.take("static", "public", "private", "dynamic", "intrinsic"):
                    pass
                members.append(self.statement())
            return ("class", n, ext, members)
        if self.take("import"):
            self.dotted()
            if self.take("."):
                self.need("*")
            self.take(";")
            return ("block", [])
        e = self.expression()
        self.take(";")
        return ("expr", e)

    def dotted(self):
        n = self.name()
        while self.at(".") and self.peek(1)[0] in ("name", "kw"):
            self.i += 1
            n += "." + self.name()
        return n

    def function_rest(self):
        self.need("(")
        params = []
        while not self.take(")"):
            params.append(self.name())
            if self.take(":"):
                self.name()
            self.take(",")
        if self.take(":"):
            self.name()  # return type
        self.need("{")
        body = []
        while not self.take("}"):
            body.append(self.statement())
        return params, body

    # ---- expressions
    def expression(self):
        e = self.assignment()
        while self.take(","):
            e = ("op", ",", e, self.assignment())
        return e

    def assignment(self, no_in=False):
        left = self.conditional(no_in)
        tok = self.peek()
        if tok[0] == "p" and tok[1] in ASSIGN:
            self.i += 1
            return ("assign", tok[1], left, self.assignment(no_in))
        return left

    def conditional(self, no_in):
        c = self.binary(0, no_in)
        if self.take("?"):
            a = self.assignment(no_in)
            self.need(":")
            b = self.assignment(no_in)
            return ("cond", c, a, b)
        return c

    def binary(self, level, no_in):
        if level == len(BINARY):
            return self.unary()
        left = self.binary(level + 1, no_in)
        while True:
            tok = self.peek()
            if tok[0] in ("p", "kw") and tok[1] in BINARY[level] and not (no_in and tok[1] == "in"):
                self.i += 1
                op = {"or": "||", "and": "&&"}.get(tok[1], tok[1])
                left = ("op", op, left, self.binary(level + 1, no_in))
            else:
                return left

    def unary(self):
        tok = self.peek()
        if tok[0] in ("p", "kw") and tok[1] in ("!", "not", "-", "+", "~", "typeof", "void"):
            self.i += 1
            return ("unary", {"not": "!"}.get(tok[1], tok[1]), self.unary())
        if tok[0] == "p" and tok[1] in ("++", "--"):
            self.i += 1
            return ("incdec", tok[1], True, self.unary())
        if tok == ("kw", "delete"):
            self.i += 1
            return ("unary", "delete", self.unary())
        e = self.postfix()
        tok = self.peek()
        if tok[0] == "p" and tok[1] in ("++", "--"):
            self.i += 1
            return ("incdec", tok[1], False, e)
        return e

    def postfix(self):
        if self.take("new"):
            callee = self.primary()
            while True:
                if self.take("."):
                    callee = ("get", callee, self.name())
                elif self.take("["):
                    callee = ("index", callee, self.expression())
                    self.need("]")
                else:
                    break
            args = self.arguments() if self.at("(") else []
            e = ("new", callee, args)
        else:
            e = self.primary()
        while True:
            if self.take("."):
                e = ("get", e, self.name())
            elif self.take("["):
                e = ("index", e, self.expression())
                self.need("]")
            elif self.at("("):
                e = ("call", e, self.arguments())
            else:
                return e

    def arguments(self):
        self.need("(")
        args = []
        while not self.take(")"):
            args.append(self.assignment())
            self.take(",")
        return args

    def primary(self):
        tok = self.peek()
        self.i += 1
        kind, v = tok
        if kind == "num" or kind == "str":
            return ("lit", v)
        if kind == "kw":
            if v in ("true", "false"):
                return ("lit", v == "true")
            if v in ("undefined", "null"):
                return ("undef",)
            if v == "function":
                n = self.name() if self.peek()[0] == "name" else None
                params, body = self.function_rest()
                return ("func", n, params, body)
        if kind == "name" or (kind == "kw" and v not in ("var",)):
            return ("var", v)
        if tok == ("p", "("):
            e = self.expression()
            self.need(")")
            return e
        if tok == ("p", "["):
            items = []
            while not self.take("]"):
                items.append(self.assignment())
                self.take(",")
            return ("array", items)
        if tok == ("p", "{"):
            members = []
            while not self.take("}"):
                k = self.peek()
                self.i += 1
                self.need(":")
                members.append((str(k[1]), self.assignment()))
                self.take(",")
            return ("object", members)
        raise ParseError("unexpected %r (token %d)" % (v, self.i - 1))


def parse(text):
    return Parser(text).program()


# ------------------------------------------------------------- printing

def source(node):
    """A node back as compact ActionScript (for reports and comparing
    functions between games)."""
    k = node[0]
    if k == "lit":
        v = node[1]
        if isinstance(v, bool):
            return "true" if v else "false"
        if isinstance(v, str):
            return '"' + v.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
        return repr(v)
    if k == "undef":
        return "undefined"
    if k == "var":
        return node[1]
    if k == "get":
        return source(node[1]) + "." + node[2]
    if k == "index":
        return source(node[1]) + "[" + source(node[2]) + "]"
    if k in ("call", "new"):
        return ("new " if k == "new" else "") + source(node[1]) + "(" + ",".join(source(a) for a in node[2]) + ")"
    if k == "op":
        return "(" + source(node[2]) + node[1] + source(node[3]) + ")"
    if k == "unary":
        return node[1] + (" " if node[1].isalpha() else "") + source(node[2])
    if k == "cond":
        return "(" + source(node[1]) + "?" + source(node[2]) + ":" + source(node[3]) + ")"
    if k == "array":
        return "[" + ",".join(source(a) for a in node[1]) + "]"
    if k == "object":
        return "{" + ",".join(key + ":" + source(v) for key, v in node[1]) + "}"
    if k == "func":
        return "function " + (node[1] or "") + "(" + ",".join(node[2]) + "){" + body_source(node[3]) + "}"
    if k == "assign":
        return source(node[2]) + node[1] + source(node[3])
    if k == "incdec":
        return node[1] + source(node[3]) if node[2] else source(node[3]) + node[1]
    # statements
    if k == "expr":
        return source(node[1]) + ";"
    if k == "vardecl":
        return "var " + ",".join(n + ("=" + source(e) if e else "") for n, e in node[1]) + ";"
    if k == "if":
        s = "if(" + source(node[1]) + "){" + body_source(node[2]) + "}"
        return s + ("else{" + body_source(node[3]) + "}" if node[3] else "")
    if k == "while":
        return "while(" + source(node[1]) + "){" + body_source(node[2]) + "}"
    if k == "dowhile":
        return "do{" + body_source(node[1]) + "}while(" + source(node[2]) + ");"
    if k == "for":
        return "for(" + (source(node[1]) if node[1] else ";") + (source(node[2]) if node[2] else "") + ";" + \
               (source(node[3]) if node[3] else "") + "){" + body_source(node[4]) + "}"
    if k == "forin":
        return "for(" + ("var " if node[2] else "") + node[1] + " in " + source(node[3]) + "){" + \
               body_source(node[4]) + "}"
    if k == "switch":
        return "switch(" + source(node[1]) + "){" + "".join(
            ("case " + source(t) if t else "default") + ":" + body_source(b) for t, b in node[2]) + "}"
    if k == "return":
        return "return" + (" " + source(node[1]) if node[1] else "") + ";"
    if k in ("break", "continue"):
        return k + ";"
    if k == "block":
        return body_source(node[1])
    if k == "function":
        return "function " + node[1] + "(" + ",".join(node[2]) + "){" + body_source(node[3]) + "}"
    if k == "delete":
        return "delete " + source(node[1]) + ";"
    if k == "with":
        return "with(" + source(node[1]) + "){" + body_source(node[2]) + "}"
    if k == "try":
        return "try{" + body_source(node[1]) + "}" + ("catch(" + node[2] + "){" + body_source(node[3]) + "}"
                                                         if node[2] else "") + \
               ("finally{" + body_source(node[4]) + "}" if node[4] else "")
    if k == "throw":
        return "throw " + source(node[1]) + ";"
    if k == "on":
        return node[1] + "(" + ",".join(node[2]) + "){" + body_source(node[3]) + "}"
    if k == "class":
        return "class " + node[1] + "{" + body_source(node[3]) + "}"
    raise ValueError(k)


def body_source(body):
    return "".join(source(s) for s in body)
