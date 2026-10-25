# Third-party licenses

This project's own code is MIT-licensed (see `LICENSE`). It vendors or ports
a few third-party libraries, each under its own permissive license, kept
alongside its source under `port/third_party/` and `port/src/engine/cocos/`:

| Component | Where | License |
|---|---|---|
| cocos2d-x 2.1.1 (a subset) | `port/src/engine/cocos/` | MIT (`LICENSE-cocos2d-x.txt` there) |
| lunasvg 3.5.0 | `port/third_party/lunasvg-3.5.0/` | MIT (`LICENSE` there) |
| libtess2 | `port/third_party/libtess2/` | SGI Free Software License B, v2.0 (`LICENSE.txt` there) |
| texture2ddecoder | `port/third_party/texture2ddecoder/` | MIT (`LICENSE` there) |
| miniaudio | `port/third_party/miniaudio.h` | MIT-0 / public domain (dual-licensed, see its header) |
| stb_image / stb_image_write | `port/third_party/stb_image*.h` | MIT / public domain (dual-licensed, see their headers) |

Each vendored component's own license file or header comment is the
authoritative text; this table is a summary, not a substitute.
