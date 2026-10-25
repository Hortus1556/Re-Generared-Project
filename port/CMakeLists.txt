cmake_minimum_required(VERSION 3.20)
# Windows (llvm-mingw, cmake/llvm-mingw.cmake), or Android (the NDK's
# toolchain file, port/tools/android/build_apk.py): there one game
# (ANDROID_GAME), as the libmain.so SDL's Android glue loads.
project(AmateurSurgeon CXX C)
if(WIN32)
  enable_language(RC)  # (the exes' icon and version resources)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(ANDROID)
  # SDL3 from its source (tools/sdl3-src, the same version as the Windows
  # one), built as the shared library its Java glue loads.
  get_filename_component(_ws "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
  file(GLOB _sdl_src "${_ws}/tools/sdl3-src/SDL3-*")
  if(NOT _sdl_src)
    message(FATAL_ERROR "SDL3's source not found under ${_ws}/tools/sdl3-src")
  endif()
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  add_subdirectory(${_sdl_src} sdl3 EXCLUDE_FROM_ALL)
else()
  find_package(SDL3 REQUIRED CONFIG)
endif()

# lunasvg (MIT): rasterises vector art (SVG) at the display's resolution.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/lunasvg-3.5.0 EXCLUDE_FROM_ALL)

# libtess2 (SGI Free Software License B 2.0): cuts vector art's filled
# outlines into triangles for the GPU (flash/VectorMesh).
file(GLOB TESS_SOURCES third_party/libtess2/Source/*.c)
add_library(tess2 STATIC ${TESS_SOURCES})
target_include_directories(tess2 PUBLIC third_party/libtess2/Include)
target_compile_options(tess2 PRIVATE -w)

# Bleed Engine, shared by every game: the core, the PC platform layer, the
# engine (grown from the original iOS games' "A3I" engine) and the Flash
# runtime.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/platform/*.cpp src/engine/*.cpp src/flash/*.cpp)
add_library(BleedEngine STATIC ${ENGINE_SOURCES})
target_include_directories(BleedEngine PUBLIC src third_party)
if(ANDROID)
  # OpenGL ES 1.1 (platform/GL.h), the NDK's media codecs (AAC music).
  target_link_libraries(BleedEngine PUBLIC SDL3::SDL3 GLESv1_CM EGL android log mediandk lunasvg::lunasvg plutovg::plutovg tess2)
elseif(WIN32)
  target_link_libraries(BleedEngine PUBLIC SDL3::SDL3 opengl32 mfplat mfreadwrite mfuuid ole32 shell32 lunasvg::lunasvg plutovg::plutovg tess2)
else()
  # Linux: desktop GL (platform/GL.h's non-Android branch, the same
  # fixed-function API Windows uses). AAC and movie decoding
  # (platform/MovieDecoder.cpp) has no Linux implementation yet, so no
  # codec library is linked for it -- those calls just report "absent"
  # rather than play (AS1 and Christmas need neither).
  target_link_libraries(BleedEngine PUBLIC SDL3::SDL3 GL pthread lunasvg::lunasvg plutovg::plutovg tess2)
endif()
target_compile_options(BleedEngine PUBLIC -Wall -Wno-unused-parameter)

# add_game(<exe> <game source dir> <resource .rc> <assets dir> <dist dir> [shared source dirs...])
# One game: its own game layer on the shared engine, its icon and version
# resource, and a `<exe>_dist` target that packs <workspace>/dist/<dist dir>:
# the exe (debug info stripped), SDL3.dll and port/<assets dir> as `assets`.
# Debug features stay off unless the exe is started with -d.
function(add_game exe game_dir rc assets_dir dist_name)
  if(NOT WIN32 AND NOT ANDROID)
    # A native Linux build's dist must not land in the same dist/<name>
    # folder a Windows build already uses for an ELF of the same name.
    set(dist_name "${dist_name}-linux")
  endif()
  file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS ${game_dir}/*.cpp)
  foreach(shared_dir ${ARGN})
    file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS ${shared_dir}/*.cpp)
    list(APPEND GAME_SOURCES ${SHARED_SOURCES})
  endforeach()
  if(WIN32)
    # ${rc} (the icon/version resource) needs the RC language, enabled
    # above only on Windows; it has no Linux equivalent.
    add_executable(${exe} WIN32 ${GAME_SOURCES} ${rc})
  else()
    add_executable(${exe} ${GAME_SOURCES})
  endif()
  target_link_libraries(${exe} PRIVATE BleedEngine)

  get_filename_component(dist_dir "${CMAKE_SOURCE_DIR}/../dist/${dist_name}" ABSOLUTE)
  set(dist_cmds
    COMMAND ${CMAKE_COMMAND} -E make_directory "${dist_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${exe}> "${dist_dir}"
    COMMAND ${CMAKE_STRIP} "${dist_dir}/$<TARGET_FILE_NAME:${exe}>")
  if(WIN32)
    # Self-contained exe: no dependency on the llvm-mingw runtime DLLs.
    target_link_options(${exe} PRIVATE -static)
    # Ship next to the exe: SDL3.dll (the assets are found in port/ from the build dir).
    add_custom_command(TARGET ${exe} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:${exe}>)
    list(APPEND dist_cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> "${dist_dir}")
  endif()
  # (On Linux, SDL3 is linked statically -- built that way by
  # port/tools/linux/setup_sdl3.sh -- so there's no shared lib to ship
  # beside the exe.)
  list(APPEND dist_cmds
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${dist_dir}/assets"  # (no file the package dropped stays)
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/${assets_dir}" "${dist_dir}/assets")
  add_custom_target(${exe}_dist ${dist_cmds}
    DEPENDS ${exe}
    COMMENT "Packing ${exe} into ${dist_dir}")
endfunction()

if(ANDROID)
  # One game for Android, ANDROID_GAME (as1: Amateur Surgeon, christmas:
  # its Christmas Edition, both on the stage, src/surgeon; as2: Amateur
  # Surgeon 2; as3: Amateur Surgeon 3): its classes as libmain.so (its main
  # is SDL_main, SDL_main.h); the APK is packed by
  # port/tools/android/build_apk.py with port/games/<game> as its assets
  # (the Flash games' packages made by the Windows build).
  set(ANDROID_GAME "as2" CACHE STRING "The game built for Android: as1, christmas, as2 or as3")
  if(NOT ANDROID_GAME MATCHES "^(as[123]|christmas)$")
    message(FATAL_ERROR "ANDROID_GAME must be as1, christmas, as2 or as3, not ${ANDROID_GAME}")
  endif()
  file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/${ANDROID_GAME}/*.cpp)
  if(ANDROID_GAME MATCHES "^(as1|christmas)$")
    file(GLOB_RECURSE STAGE_SOURCES CONFIGURE_DEPENDS src/surgeon/*.cpp)
    list(APPEND GAME_SOURCES ${STAGE_SOURCES})
  endif()
  add_library(main SHARED ${GAME_SOURCES})
  target_link_libraries(main PRIVATE BleedEngine)
  return()
endif()

# Amateur Surgeon (AS1), rebuilt from its Flash original on the engine
# (src/as1: its set-up and its operation; src/surgeon: the stage). The
# earlier port from the iPad app is kept, unbuilt, in attic/as1-ipad/.
add_game(AmateurSurgeon src/as1 res/AmateurSurgeon.rc games/as1 surgeon1 src/surgeon)
# Christmas, rebuilt from its Flash original on the engine (src/surgeon: the
# surgeon game AS1's rebuild will share; src/christmas: its content). The
# earlier iOS-based port is kept, unbuilt, in attic/xmas-ios/.
add_game(AmateurSurgeonXmas src/christmas res/AmateurSurgeonXmas.rc games/christmas surgeonxmas src/surgeon)

# Amateur Surgeon 2 HD, ported from its iPad app (src/as2: its classes;
# games/as2: its package, made by tools/extract_ipa.py and tools/as2_data.py).
add_game(AmateurSurgeon2 src/as2 res/AmateurSurgeon2.rc games/as2 surgeon2)

# Amateur Surgeon 3, ported from its iOS app (src/as3: its classes, on the
# engine's cocos layer, src/engine/cocos; games/as3: its package, made by
# tools/extract_ipa.py).
add_game(AmateurSurgeon3 src/as3 res/AmateurSurgeon3.rc games/as3 surgeon3)
# Its debug launcher (-d: the F1 debug menu) beside the exe, in the build and the package.
set(AS3_DEBUG_BAT "${CMAKE_SOURCE_DIR}/res/AmateurSurgeon3 (debug).bat")
add_custom_command(TARGET AmateurSurgeon3 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AS3_DEBUG_BAT}" $<TARGET_FILE_DIR:AmateurSurgeon3>)
add_custom_command(TARGET AmateurSurgeon3_dist POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AS3_DEBUG_BAT}" "${CMAKE_SOURCE_DIR}/../dist/surgeon3")

# Christmas's package: the Flash original (re/flash, not shipped) extracted
# into port/games/christmas (library.json, vector/, audio/; docs/engine.md)
# beside its own data/, with the user's restored sound: every sound it
# shares with AS1 is AS1's restored one (music restored/as1/sfx, and the
# operation's music operation.mp3 for operation_s), except those Christmas
# has its own of (music restored/asx/sfx: <linkage>.wav or .mp3), and its
# title music asx/xmasmusico.mp3. (A new file there: re-run cmake.)
add_executable(extract_swf src/tools/extract_swf.cpp)
target_link_libraries(extract_swf PRIVATE BleedEngine)
if(WIN32)
  target_link_options(extract_swf PRIVATE -static)
endif()
set(XMAS_SWF "${CMAKE_SOURCE_DIR}/../re/flash/xmas/games.adultswim.com/AmateurSurgeonChristmas/Amateur Surgeon Christmas.swf")
set(XMAS_PACKAGE "${CMAKE_SOURCE_DIR}/games/christmas")
set(XMAS_MUSIC "${CMAKE_SOURCE_DIR}/../music restored/asx")
set(XMAS_SOUNDS --sound "xmasmusico=${XMAS_MUSIC}/xmasmusico.mp3")
set(XMAS_SOUND_FILES "${XMAS_MUSIC}/xmasmusico.mp3")
file(GLOB XMAS_OWN_SFX CONFIGURE_DEPENDS "${XMAS_MUSIC}/sfx/*.wav" "${XMAS_MUSIC}/sfx/*.mp3")
set(XMAS_OWN_NAMES)
foreach(sfx ${XMAS_OWN_SFX})
  get_filename_component(linkage "${sfx}" NAME_WE)
  list(APPEND XMAS_OWN_NAMES "${linkage}")
  list(APPEND XMAS_SOUNDS --sound "${linkage}=${sfx}")
  list(APPEND XMAS_SOUND_FILES "${sfx}")
endforeach()
file(GLOB XMAS_AS1_SFX CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/../music restored/as1/sfx/*.mp3")
set(XMAS_AS1_OPERATION "${CMAKE_SOURCE_DIR}/../music restored/as1/operation.mp3")
if(NOT "operation_s" IN_LIST XMAS_OWN_NAMES)
  list(APPEND XMAS_SOUNDS --sound "operation_s=${XMAS_AS1_OPERATION}")
  list(APPEND XMAS_SOUND_FILES "${XMAS_AS1_OPERATION}")
endif()
foreach(sfx ${XMAS_AS1_SFX})
  get_filename_component(linkage "${sfx}" NAME_WE)
  if(NOT linkage IN_LIST XMAS_OWN_NAMES)
    list(APPEND XMAS_SOUNDS --sound "${linkage}=${sfx}")
    list(APPEND XMAS_SOUND_FILES "${sfx}")
  endif()
endforeach()
add_custom_command(OUTPUT "${XMAS_PACKAGE}/library.json"
  COMMAND $<TARGET_FILE:extract_swf> "${XMAS_SWF}" "${XMAS_PACKAGE}" ${XMAS_SOUNDS}
  DEPENDS extract_swf "${XMAS_SWF}" ${XMAS_SOUND_FILES}
  COMMENT "Extracting Christmas's Flash movie")
add_custom_target(xmas_package DEPENDS "${XMAS_PACKAGE}/library.json")
add_dependencies(AmateurSurgeonXmas xmas_package)

# AS1's package: its Flash original extracted into port/games/as1 beside its
# own data/ and scripts/ (docs/as1.md), with the user's restored sound
# (music restored/as1): title.mp3 and operation.mp3 in place of the title's
# and the operation's tracks, and every sfx/<linkage>.mp3 in place of the
# sound exported by that name. (A new file there: re-run cmake.)
set(AS1_SWF "${CMAKE_SOURCE_DIR}/../re/flash/as1/i.adultswim.com/adultswim/games2/game-files/amateursurgeon/game.swf")
set(AS1_PACKAGE "${CMAKE_SOURCE_DIR}/games/as1")
set(AS1_MUSIC "${CMAKE_SOURCE_DIR}/../music restored/as1")
set(AS1_SOUNDS --sound "theme_s=${AS1_MUSIC}/title.mp3" --sound "operation_s=${AS1_MUSIC}/operation.mp3")
set(AS1_SOUND_FILES "${AS1_MUSIC}/title.mp3" "${AS1_MUSIC}/operation.mp3")
# Valerie's and Aureola's screams (patients 13 and 26), new sounds: AS2's
# female screams, from its package (the original has male screams only).
foreach(n 1 2 3 4)
  set(female "${CMAKE_SOURCE_DIR}/games/as2/audio/female-${n}.mp3")
  list(APPEND AS1_SOUNDS --sound "femscream${n}_s=${female}")
  list(APPEND AS1_SOUND_FILES "${female}")
endforeach()
file(GLOB AS1_SFX CONFIGURE_DEPENDS "${AS1_MUSIC}/sfx/*.mp3")
foreach(sfx ${AS1_SFX})
  get_filename_component(linkage "${sfx}" NAME_WE)
  list(APPEND AS1_SOUNDS --sound "${linkage}=${sfx}")
  list(APPEND AS1_SOUND_FILES "${sfx}")
endforeach()
add_custom_command(OUTPUT "${AS1_PACKAGE}/library.json"
  COMMAND $<TARGET_FILE:extract_swf> "${AS1_SWF}" "${AS1_PACKAGE}" ${AS1_SOUNDS}
  DEPENDS extract_swf "${AS1_SWF}" ${AS1_SOUND_FILES}
  COMMENT "Extracting AS1's Flash movie")
add_custom_target(as1_package DEPENDS "${AS1_PACKAGE}/library.json")
add_dependencies(AmateurSurgeon as1_package)

# AS2's art from its Flash original (docs/as2.md): its movies extracted into
# port/games/as2 as packages, as Christmas's: assets.swf (the HUD, the
# pointer, the tools' art) into flash/, the shell (the pause and death
# clipboards, the bandages) into flash-shell/, the preloader (the loading
# screen) into flash-preloader/, the cut scenes (the dialogues' animated
# backgrounds, the operation's results) into flash-cutscene/.
set(AS2_FLASH_SRC "${CMAKE_SOURCE_DIR}/../re/flash/as2/i.adultswim.com/adultswim/games2/game-files/amateursurgeon2")
set(AS2_FLASH_PACKAGES)
foreach(pair "media/assets:flash" "core/skin/shell:flash-shell" "core/skin/shell_preloader:flash-preloader"
        "media/cut_scene:flash-cutscene")
  string(REPLACE ":" ";" parts "${pair}")
  list(GET parts 0 swf)
  list(GET parts 1 dir)
  set(out "${CMAKE_SOURCE_DIR}/games/as2/${dir}")
  add_custom_command(OUTPUT "${out}/library.json"
    COMMAND $<TARGET_FILE:extract_swf> "${AS2_FLASH_SRC}/${swf}.swf" "${out}"
    DEPENDS extract_swf "${AS2_FLASH_SRC}/${swf}.swf"
    COMMENT "Extracting AS2's Flash art (${swf}.swf)")
  list(APPEND AS2_FLASH_PACKAGES "${out}/library.json")
endforeach()
add_custom_target(as2_flash DEPENDS ${AS2_FLASH_PACKAGES})
add_dependencies(AmateurSurgeon2 as2_flash)

# AS3's pointer on a PC: the same yellow Flash cursor as AS2's (its `cursor`
# symbol alone, from the same assets.swf) as a package of AS3's own,
# port/games/as3/flash-pointer (docs/as3.md).
set(AS3_POINTER_PACKAGE "${CMAKE_SOURCE_DIR}/games/as3/flash-pointer")
add_custom_command(OUTPUT "${AS3_POINTER_PACKAGE}/library.json"
  COMMAND $<TARGET_FILE:extract_swf> "${AS2_FLASH_SRC}/media/assets.swf" "${AS3_POINTER_PACKAGE}" --only cursor
  DEPENDS extract_swf "${AS2_FLASH_SRC}/media/assets.swf"
  COMMENT "Extracting AS3's pointer (AS2's Flash cursor)")
add_custom_target(as3_pointer DEPENDS "${AS3_POINTER_PACKAGE}/library.json")
add_dependencies(AmateurSurgeon3 as3_pointer)

# Asset tool: renders an SVG with lunasvg, as the game does (the archived
# attic/xmas-ios/tools/xmas_flash.py).
add_executable(svgrender src/tools/svgrender.cpp)
target_include_directories(svgrender PRIVATE third_party)
target_link_libraries(svgrender PRIVATE lunasvg::lunasvg)
if(WIN32)
  target_link_options(svgrender PRIVATE -static)
endif()

# `cmake --build <dir> --target dist`: AS1, as before.
add_custom_target(dist DEPENDS AmateurSurgeon_dist)
