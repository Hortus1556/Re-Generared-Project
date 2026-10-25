# Bleed Engine

Bleed Engine (the `BleedEngine` library: `port/src/core`, `platform`, `engine`, `flash`, and the shared surgeon game in `port/src/surgeon`) is the one engine every Amateur Surgeon runs on, each from its content:
- Christmas and AS1 are rebuilt from their Flash originals;
- AS2 is a hybrid of its IPA and its Flash version;
- AS3 comes from its IPA.

Porting a game should mean extracting and indexing its content, plus native modules only where the original's scripts are real game logic. The plan and where each step stands: [plan.md](plan.md). The rules every port follows: [rules.md](rules.md).

This page describes what exists so far:
- the engine's **splash**, shown before every game;
- the **loop and the display**: logic passes on one thread, the display on its own, textures uploaded on a third; art in shared pages, drawn in batches;
- the **game package**, the folder a game's content lives in;
- the **script index**, what each of a Flash game's scripts does;
- the **stage**, which runs a game by its own translated scripts, with native modules where the scripts are real logic;
- the **UIKit layer**, the iOS games' screens from their nibs;
- the **cocos layer**, cocos2d-x 2.1.1's scene graph for the games built on it.

Christmas runs on the stage. AS2 runs on the platform, the UIKit layer and `VectorArt` (its Flash pieces), its game the iOS classes mirrored ([as2.md](as2.md)).

## The splash: "Powered by Bleed Engine"

Every game opens on the engine's own splash before its first screen (`platform/Splash.cpp`, called by the shared main loop, `runGame` in `platform/GameMain.h`): the wordmark, "POWERED BY" over a blood-red "BLEED ENGINE" with drips running from its foot, fades in on black (0.6 s), holds (1.8 s) and fades out (0.6 s); a click or Esc goes straight to fading out. The wordmark is vector art in the engine's own angular stroke lettering (no font file): `port/tools/make_bleed_splash.py` writes it as an SVG string into `platform/BleedSplashArt.h` (rerun the tool to change it; `--svg <file>` also writes a preview), and the splash rasterises it with lunasvg at the window's resolution, again whenever that changes, at 70% of the game screen's width (at most half its height). It is drawn through the recorded drawing like everything else but is not one of the game's frames, and while debug automation drives a run (`automationActive()`: `AS_CONTROL`, `AS_SHOT`, `AS_CLICKS`, `AS_EVENT`, `AS_HIDDEN`, …) it is left out, so tests and their frame numbers start at the game's own first screen.

## The loop and the display: three threads

