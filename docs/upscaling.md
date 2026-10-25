# Upscaling the art: research

How to get sharper versions of the game art (mainly the Christmas art that AS1 has no HD copy of) without "AI slop": invented texture, changed style, wobbly lines. Researched 2026-10-14. Option 1 was used by the archived iOS-based Christmas port: 116 images and the story intro drawn from the Flash vectors at the display's resolution (`port/attic/xmas-ios/tools/xmas_flash.py`, `xmas_intro.py`; see [christmas-ios-port.md](christmas-ios-port.md#christmas-on-as1s-screen)). Christmas is now rebuilt from the Flash original, all of its art vectors ([christmas.md](christmas.md)); this page still applies to AS1's art until its own Flash rebuild.

## What the art is

- Flat-colour cartoon art with dark outlines and some soft gradients and shading. It was drawn in Flash: both games began as Adult Swim Flash games by Mediatonic (Amateur Surgeon, May 2008; Christmas Edition, December 2008), and the iOS versions use pictures exported from that art.
- Mostly small transparent PNG sprites (20–500 pixels), sprite sheets of frames, and some full-screen backgrounds. Christmas is iPhone resolution (480×320).
- The engine already takes an HD version of any image: `images-hd/<name>` at exactly twice the size (see [christmas-ios-port.md](christmas-ios-port.md#christmas-on-as1s-screen)). Whatever method is used only has to produce those files.

## Options, best first

### 1. The original vector art from the Flash games (no AI at all)

The Flashpoint archive preserves both Flash games ("Amateur Surgeon" and "Amateur Surgeon Christmas", Mediatonic, Flash, arcade library). A SWF keeps the art as vector shapes. JPEXS Free Flash Decompiler (FFDec, open source, also scriptable from the command line) exports shapes as SVG and sprites or frames as PNG at any zoom, with an option against antialiasing seams.

- **Pros:** the real art, re-rendered at any size. Nothing is guessed, so nothing can be slop.
- **Plan:** export every shape and sprite at a high zoom. Pair each iOS PNG with its Flash render using the content matcher `port/tools/xmas_hd.py` already has: same outline, same picture after a colour fit, checked by eye. Then render the match at exactly 2× into `images-hd/`.
- **Caveats:**
  - The iOS art may have been redrawn, re-cropped or combined from several Flash symbols, so some pictures will not match one shape.
  - A few pieces may be bitmaps inside the SWF, which brings no gain.
  - The Flash games may lack iOS-only art (the help phone, iPhone menus).

### 2. Faithful CNN/GAN upscalers trained on anime and cartoon art

These sharpen what is there and do not invent texture, which suits flat-colour art. Avoid diffusion upscalers (SUPIR, Stable Diffusion "upscale", generative or creative modes): they repaint the image from a prompt, and that is exactly the slop.

| Model | Notes |
|---|---|
| Real-CUGAN (Bilibili) | Anime model, 2×/3×/4×, several denoise strengths and a "conservative" variant. Preserves lines and soft areas well. `realcugan-ncnn-vulkan` is a portable exe. |
| Real-ESRGAN AnimeVideo-v3 | Clean and faithful to colours, fast; `realesrgan-ncnn-vulkan` exe. The older `x4plus_anime_6B` sharpens hardest and can change the style or redraw lines wrongly, so prefer v3. |
| waifu2x (cunet) | The most conservative: softer, but never strange. A good baseline. |
| 4x PocketMonsters Alpha (OpenModelDB) | Trained on Pokémon drawings, **vector art** and sprites **with alpha**: the closest match to this art, transparency included. |
| 2x AniScale 2 (OmniSR, OpenModelDB) | Described as faithful and deliberately non-aggressive, with good line refinement; an optional 1× refiner. CC-BY-NC. |
| 2x HFA2k Compact, 2x AnimeJaNai v2, 2x AnimeSharpV3, 2x KemonoScale v2 | Anime and flat-shaded 2× models: natural, line-preserving. Worth a side-by-side. |

**Tooling:**
- **chaiNNer** is a node-based GUI with folder batch processing. It runs PyTorch (via Spandrel, so almost every OpenModelDB model), NCNN, ONNX and TensorRT, and installs its own Python.
- **The ncnn-vulkan executables** run from a script without any Python setup.
- **This PC:** an RTX 4090 with 32 GB of RAM runs all of these quickly, the transformer-based ones too.

**Transparency:** most models take RGB only. Upscale the colours over a matte and upscale the alpha channel separately (as a grey image, or with a model trained with alpha, such as PocketMonsters Alpha). Then rebuild a straight-alpha PNG. Working premultiplied avoids dark or white fringes.

**Sprite sheets:** upscale the whole sheet so neighbouring frames keep their gaps, then resize to exactly 2× so the engine's frame grid still lines up.

### 3. Vectorising (VTracer)

VTracer traces colour images into SVG; Potrace only handles black and white. Clean flat pieces such as text tags, buttons and simple shapes come out perfectly sharp at any size. Gradients, soft shadows and small details are flattened or simplified. Only for selected pieces, and option 1 gives the real vectors anyway.

### 4. Classic scalers

xBRZ, ScaleFX and hqx are made for pixel art without antialiasing. On this antialiased art they add little over plain resampling. Lanczos stays the fallback.

## Keeping it honest: an automatic check against slop

Whatever the method, every result can be checked against the original. Scale it back down to the original size and compare it with the original, using the same test `xmas_hd.py` uses (blurred outline, structure and colour). A faithful upscale downsamples to (almost) the original. Invented detail, moved lines or changed colours fail. Only results that pass are kept, and a side-by-side sheet goes to a human for the final look.

## Recommendation

1. Try the Flash originals first. Download both games from Flashpoint, export everything with FFDec, and see what share of the iOS pictures match a vector shape. That share becomes perfect HD.
2. For the rest, run a short shoot-out on about 10 representative pictures: an outlined object, a gradient organ, a text tag, a sprite sheet and a background. Candidates are Real-CUGAN (conservative), Real-ESRGAN AnimeVideo-v3, PocketMonsters Alpha and AniScale 2, with waifu2x as the baseline. Pick the method that passes the round-trip check and looks cleanest.
3. Priority when both exist: AS1's own HD art (already in use), then the Flash vector render, then the AI upscale. The engine needs nothing new: all of them become `images-hd/` files at 2×.

## Sources

- Flashpoint database entries (Amateur Surgeon, Amateur Surgeon Christmas, Amateur Surgeon 2; Mediatonic): https://db-api.unstable.life/search?smartSearch=Amateur%20Surgeon
- Amateur Surgeon history (Flash, Mediatonic/Adult Swim, Christmas Edition December 2008): https://tropedia.fandom.com/wiki/Amateur_Surgeon
- JPEXS FFDec: https://github.com/jindrapetrik/jpexs-decompiler/wiki/Commandline-arguments
- Real-CUGAN: https://github.com/bilibili/ailab/blob/main/Real-CUGAN/README_EN.md, https://github.com/nihui/realcugan-ncnn-vulkan
- Real-ESRGAN anime comparisons: https://github.com/xororz/Real-ESRGAN/blob/master/docs/anime_comparisons.md
- OpenModelDB: https://openmodeldb.info/models/4x-PocketMonsters-Alpha, https://openmodeldb.info/models/2x-AniScale-2-OmniSR, https://openmodeldb.info/models/2x-HFA2kCompact, https://openmodeldb.info/models/2x-AnimeJaNai-v2-Compact, https://openmodeldb.info/models/2x-AnimeSharpV3, https://openmodeldb.info/models/2x-KemonoScale-v2
- chaiNNer: https://github.com/chaiNNer-org/chaiNNer
- VTracer: https://github.com/visioncortex/vtracer
- Diffusion upscalers and their trade-offs: https://botmonster.com/ai/local-ai-image-upscaling-real-esrgan-topaz-supir/
