# The launcher a recompiled title ships beside its game.
#
# Not a library and not built here: each title makes its own, because the
# executable it starts is the title's. A title's CMakeLists calls
#
#     recomp_add_launcher(timesplitters2_launcher timesplitters2_recomp.exe)
#
# or, the same on every host, with the game's own target:
#
#     recomp_add_launcher(gladius_launcher $<TARGET_FILE_NAME:gladius_recomp>)
#
# (add NO_WIDESCREEN for a title where widescreen must not be offered; see
# below. What the one Widescreen switch does for a title that does offer it
# is said with recomp_title_widescreen, src/config/CMakeLists.txt)
#
# and gets the launcher beside the game, reading the game's own XBE for the
# name it shows and the settings file it writes.
#
# Windows, macOS and Linux. launcher.c is the launcher and is the same on
# all three; the window is launcher_win32.c, or launcher_sdl.c (SDL3)
# putting its frames up through launcher_cocoa.m on macOS and an SDL
# renderer on Linux; the text is GDI (launcher_theme_gdi.c), CoreText
# (launcher_theme_coretext.c) or FreeType with fontconfig
# (launcher_theme_freetype.c). Elsewhere, or on a Linux without FreeType's
# or fontconfig's headers, the call is accepted and makes nothing, so a
# title's CMakeLists need not ask which host it is on.

# A title's own look (optional; launcher_theme_brand.inc):
#
#     recomp_add_launcher(turok_launcher turok_recomp.exe
#         BRAND_BACKGROUND assets/launcher/background.jpg   # PNG or JPEG
#         BRAND_LOGO       assets/launcher/logo.png         # PNG with alpha
#         BRAND_ACCENT     B8232B        # Play and the highlights, RRGGBB
#         BRAND_MOTES      5CFF8A        # the motes of light rising, RRGGBB
#         BRAND_TITLE      "Turok Evolution Recompiled"     # the window title
#         BRAND_FOOTER     "Fan project, not affiliated with ..."
#         BRAND_NEWS_TITLE "..." BRAND_NEWS_TEXT "..."
#         BRAND_UPDATE_REPO owner/repo)   # GitHub releases: newer ones are offered
#
# The pictures are compiled into the launcher, so a release carries them
# without carrying files. With them the launcher opens on a front page
# (picture, logo, Play, cards) in front of its tabs.