A game runs in logic passes of a fixed length (`GameInfo::passSeconds`: 40 ms for the Flash games, 25 a second), on the main loop (`runGame`, `platform/GameMain.h`). Each pass updates the game and records one frame of draws (`platform/DrawList`); nothing is drawn to the window while it runs. Three threads share the work, each with its own GL context, the textures shared between them:
- **The game's thread** (the main one): the window's events, the logic, the recording, the vector art's triangles (`flash::MeshCache`, below) and what art is still rasterised (`flash::Raster`, its workers making it). It makes no GL call while it records (below) and touches no texture's texels itself.
- **The display thread** (`presentLoop`, `Platform.cpp`): shows the recorded frames, once per display refresh (vsync; at most 240 a second without it), at the window's native resolution. Each frame is shown on its own clock: the frame a pass records is due a short lead after its pass was (`platform::setFrameDue`): as long as the longest pass of the last second took to compute, 3 ms more, at least 4 ms and at most a pass, moving a millisecond a pass at most (`runGame`). So the display never waits on a pass, and what the player does shows a few milliseconds after the pass that saw it (with a whole pass of lead, 40 ms more, the input lag was plain). Between its due time and the next frame's, it is eased in from the frame before (smooth motion, [port.md](port.md#port-architecture)). A frame not ready by then (its pass late, or its textures not in) holds the one before at its end; nothing else stops the display. Before, the display waited on every pass: at 240 Hz, any pass over 4 ms (half of them in AS1's intro) cost a display frame, and a transition's passes (art made and uploaded at once, 40–350 ms) froze it outright.
- **The texture thread** (`textureWorkLoop`, `DrawList.cpp`): the texels of every texture the game's thread makes (`platform::setTexels`, `setTexelsAt`: the raster cache's art) are uploaded here as soon as they are queued, and the textures let go are deleted here, in order. A frame is shown only once all its textures are in, never before (drawn before, a new texture's name was used in two contexts before either had made it, and each made its own: the first page of art stayed empty for the game's thread). It also makes textures ahead, named and made in its context, which `platform::newTexture` hands out, so the game's thread names a texture with no GL call. The display thread and the texture thread take turns (`DrawTurn`): uploads go in strips of 512 KB between the display's draws. (Uploading from the game's thread while another context drew made each upload six times slower, and a big texture's storage, made by the driver, took up to 15 ms.)

**No GL call while recording.** Game code's matrix calls (`glMatrixMode`, `glOrtho`, `glTranslatef`, `glPushMatrix`, `glGetFloatv` of a matrix...) go to a matrix stack the engine keeps on the CPU (`platform/GLMatrices.cpp`, the names redirected in `GL.h`), which recording reads; the replay sets GL's own (`DrawList.cpp` defines `BLEED_GL_MATRICES` before including). With Android's GL, which lets one context's calls run at a time, every call on the game's thread waited for the display thread's: recording a frame took up to 180 ms.

**Vector art on the GPU.** A Flash game's art is vectors, and the GPU draws it as the Flash player did: each shape, morph step and static text is cut into triangles once (`flash/VectorMesh`: fills' outlines by libtess2, even-odd, curves flattened to within 0.2 pixels at the size they are drawn; strokes from their segments, joins and caps, a flattened curve's points sharing one pair of corners), cached per size step (√2 apart: only flattening and hairlines' width depend on it) and morph step (64), and drawn at any size, place, turn and colour transform with no texture. Solid colours are per vertex; gradients are small textures (a 256-texel ramp, a 128 × 128 radial, focal too) and bitmap fills their image, the triangles mapping them by the fill's matrix. Edges are smoothed by the window's 4× multisampling (debug `AS_MSAA=<n>`; debug shots are drawn 4× larger and averaged down). The replay transforms the vertices on the CPU into the same batches as the quads (`drawVector`, `drawVectorAt`), indexed, skipping meshes off the game screen; smooth motion moves a mesh by its place (`ArtPiece`) exactly as a quad of art. What is still rasterised to textures (`flash::Raster`): pieces under filters (blur, glow, shadows), text fields, a gradient under an added colour (its texels baked). Against the rasteriser (2026-10-24, AS1's mission 1 at 1800×1380): texture memory 86 → 18 MB, atlas pages 142 → 12 MB, the process's private memory 690 → 410 MB, the meshes 2.3 MB, about 40,000 vertices a display frame (0.4 ms to draw), and no art made or uploaded when something new shows (the holds that were left came from that); its screenshots differ by under 1/255 on average (edge smoothing). Debug `AS_RASTER_ART=1` rasterises everything as before, for comparison.

**Shared pages and batched draws.** Art up to 256 texels a side is packed in shared texture pages (`platform/TextureAtlas`), each slot with a ring of one transparent texel so drawing it never filters in its neighbours; art over 128 texels high (the big art's tiles, its edge tiles) goes in pages of 4 × 4 cells of 258, any cell let go taken again as it is, and the rest in 1040-texel pages cut into shelves a power of two high. New art goes in the fullest page that holds it, a slot is let go as a texture is (once no frame can still draw it), a page with nothing left goes (one empty one kept of each kind), and the raster cache counts the pages' empty room against its budget and drops art from the emptiest pages first. The replay then sends consecutive quads with the same texture and blending as one draw, their colours per vertex (`flushBatch`); a mask, a grid effect, the pointer's shift or a mesh sends what was gathered first. A frame of AS1's operation, some 575 draws, goes to the GPU as about 140 (2026-10-24); a phone's GL, OpenGL ES 1.1 emulated, pays for each draw and its state. With the vector art drawn as meshes the pages hold only the rasterised rest (12 MB in AS1's operation, 2026-10-24); rasterising everything they held 125–200 MB at 1800×1380.

The games that make their own textures (AS2, AS3: `Resource`, `CCTexture2D`, the UIKit layer, movies) still upload them on the game's thread; each commit finishes that work (`glFinish`) before the display thread may draw it.

Loops outside the main one (the splash, `AS_FAST` runs) show what they drew with `platform::show`, which puts the latest frame on show at once and waits until it is shown. During a live resize (Windows holding the event loop) the display thread goes on showing the last frame at the window's new size.

**On Android** an EGL surface is current in one context at a time: the game's and the texture thread's contexts are current with no surface (`EGL_KHR_surfaceless_context`, made current by EGL itself: SDL's Android backend, given no window, makes no context current at all and says it did), and only the display thread's has the window's. When the app goes to the background (SDL's `SDL_EVENT_WILL_ENTER_BACKGROUND`, on the game's thread, before SDL puts the game's context aside and the surface goes), the display thread lets go of the surface and waits; back (`SDL_EVENT_DID_ENTER_FOREGROUND`, SDL having made the game's context current with the new surface), the game's context goes back to none and the display thread takes the new surface. Verified on the emulator: home and back, twice, the game drawn and played on. A driver without surfaceless contexts gets the game's thread showing the frames itself, between passes, as debug mode's `AS_ONE_THREAD=1` does on the desktop, for comparison.

**Checking the display's pace.** With `AS_DISPLAY_LOG=<file>` (debug mode) the display thread logs each display frame: when it began, the frame shown, how far eased in, the time to draw it and to swap, the draws of the two frames and the GL draws sent. `port/tools/display_pace.py <file> [--pass 0.04] [--from s] [--to s]` reads it and reports holds (the display standing still: a frame late), jumps (catching up) and slow display frames (one that missed a refresh); an eighth column has the vertices of the batched draws. The logic's own pace is in `AS_PROFILE=1`'s pass lines (update, draw, art made and uploaded at once, upload hand-over, each with its time). Measured on AS1's and Christmas's whole way through (title, level select, story, operation, win and its scenes, at 1800×1380 and 240 Hz, 2026-10-24): no holds, no jumps, no pass over its length. On Android the same lines go to files in the save folder ([port.md](port.md#building-and-running)).

## The package: `port/games/<game>/`

One folder per game. It is the game's `assets` folder: the build points the exe at it, and the dist targets copy it as `assets/`.

| Path | Holds | Written by |
|---|---|---|
| `library.json` | stage size, frame rate, background, Flash version; the linkage names (`exports`: name → character id; from `ExportAssets`, or an ActionScript 3 movie's `SymbolClass` class names); the ids of every shape, morph, sprite and font | `extract_swf` |
| `vector/shapes/<id>.svg` | each shape, as SVG (dialect below) | `extract_swf` |
| `vector/morphs/<id>_start.svg`, `<id>_end.svg` | each morph shape's two ends, same path order | `extract_swf` |
| `vector/symbols/<id>.json` | each sprite's timeline (`0.json`: the main one), in order: `frameCount`; `labels` (lower case; a label on several frames is its first, where Flash's goto goes: AS1's and Christmas's extraction wound has `worm` on frame 10, the worm, and again on 17, a copy of the crystal's); `frames` (per frame, its `place`/`remove` ops); `scripts` (the frames that have a script); `actions` (per frame, its plain frame actions: `stop`, `play`, `goto`, `gotoLabel`, `gotoRandom`, `next`, `prev`) | `extract_swf` |
| `vector/buttons.json` | each button's records: states, character, depth, matrix, colour transform | `extract_swf` |
| `vector/texts.json`, `vector/edittexts.json` | static texts (glyph records) and edit fields (font, size, colour, layout, variable, initial text) | `extract_swf` |
| `vector/fonts/<id>.json` | name, style, metrics, character codes and advances; each glyph an SVG (same dialect) | `extract_swf` |
| `audio/<name>.mp3` / `.wav`, `audio/index.json` | each sound, named by its linkage (else `sound_<id>`); see the audio notes below. `extract_swf --sound <export>=<file>` puts another sound in an exported sound's place: an MP3, or a 16-bit PCM WAV kept as the movie's raw sound (`format` 3, written as `.wav`) (AS1's restored music: [as1.md](as1.md#restored-music)), or adds it as a new sound exported by that name when the movie has none (AS1's female screams) | `extract_swf` |
| `scripts/` | the decompiled ActionScript (JPEXS FFDec's export: `frame_N/` the main timeline, `DefineSprite_<id>/frame_N/`, `DefineButton2_<id>/`, `__Packages/`), and `index.json` (below) | FFDec, `index_scripts.py` |
| `data/` | the game's own data, kept in the source tree: Christmas's `dialogue.xml`, `modules.json` (below), `gamedata.json` (missions, injury types, lines; generated by `port/tools/flash_data.py` from the index) | by hand / generated |

**Timeline ops in `vector/symbols/<id>.json`:**
- A `place` op carries the depth and only what the original tag set:
  - `move`, `char`;
  - `matrix` `[a,b,c,d,tx,ty]` (twips);
  - `color` `[4 multipliers, 4 adds]`;
  - `ratio`, `name`, `clip` (mask depth), `blend`, `filters`.
- A `remove` op carries the depth.

**Audio:**
- MP3 is kept exactly as stored, so the gapless data stays.
- Raw PCM becomes a `.wav`.
- `index.json` gives each sound's id, file, format, rate, sample size, channels, the samples played (`samples`), and the decoder delay to skip (`seek`).

### The SVG dialect (`flash/SvgShape.*`)

Each file is valid SVG: a browser or an editor shows the shape. It is also a fixed dialect that reads back to exactly the same shape.

| Element | Content |
|---|---|
| `<svg>` | `viewBox` in pixels; `data-bounds` is the shape's bounds in twips |
| `<defs>` | One gradient per gradient style:<br>• ids `L<layer>F<i>` (fills) and `L<layer>S<i>` (strokes);<br>• `gradientUnits="userSpaceOnUse"` over Flash's gradient square (−819.2..819.2 px);<br>• `gradientTransform`, `spreadMethod`, stops;<br>• focal radials carry `fx` and `data-focal`;<br>• `data-tx`/`data-ty` give the translation exactly in twips. |
| `<g data-layer="n">` | One per style layer (Flash shapes can change style sets mid-shape) |
| `<path data-fill="i">` | Even-odd fill paths in drawing order |
| `<path data-line="i">` | Stroke paths in drawing order, with their caps, join, miter limit and `data-width` (twips) |

- Coordinates are pixels with two decimals, which is exactly Flash's twips (1/20 px). Paths use `M`, `L` and `Q` only.
- A style no path uses is kept as an empty path, so style indices stay stable.
- Bitmap fills (unused by the games) are marked `data-type="bitmap"`.

## Extracting: `extract_swf`

`extract_swf <movie.swf> <package dir>` (`port/src/tools/extract_swf.cpp`, built with the games):
1. It parses the SWF (`flash/Swf.*`) and writes the package (`flash/Package.*`), replacing `library.json`, `vector/` and `audio/`. It leaves `data/` alone.
2. It then checks the round trip: it reads the package back, writes it again beside it, and requires every file to be byte-identical.

The build runs it for Christmas (`xmas_package` target in `port/CMakeLists.txt`) from `re/flash/xmas/.../Amateur Surgeon Christmas.swf`. The SWF itself is never shipped.

For Christmas, the package holds:
- 2785 shapes, 41 morphs, 2979 symbols;
- 101 buttons, 365 texts, 25 fonts;
- 58 sounds.

It reads back in about 0.35 s, and the game shows its first frame about 0.6 s after start.

## Loading

`VectorArt::library("library.json")` (or `"<folder>/library.json"`) reads a package through `assets::read` into the same in-memory model the SWF parser fills (`flash::Swf`). Its shapes are read when first drawn (`Swf::shape`) and let go after five minutes unused (`Swf::unloadShapes`, from `Raster::endFrame` while no art is being made): most of a game's art is not on screen at any one time (AS1's heap went from 88 to 55 MB). Everything above it (rasteriser, timelines, sounds, screens) is unchanged. `VectorArt::library("<name>.swf")` still reads a SWF directly, for tools.

`VectorArt::preload(name)` starts reading a package on a worker thread (only file reads and parsing), for `library(name)` to take when it is first asked for, waiting only if it is not read yet: a big package (AS2's `flash`) takes a third of a second. Art made in the background (`Raster`) is uploaded a strip of rows (1 MB) at a time within the frame's upload budget (`Raster::pump`), and finished at once if it is drawn first: a big texture took some 30 ms in one go. `VectorArt::drawText` can widen the field's box (`boxWidth`, its text aligned as the field's), for a longer text in a field's style (a name written in where the art has no page for it).

**Pictures.** `platform::decodeImage` (`platform/ImageFile.*`) turns a picture file into straight-alpha RGBA for every game's textures (AS2's `Texture2D`, the UIKit layer's images):
- PNG and JPEG go through stb_image.
- PVR textures (versions 2 and 3: PVRTC at 2 or 4 bits a pixel, and the uncompressed formats cocos2d-x read) are decoded on the CPU, desktop GL having no PVRTC. The decoder is `third_party/texture2ddecoder`.
- A CCZ file (cocos2d's zlib wrapper: AS3's `.pvr.ccz` sheets) is inflated first.

A PVR stored upside down is turned right way up, and one stored premultiplied is divided out. The sheets stay in the package as the IPA had them: AS3's PVRTC sheets are about a quarter of the size of the same pixels as PNGs.

**Sounds.** Every sound is decoded whole when loaded (`engine/MediaPlayer`), in this order:
1. Apple's CAF files natively (`platform::decodeCaf`, `platform/AudioFile.*`). These are AS2's linear PCM and AS3's IMA4 ADPCM, trimmed to their packet tables' valid frames.
2. MP3 and WAV through miniaudio.
3. AAC through the platform (Media Foundation).

IMA4's packet headers keep the predictor to only 9 bits. As QuickTime's decoder does, the running state is kept while a header agrees with it, so there is no jump of up to 127 every 64 samples. Restarting each packet from its header, as a first conversion did, put a buzz of 689 Hz under every sound. The output is sample for sample ffmpeg's `adpcm_ima_qt`.

`assets::path` resolves a game's files as follows:
- a name with a folder is relative to the package (a converted file under its new name: AS3's `4.0x/x.plist` is `4.0x/x.json`);
- a bare name with a known bundle type goes to that type's folder (the IPA layout of `extract_ipa.py`);
- any other bare name (`library.json`) is at the package root.

## Verifying a change to the package format

1. Run `extract_swf`. It fails unless the round trip is exact.
2. Take before/after screenshots with the same `AS_CONTROL` input (boot frames, menus, operations) and with `AS_VECTOR_SYMBOL` sweeps; they must be pixel-identical.
3. Run the play-tests in `port/tools/flash_playtest.py`.

`AS_SEED=<n>` makes the operation's `random()` repeatable for such comparisons.

## The script index

`port/tools/index_scripts.py <game> [--swf <movie.swf>] [--report] [--diff <other game>]` reads every script of the package's `scripts/` (exported from `--swf` with FFDec when missing), parses it into a syntax tree (`port/tools/as2.py`: ActionScript 1/2 as FFDec writes it; every Christmas script parses and prints back to the same tree), and sorts each statement:

| Kind | What | Kept as |
|---|---|---|
| **translated** | statements the runner can do:<br>• timeline control on any target (`stop`, `play`, `gotoAndStop/Play`, `nextFrame`, `prevFrame`);<br>• sets of variables and properties (`=`, `+=`, `++`; literal arrays and objects included: the missions, injury types and lines are data this way);<br>• `if`, loops (`while`, `do..while`, `for`, `for..in`, with `break`, `continue`), `return`;<br>• `delete`;<br>• calls of functions (the scripts' own, the engine's classes, a module's), `new` of Flash's built-in classes;<br>• functions and event handlers whose body is all of these (`function ftb(t) { _root.fTarg = t; ftbclip.gotoAndPlay(2); }`, `this.onEnterFrame = function () {...}`) | actions in `scripts/index.json` |
| **native** | what the runner cannot do (a `switch`, `with`, `try`, a computed class...), or what a module claims | listed with what it is (`function CheckTongs`, `while: ...`) and the module that implements it |
| **unported** | native with no module | the report's checklist |

**The `index.json` format:**
- Each script is one entry (`kind` frame, button or clip; `owner` the sprite or button id, 0 the main timeline; `frame`; `events` for buttons and clip events; `files`).
- It has `actions` in order. A native statement leaves `{"do": "native", "ref": n}` at its place, pointing into `native`.
- Expressions are small arrays:
  - `["lit", v]`, `["undef"]`, `["array", [...]]`, `["object", [[k, v]...]]`;
  - `["this"]`, and `["scope"]` (the timeline a bare name belongs to: in a function, the one it was defined on, not `this`);
  - `["var", name]`, `["get", obj, name]`, `["index", obj, key]`;
  - `["call", fn, [args]]`, `["new", class, [args]]`;
  - `["op", op, a, b]`, `["unary", op, a]`, `["cond", c, a, b]`.
- A script's functions come first in its actions (`{"do": "function", "name", "params", "body"}`), as ActionScript defines them before the script runs.
- `functions` lists every function a script defines, with its source, for comparing games.

A script recognised as one of Mediatonic's recurring shapes is noted with its parameters (`pattern`): `soundClip` is a clip holding one sound (`playme`/`stopme`, its linkage, volume, and whether it is music). It still runs as translated.

**Modules: `data/modules.json`.**
- It maps native code to the C++ that implements it.
- Each module has an `impl` (the files) and what it takes, tried in this order:
  1. `matches` (regexes over a statement's source: those statements become native and go to this module; Christmas's `removed` module takes the online high-score calls and the context menu this way);
  2. `patterns`;
  3. `functions` by name;
  4. `buttons`;
  5. `frames` (`"sprite:frame"`);
  6. whole `sprites`.

For Christmas:
- 1043 scripts; 1032 are wholly translated.
- 20 native statements, all assigned to a module (0 unported): `removed` (the online high scores and the context menu). The operation runs by its own scripts (until 2026-10-23 its statements were assigned to a native `operation` module).

**`--report`** prints:
- the counts;
- the `_root` variables the translated scripts use;
- the screens (the main timeline's labels);
- the functions called, with how many scripts call each;
- the sounds named;
- the native statements per module, and each unported one.

For a new game, this report is the checklist. **`--diff <other game>`** compares the functions both games define, by source (same / changed / only here), so code a ported game already has can be reused where the scripts match.

## Running the scripts

`src/surgeon/Script.*` runs the index's translated actions:
- **Values.** `script::Value` holds ActionScript 2 values: undefined, null, booleans, numbers, strings, objects and Arrays, and movie clips. Conversions and loose `==` follow AS2. Comparisons follow AVM1 (SWF 7 and later): `undefined` is NaN in arithmetic, `<` and `>` with NaN are undefined, and `a <= b` is `!(a > b)`, so true with NaN (a hit piece's `health--` from undefined is NaN, and `health <= 0` counts it done, as AS1's and Christmas's cream relies on).
- **Linkage names** are found whatever their case, as Flash found them (`attachMovie("evilTapeworm")` for the export `evilTapeWorm`; `VectorArt::symbol`).
- **Clips.** `script::Clip` is any clip the scripts can reach: its variables, its properties (`_x`, `_visible`, `_currentframe`, `_xmouse`...), its child clips by name, and its timeline. A reference to a clip since removed (`removeMovieClip`, its parent removed, or its timeline no longer placing it) reads as undefined, as AVM1 resolved one: `item != undefined` is false, its variables are gone, and calls on it do nothing (AS1's vacuum skips the drops it already took so).
- **Runner.** `script::Runner` evaluates expressions and runs actions. `runFrame` runs a clip's frame script, `runButton` a button's handler, and `runEnterFrames` the `onEnterFrame` handlers scripts set. A handler set on a plain object (`mysound.onSoundComplete = function () { amPlaying = false; }`) is a function kept on that object, its names looked up in the timeline that set it; the engine's classes call it (a Sound's when it ends). Dropped, the scripts' sound manager (`playme`, which plays only while `amPlaying` is false) played each sound once a game.
- **Calls.** A call reaches, in order:
  1. the clip's own methods (`removeMovieClip`, `swapDepths`, `attachMovie`, `createEmptyMovieClip`, `hitTest`, `getBounds`...), a plain `attachMovie(...)` reaching those of the timeline it runs on;
  2. a native function a module registered with `define(name, fn)` (Christmas's `ftb`, `playSound`...);
  3. the language's built-ins (`random`, `int`, `Math.*`, string and Array methods).

  Unanswered calls are traced once each (`AS_TRACE`).

  **Debug traces.** With `AS_TRACE` (or `-d`), `debug.log` also gets every hitch: a logic pass over 80 ms (`slow pass`, with what took the time: update, drawing, art made or uploaded at once) and a display frame presented over 50 ms after the last (`slow display frame`, from the display thread, with its replay and swap time). With `AS_TRACE` set (not `-d` alone: its walk of the heaps holds every other thread's allocations for a few milliseconds, a hitch every ten seconds in a `-d` play session) the stage also writes a memory census every 250 passes (`census pass ...`): its heaps (`platform::heapUse`), live engine objects, scene handles, the raster cache's textures, bytes and largest one, the meshes cached and their bytes, the art waiting to be made or uploaded, texture storage ever allocated, art made a frame, the morph detail step, the four arts with the most texture bytes, the textures drawn in the last frames by art, and the address space (`platform::memoryMap`: private memory in heaps and outside them, the biggest blocks with their protection — 404 is the GL driver's write-combined memory). It found AS1's poison swarm texture churn ([christmas.md](christmas.md), rasterising).

  **The wound log** (`surgeon/ClipWatch`, with `-d`): the stage logs every node a script attached whose name is an injury linkage of `data/gamedata.json` and then its depth (`underSurfaceExtraction1002`), or a name prefix in `AS_WATCH` (comma separated) instead. A line goes out when one first shows, whenever it changes and when it goes. Each line holds the pass, the mouse, its path, frame, shown or hidden, its place in its parent and on the stage, opacity, its drawn bounds on the stage, and its inner clips' frames (with those of the clips in it made nodes, by their paths): `watch pass 412 mouse (301,188) _root.game.clip1.underSurfaceExtraction1002: frame 2 shown at (300.0,200.0) stage (299.0,201.0) a 100 drawn (257,108)-(403,258); inner holder=3 holder.#1814=1 holder.glass=1 holder.hit=1`. An inner clip that is hidden is marked `(hidden)` (a tool's points done: a cream line's rubbed points), and unnamed clips two levels in (the pieces of those points) are left out. A wound drawn somewhere it should not be shows whether its place, its art or an inner clip moved it.

  A plain call's `this` (a function called by name, `StartTongs()`) is the timeline the call is made on, as AVM1's target clip, not the caller's own `this`: a Mouse listener's `onMouseDown` calling its timeline's functions, which read `this["injury" + bodyLevel + "Array"]`.
- **Markers.** A frame whose plain `stop`/`play`/`goto` on the clip itself the art already plays (Swf's `FrameAction` markers) has those skipped, so they never run twice.
- **ActionScript 3 movies** keep their frame scripts in their classes, not in the art, so their timelines play on through a `stop()`. A game adds the stops it needs from the scripts (`VectorArt::addStop(sprite, frame)`: a stop marker on that frame, as an AS2 movie's own), e.g. AS2's dying jogger stopping on its last frame; a clip a script keeps on one frame is held there with `setFrameOverride`.

`src/surgeon/Scene.*` puts the runner on a tree of `ArtNode`s:
- **`SceneClip`** is a node's own clip, or an inner clip of its symbol (a part, by instance path). Its variables live as long as that Flash instance does: a part placed anew is a new clip.
- **An inner clip made a node** (`ArtNode::promote`, `Scene::promote`). What only a node can do (`attachMovie` or `createEmptyMovieClip` on it, `swapDepths`, `_rotation`, `_xscale`, `_yscale` or `_alpha` set) makes the named clip a child node of its own at its timeline depth (the symbol's depth - 16384, as Flash numbers them, below what is attached). Its playhead, its parts' playheads and the looks set on them go with it, and every handle on it and on clips inside it follows (`rebindUnder`), with its variables and the frames its scripts ran, so nothing runs again. The parent's art leaves it out (hidden) and draws its own pieces in slices around it (`drawSlice`). The timeline keeps placing it (matrix, colour) until a script moves it (`scripted`) or swaps its depth, and removes it when it no longer places that instance. As in Flash, `removeMovieClip` leaves a clip the timeline placed unless its depth was swapped. Paths through it (`a.b` where `a` is now a node) are resolved by `ArtNode::through`. AS1's operation needs it: `this.attachMovie(...)` on the operation clip, a part of the main timeline.
- **`Scene::update(root)`** runs, each pass, the script of every frame a scripted clip has entered, at any depth (walked with `VectorArt::clips`, only into sprites that hold scripted clips), then `onEnterFrame`. A scripted inner clip no longer on the stage is let go then (placed again, it is a new instance whose frame scripts run anew); an unscripted one's handle, with the variables scripts set on it (a hit piece's `health`), stays while its node lives.
- **`Scene::enter(node)`** does it at once for one node: Flash runs a clip's frame script before the code that moved the clip goes on.
- **`Scene::bind`** shows a native module's state to the scripts as a clip variable. **`VarClip`** is a clip with no art (variables, bound values, named children): the `_root` of a native module's own scene (Christmas's operation), a module's stand-in clip, or the `this` of a button script run for a module.

The runner also handles:
- functions the scripts defined (`invoke`, `callMethod` for the game's code calling into them), `return`;
- loops (capped at 100 000 rounds: a script waiting on something that never comes);
- engine classes' objects (`Host`: their properties and methods);
- the string and Array methods the games use (`split`, `substr`, `splice`, `slice`, `push`, `join`...).

`Scene::update` goes over the scene again while scripts ran in the pass (up to four times). A script that moves a timeline places new clips at once, and Flash runs their frame scripts right after it, before anything plays on: a story scene's holder sets its `over = false` on its first frame, which its buttons test.

**An attached clip's frame 1.** A clip a script attaches runs its frame 1 script after that script, as Flash queued it, even when the script sent it straight on to another frame (`Scene::enter`): AS1's blood particle picks its frame there by patient (sparks, frame 9, on the car and the robot, overriding the blood frame the operation chose), and its extraction wounds set the cut they leave (`cutFrame`) there (the blockage's own frame sets none: without frame 1 its hole closed as a plain cut). A clip attached by a frame handler (`onEnterFrame`, run after the scene's walk) is entered right after the handlers (`Scene::enterNew`, only the nodes new since the walk), so its first frames run before it is drawn: the car's sparks showed as blood for one frame.

**`_alpha` replaces the timeline's opacity.** A script setting a clip's `_alpha` sets the alpha its timeline's colour placement gave it, as Flash's colour transform does (`ArtNode::alphaSet`, `opacity()`): AS1's brain cut line, placed fully transparent, shows when the skull's script sets `_alpha = 100`.

**Masks are found by name.** A clip used as a mask (a placement with a clip depth) is walked by the visitors that find clips by name (`VisitBase::walksMasks`), not only by drawing: a script plays it (AS1's credits: the hand's wipe is `mask.gotoAndPlay("wipe")`, three strips of the ink's mask shrinking; unfound, the ink stayed whole until the credits went).

A clip's `_parent` is the innermost clip holding it, unnamed ones included, whether the clip itself is named or not. Instance paths only show named clips, so `missionComplete.#4506` is in an unnamed 4507 inside `missionComplete`, and its `_parent._parent` is the banner, not `_root`. Likewise AS1's popup combo box, `inner.comboBox`, sits in two unnamed clips, and its text field's `_parent._parent.com` is the outer one's. An unnamed clip's named children are found by its named ancestor's path (`inner.#3115` reaching its `great` at `inner.great`). Where several copies share a path (named clips in unnamed copies of one clip, or unnamed copies themselves), a clip's lookup by name finds the copy inside it (`Scene::clipWithin`, by the walk's `ClipVisit::parent`), and that copy has a playhead of its own instance (`ArtNode::part(path, instance)`, drawn through `VectorLook::instanceFrames`): AS1's pause screen has three unnamed button clips each with a `myCol`, and a rollover lit all three; its bandage has eight strips each picking its own stain. A clip's `_parent` where its holder is unnamed is the very instance it is in (the walk's `ClipVisit::parent`, `SceneClip::parent`), not the first copy of that path: each strip re-places its stain every frame and the stain keeps its strip's pick in `_parent.rando`; reading another strip's (or none), it picked anew each frame, and the stains jumped about while the bandage covered the screen. The popup's unnamed text holder stops its two-frame `great` and hides its `comboBox` by name; when these lookups failed, the "GREAT" text bounced between its two frames and a combo box reading "x123" stayed.

## The stage: a game run by its own scripts

`src/surgeon/StageScreen.*` is a game's screen: the movie's main timeline (symbol 0) as an `ArtNode`, run by a `Scene` and `Runner` over the package's index, with:
- **Flash's built-in classes** (`FlashClasses.*`):
  - `Sound` (attach, start with loops, stop, volume, `onSoundComplete`);
  - `XML` (load from the package's `data/`, nodes, `onLoad` a frame later);
  - `SharedObject` (`getLocal`, `data`, `flush`). It is kept as key=value lines in the save folder; a game can name the file and rename keys, so an existing save keeps working;
  - `stopAllSounds`;
  - `Mouse` and `Key` listeners (`addListener`, `removeListener`): the pointer's `onMouseDown`, `onMouseUp`, `onMouseMove` and the keys' `onKeyDown`, `onKeyUp` broadcast to them, `Key.getCode()` and `Key.isDown()` (the number keys and Escape reach the stage).
- **Buttons as Flash ran them:** press, release, releaseOutside, rollOver, rollOut, dragOver, dragOut, with `this` the clip the button is in (`ArtNode::buttonAt` gives its node and path, attached clips and clips made nodes included) and over/down states shown.
- **Text fields** show their variables' values (`VectorLook::fieldText`, resolved from the field's clip: `_root.dScore`, `/a/b:x`), in attached clips too (their parent's lookup, through their name).
- **A piece's opacity** by its path (`VectorLook::alphas`: Flash's `alpha` set by a script, a button dimmed till it can be used).
- **A label by its beginning** (`VectorArt::labelStartingWith`: the one label of a clip that begins with a name, for a name another version shortened).
- **Each child clip's box** (`VectorArt::childBounds`): one box per movie clip placed directly in a piece (or in the symbol), by placement, so unnamed copies of one clip are told apart, as a script's `getChildAt` loop over them (a creature's hit areas).
- **The pointer clip.** A stage game can name the clip it keeps at the mouse (`StageScreen::pointerClip`, AS1's `game.cursor`; a native module sets `ArtNode::pointer` on its own, as Christmas's operation does). Once the scripts have made it a node, it is drawn as `platform::PointerArt`: where the mouse is at every display step and above everything, as AS2's and AS3's cursors are, and the system pointer is hidden while it shows.
- **Preparing art:**
  - the stage's own art is made a second ahead as it plays on;
  - the frame a transition is going to (named by a `_root` variable the game gives, `transitionTarget`) is made while the transition hides the screen.
- **Modules** (`StageModule`, `addModule(sprite, factory)`): native code for the instances of a sprite on the main timeline. A module made when its clip appears either:
  - **replaces the clip**: it is drawn between the timeline's pieces below and above it (`ArtNode::drawSlice`), takes the pointer where no button above it is, and gives the scripts its own clip for that name (`Scene::hand`); its scripts are not run; or
  - **adds to it**: drawn over it, with buttons of its own (`hits`), keys first (`takesEscape`).

A module that is real C++ (an operation) shares the game's state with the scripts through `Scene::bind`: a `_root` variable is a field of the game's code. Its sounds go through the scripts' own sound functions (`SoundBank::Scripts`).

A game is then:
1. a package (`extract_swf`) and its index (`index_scripts.py`);
2. a `StageScreen` with its save format, transition variable and modules;
3. the modules the index's report asks for.

A game's own code can run each pass after the stage's (`afterUpdate`: AS1's debug probe of its operation).

AS1 and Christmas are one Flash surgeon game (`FlashSurgeonGame`, [as1.md](as1.md#set-up-flashsurgeongame)), each set up with what is its own; neither has a native module for its operation, which runs by its own scripts. Christmas adds two small modules of the port's (its level select's BACK tag, the workshop's engine: [christmas.md](christmas.md#christmas-srcchristmas)).

## AS1 from Flash

AS1 runs wholly by its own translated scripts, its operation included: [as1.md](as1.md).


## cocos2d-x: the cocos layer

`port/src/engine/cocos/` is the subset of cocos2d-x 2.1.1 that the iOS games built on it need (AS3), ported from its MIT source (`re/as3/cocos2d-x-2.1.1/`; the notice in `LICENSE-cocos2d-x.txt`).
- **Names kept.** The classes keep cocos2d's names in namespace `cocos2d` (`CCNode`, `CCSprite`, `CCDirector`...), so a game mirrored from its binary reads against the decompile.
- **No game in it.** It is general: a feature is added to it as cocos2d had it.

**What is ported, and what is not.**
- **Logic as 2.1.1 had it:** the transforms (`nodeToParentTransform`), z order and order of arrival, onEnter/onExit and cleanup, the scheduler's timers and per-frame updates by priority, the actions and ease curves, the action manager's removal during a step, touch dispatch, the director's scene changes, sprite frames from TexturePacker sheets, and the sprites' quads. Removals while the scheduler or the action manager steps follow 2.1.1: the index of what is stepping moves back when a timer, a target's timers or a target's actions at or before it go, so nothing after it misses the frame (2.1.1's `timerIndex`; its linked hashes skipped none). A node's onEnter/onExit/cleanup walk a retained snapshot of its children and skip one taken off meanwhile, so a child removing a sibling cannot free one still to be called. A menu forgets its pressed item when the item is removed (2.1.1's `CCMenu::removeChild`).
- **Left out:** the director's display link, its renderer and GL state cache, the shaders, scripting, the camera, the platform layers. Their work goes to the engine.

**On the engine.**
- **The loop.** The game calls `CCDirector::mainLoop()` once a pass. Its delta time is the pass length, the animation interval given to `platform::setPassSeconds`, so play is deterministic and the engine smooths what moves between passes.
- **The screen.** It is the game's in points (`GameInfo`'s size at the start). `CCDirector::setDesignResolutionSize(w, h)` changes it, as 2.1.1's `CCEGLView::setDesignResolutionSize` with `kResolutionShowAll` did (the engine's platform letterboxes): the director's win size and the platform's screen (`platform::setScreenSize`) from the next frame drawn; nodes already laid out keep their places, so a game calls it between screens (AS3: as a screen is replaced). The director sets cocos's 2D projection (y up, the origin at the bottom left), and each node multiplies its transform onto GL's modelview in `visit()`.
- **Drawing.** A node draws through the recorded drawing (`platform::drawQuad`, `drawMesh`), owned by its serial, with its blend function (`platform::BlendDst` for a destination factor other than `ONE_MINUS_SRC_ALPHA`).
- **Batches.** `CCSpriteBatchNode` keeps no atlas: its sprites draw themselves in the atlas's order, which looks the same.
- **Files.** `CCFileUtils` finds files in the package by the game's search paths and resolution order (`assets`). The writable path is the save folder.
- **Textures** are premultiplied as on iOS: a PNG is premultiplied when loaded, and a PVR is as stored, flagged premultiplied when the game says so (`CCTexture2D::PVRImagesHavePremultipliedAlpha`). Sprites blend as 2.1.1 chose (`GL_ONE` and an opacity-premultiplied colour).
- **Input.** The pointer is one touch (`CCDirector::handleTouch*`). Escape is the keypad's back. The accelerometer is the device's (`platform::deviceAcceleration`); a PC has none, and a game gives the PC its own equivalent.

**Classes, so far:**
- `CCGeometry`, `ccTypes`, `CCObject` (on the engine's `Object`; the autorelease pool drained once a pass), `CCArray`;
- `CCNode`, `CCScheduler`/`CCTimer`, `CCAction` (interval, instant, ease: the ones in use), `CCActionManager`;
- `CCTouch`/`CCTouchDispatcher`, `CCDirector` (with the accelerometer and the keypad dispatcher), `CCScene`, `CCLayer`/`CCLayerColor`/`CCLayerGradient`;
- `CCFileUtils`, `CCTexture2D`/`CCTextureCache`, `CCSpriteFrame`/`CCSpriteFrameCache`, `CCSprite`/`CCSpriteBatchNode`;
- `CCTexture2D` keeps a 4×4-pixel max-alpha mask of every decoded file (built on the decode worker, 1/64 of its bytes) for hit tests on art (`alphaAt`; `CCSprite::opaqueAt` maps a node point to it, rotated frames and flips included);
- `CCMenuItem::setTouchRect` (the port's): where a touch takes the item, in its own coordinates (a part of its picture); none is the whole item, as in 2.1.1;
- a texture has its own content scale (`CCTexture2D::getContentScale`): its art folder's (`.../3.5x/...` → 3.5), else the director's; sprite frames, sprites' texture coordinates and particles' texture rects convert between pixels and points by it, so art drawn for different scales mixes on one screen (AS3's iOS 4.0x art and the Android Hospital's 3.5x art);
- sprite frames: a frame name loaded from several sheets keeps its first frame, as 2.1.1 did, except that a blank one (TexturePacker's 1×1 for a wholly transparent picture) is replaced by a later sheet's drawn frame of the same name (AS3's `surgery_hubris1_sheet` packed some mini worm frames blank);
- textures: the cache keeps what is taken off it (`removeTextureForKey`, `removeUnusedTextures`), oldest dropped first, up to a sixteenth of the machine's memory (64 MB–1 GB), so a file asked for again gets its texture back with no decoding (the games unload their sheets as a screen goes; iOS uploaded PVRTC as it was). A sprite sheet's texture is decoded on a worker thread: `CCSpriteFrameCache` passes the sheet's size (`metadata.size`) to `addImage(path, w, h)`, which returns a stand-in at once, uploaded when first drawn (`getName` waits for it) or when the game asks (`finishDecodings`, `uploadDecoded(seconds)`); a PVR whose premultiplication can't be foreseen is decoded at once. PVRTC is decoded on several threads (the decoding's own and up to seven helpers taking block rows in chunks, each helper only while it holds one of the cores shared by all decodings, all but two left for the game's thread and the GL driver's), bit for bit `texture2ddecoder`'s: a 4096×4096 sheet in about 50 ms instead of 330.
- `CCTransitionScene` (only its base: the director tells scene changes by it);
- the platform's clipboard: `platform::clipboardText` / `setClipboardText` (SDL; in debug mode `AS_CLIPBOARD_FILE` keeps it in a file, for tests);
- `CCDirector::setOverlay(fn)` (not 2.1.1's): called each frame after the scene and the notification node, before the frame is presented, with the projection it sets itself; AS3's debug menu draws there;
- the grid effects: `CCGridBase`/`CCGrid3D` and the actions `CCGridAction`/`CCGrid3DAction`, `CCLiquid`, `CCStopGrid` (below);
- `CCDrawNode` (filled dots, segments and convex polygons as recorded meshes; polygons extruded as 2.1.1: mitred corners, the border ±borderWidth, the antialiased fringe ±0.5 drawn solid) and `CCClippingNode` (its children drawn only where its stencil node draws, on the engine's masks: `stencilMode`/`maskLevel`, which keep the stencil's texels over half opaque as 2.1.1's default alpha threshold of 1 keeps a solid stencil; no inverted clipping);
- `CCParticleSystem`/`CCParticleSystemQuad` (below).

**Grid effects.** In 2.1.1 a node with an active grid was drawn into a window-sized texture, and the texture drawn on the grid's moved corners. The port draws the node's pieces on the moved grid directly: `CCGrid3D::beforeDraw` starts `platform::beginWarp` with the grid's corners (window points, under the parent's transform, as the original's blit), `afterDraw` ends it; every draw recorded meanwhile is shown with each point moved bilinearly with the corners of its cell, quads cut into pieces small against a cell. It looks the same as long as the node's pieces don't overlap in ways the texture would have merged (alpha over alpha). The displacement eases between passes like other code-driven motion. The depth of the corners is not shown (the grid effects in use are flat).

**Particles.** `CCParticleSystem` is 2.1.1's emitter as it was: life, position, colour, size and spin as value ± variance (`CCRANDOM_MINUS1_1`, `rand()/RAND_MAX`); gravity and radius modes; free, relative or grouped particles; emission, death and auto-removal as in 2.1.1's `update`. A system is read from its Particle Designer list, which the package keeps as JSON (`initWithFile`, `initWithDictionary(json::Value, dirname)`). Its texture is a file beside the list, or the list's own base64 gzip `textureImageData` (`CCTextureCache::addImageData`). `CCParticleSystemQuad` draws each living particle's quad through the recorded drawing (`drawQuad` under `BlendDst`, with the system's blend function). Each particle is its own draw owner (`tCCParticle::drawId`, a serial given when it is emitted), so its motion is smoothed between passes. Fixed: an emitter moved after its update (a game placing it in a timer, which 2.1.1's scheduler runs after the updates: AS3's surgery moves the tool's effects in its tick) drew every free or relative particle shifted by that move, so trails jumped with each step of the pointer; the quads remember the emitter position they were written for (`m_tQuadsPosition`, `emitterPosition()`) and `draw` shifts them back by any move since. Between passes a transform's move follows a curve through the last three passes, except for a draw whose own corners slid too (a free particle: written where it is less its emitter's move, the transform adding the move back): both halves then go in a straight line, so they cancel and the particle stays put. Not ported: `CCParticleBatchNode` (unused), the vertex buffers and shader. Fixed: growing `setTotalParticles` no longer loses the quads' texture coordinates (2.1.1 and the game's copy cleared them, so the particles drew an empty texel). 2.1.1's scheduler retains its update targets; the port's does not, and a node takes its update off when it is freed (`~CCNode`). A node that can remove itself in its update (an auto-removing particle system) must not touch itself afterwards.

**Memory checks.** An AddressSanitizer build of AS3 ([port.md](port.md)'s recipe, target `AmateurSurgeon3`) runs the play-test driver unchanged (`--build <asan dir>`); `ASAN_OPTIONS=log_path=asanlog:detect_leaks=0` writes any report beside the exe.

## UIKit: views from nibs

`port/src/engine/ui/` is the subset of UIKit and Core Animation that the iOS games whose screens are UIKit views need (AS2). It is general: no game's names are in it.

- **Nibs.** `port/tools/nib2json.py` decodes a compiled nib (Apple's NIBArchive format) into JSON: the view tree (class, the UIKit class under a custom one, frame, hidden, alpha, tag, content mode, background colour; images, button states, text and font), the File's Owner's outlets and the actions. `extract_ipa.py` writes each IPA nib as `ui/<name>.json`. `ui::loadNib` builds the views; a game registers its own view classes by name (`ui::registerViewClass`, e.g. AS2's `PCGImageView`).
- **Views.** `View` (centre, bounds, transform, alpha, hidden, tag, subviews, hit testing, the responder chain to its view controller and superview), `ImageView` (content modes, `animationImages`, `sizeToFit`), `Button` (images, background images and titles per state; target-action on touch up inside), `TextView` (text wrapped as UIKit does, inset 8 for a text view; drawn with the system font that stands in for the iOS one, at the display's resolution), `GLView` (a hook draws its content: the game's GL scene), `AlertView` (a modal box: title, message, buttons), `Window` (the root: touches, timers, animations, drawing; a touch on a view taken out of the window is cancelled, as UIKit's, since a control's targets are not retained and may be going with it: a view controller killed mid-press).
- **Images.** `Image::named` reads a bundle picture (PNG by default). A name with a scale or device modifier (`x@2x.png`, `tutePop~ipad.png`) falls back to the plain name when the bundle has only that, as UIKit did; a picture not found at all is traced (`image not found: <name>`).
- **View controllers.** `ViewController` loads its nib on first use, connects its outlets (`connectOutlet`) and its buttons' actions (`action(selector, sender)`), then calls `viewDidLoad`. One with no nib name (a screen drawn by the game, as AS2's Flash clipboards) gets a plain view the size of the screen.
- **Core Animation.** Layer animations by key: basic (`position`, `opacity`, `transform.rotation`, from/to), keyframe (`position` along a path, each segment the same time), and the window's `CATransition` (fade, moveIn, push, reveal; from the four sides), with their timing functions and delegates (`animationDidStart`, `animationDidStop`). A transition shows the window as it was last drawn giving way to the window as it is.
- **Timers.** `Timer::scheduled` (NSTimer): once or repeating.
- **Time.** Everything runs on the pass clock (`platform::passTime()`), so it advances exactly one pass per pass; the views draw through the recorded drawing under their own transforms, so what an animation moves is smoothed between passes.
