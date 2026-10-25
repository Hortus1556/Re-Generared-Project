# Amateur Surgeon (iPad) 1.0.3 — analysis

Source: `IPAs/A.Surgeon_v1.0.3_os32.ipa` → `Payload/AmateurSurgeonIPad.app`

## Identity

- Bundle id `com.turner.AmateurSurgeonIPad`, display name "A.Surgeon", v1.0.3 (Adult Swim / Turner; original game by Mediatonic).
- iPad only (`UIDeviceFamily = [2]`), iOS 3.2 minimum, built against iOS 4.0 SDK.
- Fixed landscape, 1024×768, status bar hidden.

## Executable

- `AmateurSurgeonIPad`: 4.3 MB, single-arch **armv7** Mach-O.
- **Decrypted** (`LC_ENCRYPTION_INFO cryptid = 0`), so it can be disassembled directly.
- **Not stripped**: 66,299 symbols with full Objective-C method names and mangled C++ names. Every method's name and argument labels are recoverable, which makes decompiling it (Ghidra/IDA) much easier.
- About 1.43 MB of `__text`, split roughly as:
  - ~720 KB game and engine code (about 150 game classes);
  - ~600 KB OpenFeint 2.4.10 (social/leaderboards; the service is dead);
  - ~110 KB Facebook, Twitter, and the "HS" high-score web service (all dead).

### Engine ("A3I" in-house framework)

- Rendering: an `EAGLView` with **OpenGL ES 1.1 fixed-function** only. The game imports just 41 GL calls (`glOrthof`, `glVertexPointer`, `glDrawElements`, `glTexImage2D`, `glCompressedTexImage2D`, matrix stack, blend, FBO OES). Desktop OpenGL compatibility profile maps almost 1:1.
- 2D sprite system: `Layer`, `Spritex`, `AdvancedImage`, `FrameData`, `GameCanvas`, C++ `ResourceManager` / `ResourceTexture` / `A3ICTexture` (loads PNG and PVRTC).
- 3D: `A3ICPODScene`, the PowerVR SDK POD loader, used only for `poison01–05.POD`.
- Audio: OpenAL (`OALSoundEngine`, `OALSoundObject`, `SoundEffectTrack`) plus `AVAudioPlayer` for music (`BackgroundMusicTrack`).
- Video: `MPMoviePlayerController` for `splash.m4v`, `storyOpening.mp4`, and `ipad_storyOpening.mp4`.
- Input: touches via `touchData`, plus `UIAccelerometer` (`GCAcceleration`).

### Game structure (class names)

- Flow: `Splash`, `MainMenu`, `PatientSelection`, `StoryBoard` (dialogue cutscenes), `HelpMenu`, `ReportBoard`, `LoseGame`, `WinSplash`, `LoadingPanel`, `GamePanel`, `Toolbar`.
- Levels: base class `Operation` plus `Operation01` … `Operation30`.
- Surgical mechanics: `SmallCut`, `LargeCut`, `DoubleLargeCut`, `CutOpen`, `Wound`, `FireWound`, `SurfaceExtraction`, `HiddenExtraction`, `ExplosiveExtracton`, `OrganTransplant`, `Implant`, `RibCagePuzzle`, `WireConnect`, `Vacuum`, `CorkScrew`, `Electricity`, `Poison`, and others.
- Creatures and objects: `Ant`, `AntNest`, `Centipede`, `SpiderQueen`, `SpiderEgg`, `Scopion`, `BloodSucker`, `BloodSuckerKing`, `TapeWorm`, `Bomb`, `CarA`, and organ classes (`Kidney`, `Lung`, `BrainA`, and so on).
- Persistence: `NSUserDefaults` / `NSKeyedArchiver` (`RMSManager`, `HighScoreManager`).

## Assets (all reusable as-is or with simple conversion)

| Kind | Count | Notes |
|---|---|---|
| PNG | 1741 | **Apple CgBI "crushed" PNGs**: BGRA byte order, premultiplied alpha, raw deflate. They must be un-crushed before standard tools or libraries can read them. |
| JPG | 75 | Backgrounds, standard JPEG |
| TXT | 128 game files | Animation and layout data (see below) |
| MP3 / WAV | 26 / 3 | Music and effects (`Sound_N`) |
| MP4 / M4V | 3 | Splash and story intro video (H.264) |
| XML | 4 | `ipad_dialogue.xml` + `ipad_dialogue_seq.xml` (story/dialogue script and its per-level sequence) |
| POD | 5 | PowerVR 3D models (poison bottle animation) |
| NIB | 182 | All OpenFeint UI except `AmateurSurgeonViewController.nib`, so none are needed for a port |

Data text formats:

- `PNGpackinglist.txt`: atlas map with lines of the form `<sprite> <atlas> x0 y0 x1 y1`. It includes leftover entries from the iPhone build; only the `ipad_*` atlases are present in this bundle.
- `<name>.txt` next to a same-named image (for example `ipad_start01.txt`): per-frame offset, angle, and alpha data feeding `FrameData` (`initWithOffsetX:offsetY:angle:alpha:`).
- Compact animation tracks (for example `Karl.txt`): numeric tokens separated by `a`/`b`/`c`, exported from the original Flash animations.
- `boneSpot.txt` / `brainSpot.txt`: `index,frame,angle` path tables for the cut/trace mechanics.

These formats still need confirming against the parsing code (`DataLoader`, `AdvancedImage`, `Spritex`) during the port.

## Porting options for Windows

**Settled:** option 2 was taken (the iPad port, [port.md](port.md)). Since then the plan is one engine for every game, with AS1 to be rebuilt from its Flash original on it ([plan.md](plan.md)).

1. **Emulate with touchHLE** (open-source iPhone OS HLE emulator for Windows). It supports this era (armv6/armv7, GLES 1.1, OpenAL), but iPad (iOS 3.2) apps are only partially supported. The compatibility database lists only the iPhone build (`com.turner.AmateurSurgeon` 1.1.9), and that report records an error. Getting this build running would be quick to try, but any gaps (MediaPlayer video, OpenFeint, MapKit or CoreLocation linkage) would have to be fixed inside the emulator.
2. **Native reimplementation** (C++ with SDL2 + OpenGL + OpenAL Soft + an MP4 decoder): use Ghidra plus the full symbol names to decompile the ~150 game classes and rewrite them against a small platform layer that replaces UIKit, GLES, and MediaPlayer. Drop OpenFeint, Facebook, Twitter, and the HS web service. The result is a real Windows executable and the cleanest outcome, but it means rewriting about 720 KB of ARM logic: a large but mechanical job, since the names are intact.

The rendering and audio subsets are small. Game logic is the bulk of the work in either native approach.