# Writes <out> defining `name`[] and `name`_size from the bytes of <file>.
function(_recomp_launcher_embed FILE NAME OUT)
    file(READ "${FILE}" _hex HEX)
    string(LENGTH "${_hex}" _len)
    math(EXPR _size "${_len} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
    file(APPEND "${OUT}"
        "const unsigned char ${NAME}[] = {${_bytes}};\n"
        "const unsigned ${NAME}_size = ${_size}u;\n")
endfunction()

function(recomp_add_launcher TARGET GAME_EXE)
    cmake_parse_arguments(_launcher "NO_WIDESCREEN"
        "BRAND_BACKGROUND;BRAND_LOGO;BRAND_ACCENT;BRAND_MOTES;BRAND_TITLE;BRAND_FOOTER;BRAND_NEWS_TITLE;BRAND_NEWS_TEXT;BRAND_UPDATE_REPO"
        "" ${ARGN})
    set(_dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})

    if(WIN32)
        add_executable(${TARGET} WIN32
            ${_dir}/launcher.c
            ${_dir}/launcher_win32.c
            ${_dir}/launcher_theme.c
            ${_dir}/launcher_theme_gdi.c
            ${_dir}/launcher_bindings.c
        )
        target_link_libraries(${TARGET} PRIVATE dxgi dxguid)
        if(COMMAND xboxrecomp_icon_rc)
            xboxrecomp_icon_rc(_icon_rc)        # cmake/title_icon.cmake
            target_sources(${TARGET} PRIVATE "${_icon_rc}")
        endif()
    elseif(APPLE)
        add_executable(${TARGET}
            ${_dir}/launcher.c
            ${_dir}/launcher_sdl.c
            ${_dir}/launcher_cocoa.m
            ${_dir}/launcher_theme.c
            ${_dir}/launcher_theme_coretext.c
            ${_dir}/launcher_bindings.c
        )
        # Objective-C, compiled by the C compiler (clang on any Mac) so a
        # title need not enable another language for one small file.
        set_source_files_properties(${_dir}/launcher_cocoa.m PROPERTIES
            LANGUAGE C COMPILE_OPTIONS "-xobjective-c;-fobjc-arc")
    elseif(UNIX)
        find_package(Freetype QUIET)
        find_package(Fontconfig QUIET)
        if(NOT Freetype_FOUND OR NOT Fontconfig_FOUND)
            message(STATUS "recomp_add_launcher(${TARGET}): no launcher: it needs "
                           "FreeType and fontconfig with their headers")
            return()
        endif()
        add_executable(${TARGET}
            ${_dir}/launcher.c
            ${_dir}/launcher_sdl.c
            ${_dir}/launcher_theme.c
            ${_dir}/launcher_theme_freetype.c
            ${_dir}/launcher_bindings.c
        )
        target_link_libraries(${TARGET} PRIVATE Freetype::Freetype Fontconfig::Fontconfig m)
    else()
        message(STATUS "recomp_add_launcher(${TARGET}): no launcher on this host")
        return()
    endif()

    # A title written when this was Windows only names its game <name>.exe;
    # off Windows that executable is <name>.
    if(NOT WIN32)
        string(REGEX REPLACE "\\.exe$" "" GAME_EXE "${GAME_EXE}")
    endif()

    target_include_directories(${TARGET} PRIVATE ${_dir})
    target_compile_definitions(${TARGET} PRIVATE
        LAUNCHER_GAME_EXE="${GAME_EXE}"
    )
    # The launcher has to find the game's XBE where the game does, or it
    # names the wrong settings file and every choice made in it is lost.
    # The game looks beside itself in game/, then in YOUR_GAME_DIR from
    # its main.c, for YOUR_GAME_XBE_NAME; read those defines rather than
    # repeat them here, so the two cannot drift. They are C string
    # literals, so separators are doubled backslashes: made forward
    # slashes, which Windows accepts.
    set(_main_c "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
    if(EXISTS "${_main_c}")
        file(STRINGS "${_main_c}" _dir_line
             REGEX "^#define[ \t]+YOUR_GAME_DIR[ \t]+\"")
        if(_dir_line MATCHES "\"([^\"]*)\"")
            string(REPLACE "\\\\" "/" _game_dir "${CMAKE_MATCH_1}")
            target_compile_definitions(${TARGET} PRIVATE
                LAUNCHER_GAME_DIR="${_game_dir}")
        endif()
        # The image the game boots: default.xbe unless main.c names one, as
        # a disc whose default.xbe only picks a language does. That one has
        # the title's id but not its name.
        file(STRINGS "${_main_c}" _xbe_line
             REGEX "^#define[ \t]+YOUR_GAME_XBE_NAME[ \t]+\"")
        if(_xbe_line MATCHES "\"([^\"]*)\"")
            target_compile_definitions(${TARGET} PRIVATE
                LAUNCHER_GAME_XBE="${CMAKE_MATCH_1}")
        endif()
        # Re-configure when main.c changes, e.g. after regen_title_main.py.
        set_property(DIRECTORY APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${_main_c}")
    endif()

    # xbox_config carries the settings file's format, so the launcher
    # and the runtime cannot disagree about what they are writing and
    # reading. xbox_input is the pads (recomp_pad.h: SDL3, or XInput),
    # the same code the game reads them with, and brings platform
    # (host_main.h: where this executable is). xbox_net finds the
    # addresses the System Link tab tells a host to give out.
    target_link_libraries(${TARGET} PRIVATE xbox_config xbox_input xbox_net)
    if(WIN32)
        # The rest is in every Windows.
        # ole32 for the open dialog a self-building release asks with.
        target_link_libraries(${TARGET} PRIVATE user32 gdi32 ole32 shell32 uuid winhttp)
    elseif(APPLE)
        target_link_libraries(${TARGET} PRIVATE SDL3::SDL3
            "-framework CoreText" "-framework CoreGraphics"
            "-framework CoreFoundation" "-framework Cocoa"
            "-framework QuartzCore")
    else()
        target_link_libraries(${TARGET} PRIVATE SDL3::SDL3)
    endif()

    # recomp_add_launcher(<target> <exe> NO_WIDESCREEN): a title where
    # widescreen cannot be right hides the Widescreen row, and the
    # launcher writes it off. A 2D fighter is the case: its stage
    # bounds and walls are built for 4:3, so a wider view shows space the
    # fighters cannot reach.
    if(_launcher_NO_WIDESCREEN)
        target_compile_definitions(${TARGET} PRIVATE LAUNCHER_NO_WIDESCREEN=1)
    endif()

    # The title's own look: the pictures compiled in, the rest as strings.
    target_include_directories(${TARGET} PRIVATE ${_dir}/../../third_party/stb)
    if(_launcher_BRAND_BACKGROUND)
        foreach(_k BRAND_BACKGROUND BRAND_LOGO)
            if(_launcher_${_k} AND NOT IS_ABSOLUTE "${_launcher_${_k}}")
                set(_launcher_${_k} "${CMAKE_CURRENT_SOURCE_DIR}/${_launcher_${_k}}")
            endif()
        endforeach()
        foreach(_k ACCENT MOTES)
            if(NOT _launcher_BRAND_${_k})
                set(_launcher_BRAND_${_k} "9BCE1E")
            endif()
        endforeach()
        set(_brand_c "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_brand.c")
        file(WRITE "${_brand_c}"
            "/* Generated by recomp_add_launcher from the title's BRAND_ arguments. */\n")
        _recomp_launcher_embed("${_launcher_BRAND_BACKGROUND}" launcher_brand_background "${_brand_c}")
        set(_deps "${_launcher_BRAND_BACKGROUND}")
        if(_launcher_BRAND_LOGO)
            _recomp_launcher_embed("${_launcher_BRAND_LOGO}" launcher_brand_logo "${_brand_c}")
            list(APPEND _deps "${_launcher_BRAND_LOGO}")
        else()
            file(APPEND "${_brand_c}"
                "const unsigned char launcher_brand_logo[1] = {0};\n"
                "const unsigned launcher_brand_logo_size = 0u;\n")
        endif()
        foreach(_k TITLE FOOTER NEWS_TITLE NEWS_TEXT UPDATE_REPO)
            string(TOLOWER "${_k}" _lk)
            string(REPLACE "\\" "\\\\" _v "${_launcher_BRAND_${_k}}")
            string(REPLACE "\"" "\\\"" _v "${_v}")
            file(APPEND "${_brand_c}" "const char launcher_brand_${_lk}[] = \"${_v}\";\n")
        endforeach()
        file(APPEND "${_brand_c}"
            "const unsigned launcher_brand_accent = 0x${_launcher_BRAND_ACCENT}u;\n"
            "const unsigned launcher_brand_motes = 0x${_launcher_BRAND_MOTES}u;\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_deps})
        target_sources(${TARGET} PRIVATE "${_brand_c}")
        target_compile_definitions(${TARGET} PRIVATE LAUNCHER_HAS_BRAND=1
                                                     LAUNCHER_WINDOW_W=1200)
    endif()
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE
            /utf-8 /W4 $<$<NOT:$<CONFIG:Debug>>:/O2>)
    else()
        target_compile_options(${TARGET} PRIVATE
            -Wall -Wextra -Wno-unused-parameter $<$<NOT:$<CONFIG:Debug>>:-O2>)
    endif()
endfunction()
