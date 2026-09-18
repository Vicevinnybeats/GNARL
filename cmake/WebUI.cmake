# Builds ui/ (Vite + React + TS) and embeds the result into the plugin binary.
#
# Two-step, on purpose:
#   1. CONFIGURE time - we must know the exact list of files before
#      juce_add_binary_data() is called, so the UI is built (or stubbed) now.
#   2. BUILD time     - a custom target re-runs Vite when ui/src changes, so the
#      normal edit-compile loop still picks UI edits up.
#
# Vite is configured (ui/vite.config.ts) to emit STABLE, UNHASHED filenames.
# Content hashes would change the embedded resource names on every UI edit,
# which the C++ side cannot know about at compile time.

set(GNARL_UI_DIR   "${CMAKE_SOURCE_DIR}/ui")
set(GNARL_UI_DIST  "${GNARL_UI_DIR}/dist")

# Keep this list in sync with ui/vite.config.ts rollupOptions.output.
#
# backdrop.png is the instrument's background ARTWORK, copied through from
# ui/public/assets/ by Vite. It is in this list unconditionally, and the slot
# ships a 1x1 transparent placeholder, so a build with no artwork commissioned
# yet still configures - the UI detects the placeholder and falls back to its
# procedural gradient. See docs/artwork-brief.md.
set(GNARL_UI_FILES
    "index.html"
    "assets/index.js"
    "assets/index.css"
    "assets/backdrop.png")

function(gnarl_find_npm out_var)
    find_program(GNARL_NPM_EXECUTABLE NAMES npm npm.cmd)
    set(${out_var} "${GNARL_NPM_EXECUTABLE}" PARENT_SCOPE)
endfunction()

# Writes placeholder files so that a JUCE-only build (no Node installed, or
# GNARL_BUILD_UI=OFF) still configures and produces a loadable plugin.
function(gnarl_stub_ui)
    file(MAKE_DIRECTORY "${GNARL_UI_DIST}/assets")
    foreach(f IN LISTS GNARL_UI_FILES)
        if (NOT EXISTS "${GNARL_UI_DIST}/${f}")
            if (f MATCHES "\\.png$")
                # A 1x1 transparent PNG, so the resource resolves and the UI's
                # placeholder check leaves the gradient in place.
                file(WRITE "${GNARL_UI_DIST}/${f}" "")
            elseif (f STREQUAL "index.html")
                file(WRITE "${GNARL_UI_DIST}/${f}"
                    "<!doctype html><html><head><meta charset=\"utf-8\">"
                    "<title>GNARL</title><style>html,body{margin:0;background:#0a0a0c;"
                    "color:#b4ff2e;font:12px ui-monospace,monospace;display:grid;"
                    "place-items:center;height:100%}</style></head><body>"
                    "<div>GNARL &mdash; UI not built (run: npm --prefix ui run build)</div>"
                    "</body></html>")
            else()
                file(WRITE "${GNARL_UI_DIST}/${f}" "/* GNARL: UI not built */\n")
            endif()
        endif()
    endforeach()
endfunction()

function(gnarl_build_ui_now)
    gnarl_find_npm(npm)
    if (NOT npm)
        message(WARNING
            "GNARL: npm not found. Embedding placeholder UI. "
            "Install Node 20+ and reconfigure to get the real interface.")
        gnarl_stub_ui()
        return()
    endif()

    if (NOT EXISTS "${GNARL_UI_DIR}/node_modules")
        message(STATUS "GNARL: installing UI dependencies (npm ci)")
        execute_process(
            COMMAND "${npm}" ci
            WORKING_DIRECTORY "${GNARL_UI_DIR}"
            RESULT_VARIABLE install_result)
        if (NOT install_result EQUAL 0)
            message(WARNING "GNARL: npm ci failed (${install_result}); using placeholder UI.")
            gnarl_stub_ui()
            return()
        endif()
    endif()

    message(STATUS "GNARL: building web UI")
    execute_process(
        COMMAND "${npm}" run build
        WORKING_DIRECTORY "${GNARL_UI_DIR}"
        RESULT_VARIABLE build_result)
    if (NOT build_result EQUAL 0)
        message(WARNING "GNARL: UI build failed (${build_result}); using placeholder UI.")
    endif()
    gnarl_stub_ui()  # fills in anything Vite did not emit
endfunction()

# Adds the GnarlWebUI binary-data target and a rebuild target feeding it.
function(gnarl_add_web_ui target_out)
    if (GNARL_BUILD_UI)
        gnarl_build_ui_now()
    else()
        gnarl_stub_ui()
    endif()

    set(abs_files "")
    foreach(f IN LISTS GNARL_UI_FILES)
        list(APPEND abs_files "${GNARL_UI_DIST}/${f}")
    endforeach()

    juce_add_binary_data(GnarlWebUI
        HEADER_NAME GnarlWebUIData.h
        NAMESPACE   GnarlWebUI
        SOURCES     ${abs_files})

    set_target_properties(GnarlWebUI PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE
        FOLDER "GNARL")

    if (GNARL_BUILD_UI)
        gnarl_find_npm(npm)
        if (npm)
            file(GLOB_RECURSE ui_sources CONFIGURE_DEPENDS
                "${GNARL_UI_DIR}/src/*"
                "${GNARL_UI_DIR}/index.html"
                "${GNARL_UI_DIR}/vite.config.ts"
                "${GNARL_UI_DIR}/package.json")
            add_custom_command(
                OUTPUT "${GNARL_UI_DIST}/.gnarl-ui-stamp"
                COMMAND "${npm}" run build
                COMMAND ${CMAKE_COMMAND} -E touch "${GNARL_UI_DIST}/.gnarl-ui-stamp"
                WORKING_DIRECTORY "${GNARL_UI_DIR}"
                DEPENDS ${ui_sources}
                COMMENT "GNARL: rebuilding web UI"
                VERBATIM)
            add_custom_target(GnarlWebUIRebuild
                DEPENDS "${GNARL_UI_DIST}/.gnarl-ui-stamp")
            set_target_properties(GnarlWebUIRebuild PROPERTIES FOLDER "GNARL")
            add_dependencies(GnarlWebUI GnarlWebUIRebuild)
        endif()
    endif()

    set(${target_out} GnarlWebUI PARENT_SCOPE)
endfunction()
