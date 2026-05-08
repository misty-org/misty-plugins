if(NOT DEFINED MISTY_HOST_SOURCE_DIR)
    set(MISTY_HOST_SOURCE_DIR "/Users/mtccool668/projects/misty/client" CACHE PATH "Path to the Misty host source tree")
endif()

if(NOT DEFINED MISTY_PLUGIN_API_VERSION)
    set(MISTY_PLUGIN_API_VERSION "1.0")
endif()
if(NOT DEFINED MISTY_PLUGIN_BUILD_ID)
    set(MISTY_PLUGIN_BUILD_ID "external-plugins")
endif()
if(NOT DEFINED MISTY_PLUGIN_USER_TRUST_DIR)
    set(MISTY_PLUGIN_USER_TRUST_DIR "")
endif()
if(NOT DEFINED MISTY_REQUIRE_SIGNED_PLUGINS_VALUE)
    set(MISTY_REQUIRE_SIGNED_PLUGINS_VALUE 0)
endif()
if(NOT DEFINED MISTY_HOST_OS)
    if(WIN32)
        set(MISTY_HOST_OS "windows")
    elseif(APPLE)
        set(MISTY_HOST_OS "macos")
    else()
        set(MISTY_HOST_OS "linux")
    endif()
endif()
if(NOT DEFINED MISTY_HOST_ARCH)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64|arm64|ARM64)")
        set(MISTY_HOST_ARCH "arm64")
    else()
        set(MISTY_HOST_ARCH "x86_64")
    endif()
endif()

function(misty_plugins_apply_host_defs target)
    if(COMMAND misty_configure_plugin_host)
        misty_configure_plugin_host(${target})
    else()
        target_compile_definitions(${target}
            PRIVATE
                MISTY_PLUGIN_API_VERSION="${MISTY_PLUGIN_API_VERSION}"
                MISTY_PLUGIN_BUILD_ID="${MISTY_PLUGIN_BUILD_ID}"
                MISTY_PLUGIN_USER_TRUST_DIR="${MISTY_PLUGIN_USER_TRUST_DIR}"
                MISTY_REQUIRE_SIGNED_PLUGINS=${MISTY_REQUIRE_SIGNED_PLUGINS_VALUE}
        )
    endif()
endfunction()

set(MISTY_PLUGIN_VARIANT_DIR "variants/${MISTY_HOST_OS}-${MISTY_HOST_ARCH}")
set(MISTY_HOST_SRC_DIR "${MISTY_HOST_SOURCE_DIR}/src")
set(MISTY_HOST_VENDOR_DIR "${MISTY_HOST_SOURCE_DIR}/vendor")

set(MISTY_PLUGIN_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/preview_manager")
set(MISTY_PLUGIN_VARIANT_OUTPUT_DIR "${MISTY_PLUGIN_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_PREVIEW_MANAGER_LIBRARY_NAME "preview_manager${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_PREVIEW_MANAGER_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_PREVIEW_MANAGER_LIBRARY_NAME}")

add_library(preview_manager MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/plugin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/stb_impl.cpp"
)
set_target_properties(preview_manager PROPERTIES
    PREFIX ""
    OUTPUT_NAME "preview_manager"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_PLUGIN_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_PLUGIN_VARIANT_OUTPUT_DIR}"
)
target_include_directories(preview_manager PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb"
)
target_compile_definitions(preview_manager PRIVATE MISTY_PLUGIN_BUILD=1)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/manifest.json.in")
    set(MISTY_PREVIEW_MANAGER_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/preview_manager-manifest.json")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/manifest.json.in" "${MISTY_PREVIEW_MANAGER_MANIFEST_OUT}" @ONLY)
    add_custom_target(preview_manager_manifest ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_PLUGIN_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MISTY_PREVIEW_MANAGER_MANIFEST_OUT}"
            "${MISTY_PLUGIN_OUTPUT_DIR}/manifest.json"
        DEPENDS preview_manager
    )
endif()

set(MISTY_PANEL_PREVIEW_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/panel_preview")
set(MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_PANEL_PREVIEW_LIBRARY_NAME "panel_preview${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_PANEL_PREVIEW_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_PANEL_PREVIEW_LIBRARY_NAME}")
set(MISTY_PANEL_PREVIEW_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/panel_preview/manifest.json.in")
set(MISTY_PANEL_PREVIEW_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/panel_preview-manifest.json")

add_library(panel_preview MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/panel_preview/plugin.cpp"
)
set_target_properties(panel_preview PROPERTIES
    PREFIX ""
    OUTPUT_NAME "panel_preview"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR}"
)
target_include_directories(panel_preview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(panel_preview PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_PANEL_PREVIEW_MANIFEST_IN}" "${MISTY_PANEL_PREVIEW_MANIFEST_OUT}" @ONLY)
add_custom_target(panel_preview_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_PANEL_PREVIEW_MANIFEST_OUT}"
        "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}/manifest.json"
    DEPENDS panel_preview
)

set(MISTY_PLUGIN_SANDBOX_SOURCES
    "${MISTY_HOST_SRC_DIR}/tools/plugin_sandbox_main.cpp"
    "${MISTY_HOST_SRC_DIR}/core/manager/plugin_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/plugin_signing.cpp"
    "${MISTY_HOST_SRC_DIR}/core/commands/command_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/manager/asset_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/manager/env_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/manager/font_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/manager/settings_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/system/util.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/stb_image_impl.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/svg_loader.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/ui_animate.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/ui_layout.cpp"
    "${MISTY_HOST_SRC_DIR}/panels/activity/activity_state.cpp"
    "${MISTY_HOST_SRC_DIR}/panels/panel.cpp"
    "${MISTY_HOST_SRC_DIR}/views/app_view.cpp"
    "${MISTY_HOST_VENDOR_DIR}/glad/src/glad.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/imgui.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/imgui_draw.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/imgui_tables.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/imgui_widgets.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp"
    "${MISTY_HOST_VENDOR_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)

add_executable(misty_plugin_sandbox ${MISTY_PLUGIN_SANDBOX_SOURCES})
set_target_properties(misty_plugin_sandbox PROPERTIES OUTPUT_NAME "misty-plugin-sandbox")
target_include_directories(misty_plugin_sandbox PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/host"
    "${MISTY_HOST_SRC_DIR}"
    "${MISTY_HOST_VENDOR_DIR}/glad/include"
    "${MISTY_HOST_VENDOR_DIR}/lunasvg/include"
    "${MISTY_HOST_VENDOR_DIR}/stb_image"
    "${MISTY_HOST_VENDOR_DIR}/stb"
    "${MISTY_HOST_VENDOR_DIR}/imgui/backends"
    "${MISTY_HOST_VENDOR_DIR}/imgui"
    "${MISTY_HOST_VENDOR_DIR}/json/single_include"
)
misty_plugins_apply_host_defs(misty_plugin_sandbox)
target_link_libraries(misty_plugin_sandbox PRIVATE
    glfw
    lunasvg
    OpenGL::GL
    OpenSSL::Crypto
    ${CMAKE_DL_LIBS}
)
if(APPLE)
    target_link_libraries(misty_plugin_sandbox PRIVATE
        "-framework CoreGraphics"
        "-framework CoreServices"
        "-framework Cocoa"
        "-framework Security"
    )
elseif(WIN32)
    target_link_libraries(misty_plugin_sandbox PRIVATE ws2_32 dwmapi)
endif()
if(TARGET misty_assets)
    add_dependencies(misty_plugin_sandbox misty_assets)
endif()
