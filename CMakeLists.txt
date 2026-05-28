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
if(NOT DEFINED MISTY_PRIVATE_PLUGIN_DIR)
    set(MISTY_PRIVATE_PLUGIN_DIR "$ENV{HOME}/.misty/plugins/private" CACHE PATH "Path to the user private plugin directory")
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/manifest.json")
    set(MISTY_PREVIEW_MANAGER_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/preview_manager-manifest.json")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/manifest.json" "${MISTY_PREVIEW_MANAGER_MANIFEST_OUT}" @ONLY)
    add_custom_target(preview_manager_manifest ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_PLUGIN_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MISTY_PREVIEW_MANAGER_MANIFEST_OUT}"
            "${MISTY_PLUGIN_OUTPUT_DIR}/manifest.json"
        DEPENDS preview_manager
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/plugin.json")
    add_custom_target(preview_manager_details ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_PLUGIN_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_manager/plugin.json"
            "${MISTY_PLUGIN_OUTPUT_DIR}/plugin.json"
        DEPENDS preview_manager
    )
endif()

set(MISTY_PANEL_PREVIEW_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins/preview_panel")
set(MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_PANEL_PREVIEW_LIBRARY_NAME "preview_panel${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_PANEL_PREVIEW_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_PANEL_PREVIEW_LIBRARY_NAME}")
set(MISTY_PANEL_PREVIEW_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_panel/manifest.json")
set(MISTY_PANEL_PREVIEW_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/preview_panel-manifest.json")
file(GLOB MISTY_PANEL_PREVIEW_SCENE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_panel/panels/*.cpp"
)

add_library(preview_panel MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_panel/plugin.cpp"
    ${MISTY_PANEL_PREVIEW_SCENE_SOURCES}
)
set_target_properties(preview_panel PROPERTIES
    PREFIX ""
    OUTPUT_NAME "preview_panel"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_PANEL_PREVIEW_VARIANT_OUTPUT_DIR}"
)
target_include_directories(preview_panel PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/preview_panel"
)
target_compile_definitions(preview_panel PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_PANEL_PREVIEW_MANIFEST_IN}" "${MISTY_PANEL_PREVIEW_MANIFEST_OUT}" @ONLY)
add_custom_target(preview_panel_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_PANEL_PREVIEW_MANIFEST_OUT}"
        "${MISTY_PANEL_PREVIEW_OUTPUT_DIR}/manifest.json"
    DEPENDS preview_panel
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
    "${MISTY_HOST_SRC_DIR}/core/manager/theme_manager.cpp"
    "${MISTY_HOST_SRC_DIR}/core/system/util.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/stb_image_impl.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/svg_loader.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/ui_animate.cpp"
    "${MISTY_HOST_SRC_DIR}/core/ui/ui_layout.cpp"
    "${MISTY_HOST_SRC_DIR}/panels/activity/activity_state.cpp"
    "${MISTY_HOST_SRC_DIR}/panels/panel/panel.cpp"
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

set(MISTY_QUICK_CONVERT_OUTPUT_DIR "${MISTY_PRIVATE_PLUGIN_DIR}/quick_convert")
set(MISTY_QUICK_CONVERT_VARIANT_OUTPUT_DIR "${MISTY_QUICK_CONVERT_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_QUICK_CONVERT_LIBRARY_NAME "quick_convert${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_QUICK_CONVERT_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_QUICK_CONVERT_LIBRARY_NAME}")
set(MISTY_QUICK_CONVERT_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/quick_convert/manifest.json")
set(MISTY_QUICK_CONVERT_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/quick_convert-manifest.json")

add_library(quick_convert MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/quick_convert/plugin.cpp"
)
set_target_properties(quick_convert PROPERTIES
    PREFIX ""
    OUTPUT_NAME "quick_convert"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_QUICK_CONVERT_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_QUICK_CONVERT_VARIANT_OUTPUT_DIR}"
)
target_include_directories(quick_convert PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(quick_convert PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_QUICK_CONVERT_MANIFEST_IN}" "${MISTY_QUICK_CONVERT_MANIFEST_OUT}" @ONLY)
add_custom_target(quick_convert_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_QUICK_CONVERT_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_QUICK_CONVERT_MANIFEST_OUT}"
        "${MISTY_QUICK_CONVERT_OUTPUT_DIR}/manifest.json"
    DEPENDS quick_convert
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/quick_convert/plugin.json")
    add_custom_target(quick_convert_details ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_QUICK_CONVERT_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/quick_convert/plugin.json"
            "${MISTY_QUICK_CONVERT_OUTPUT_DIR}/plugin.json"
        DEPENDS quick_convert
    )
endif()

set(MISTY_YTDLP_OUTPUT_DIR "${MISTY_PRIVATE_PLUGIN_DIR}/ytdlp")
set(MISTY_YTDLP_VARIANT_OUTPUT_DIR "${MISTY_YTDLP_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_YTDLP_LIBRARY_NAME "ytdlp${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_YTDLP_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_YTDLP_LIBRARY_NAME}")
set(MISTY_YTDLP_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/ytdlp/manifest.json")
set(MISTY_YTDLP_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/ytdlp-manifest.json")

add_library(ytdlp MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/ytdlp/plugin.cpp"
)
set_target_properties(ytdlp PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ytdlp"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_YTDLP_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_YTDLP_VARIANT_OUTPUT_DIR}"
)
target_include_directories(ytdlp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(ytdlp PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_YTDLP_MANIFEST_IN}" "${MISTY_YTDLP_MANIFEST_OUT}" @ONLY)
add_custom_target(ytdlp_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_YTDLP_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_YTDLP_MANIFEST_OUT}"
        "${MISTY_YTDLP_OUTPUT_DIR}/manifest.json"
    DEPENDS ytdlp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/ytdlp/plugin.json")
    add_custom_target(ytdlp_details ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_YTDLP_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/ytdlp/plugin.json"
            "${MISTY_YTDLP_OUTPUT_DIR}/plugin.json"
        DEPENDS ytdlp
    )
endif()

set(MISTY_THEMES_OUTPUT_DIR "${MISTY_PRIVATE_PLUGIN_DIR}/themes")
set(MISTY_THEMES_VARIANT_OUTPUT_DIR "${MISTY_THEMES_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_THEMES_LIBRARY_NAME "themes${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_THEMES_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_THEMES_LIBRARY_NAME}")
set(MISTY_THEMES_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/themes/manifest.json")
set(MISTY_THEMES_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/themes-manifest.json")

add_library(themes MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/themes/plugin.cpp"
)
set_target_properties(themes PROPERTIES
    PREFIX ""
    OUTPUT_NAME "themes"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_THEMES_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_THEMES_VARIANT_OUTPUT_DIR}"
)
target_include_directories(themes PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(themes PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_THEMES_MANIFEST_IN}" "${MISTY_THEMES_MANIFEST_OUT}" @ONLY)
add_custom_target(themes_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_THEMES_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_THEMES_MANIFEST_OUT}"
        "${MISTY_THEMES_OUTPUT_DIR}/manifest.json"
    DEPENDS themes
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/themes/plugin.json")
    add_custom_target(themes_details ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_THEMES_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/themes/plugin.json"
            "${MISTY_THEMES_OUTPUT_DIR}/plugin.json"
        DEPENDS themes
    )
endif()

set(MISTY_VAULT_OUTPUT_DIR "${MISTY_PRIVATE_PLUGIN_DIR}/vault")
set(MISTY_VAULT_VARIANT_OUTPUT_DIR "${MISTY_VAULT_OUTPUT_DIR}/${MISTY_PLUGIN_VARIANT_DIR}")
set(MISTY_VAULT_LIBRARY_NAME "vault${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(MISTY_VAULT_VARIANT_LIBRARY "${MISTY_PLUGIN_VARIANT_DIR}/${MISTY_VAULT_LIBRARY_NAME}")
set(MISTY_VAULT_MANIFEST_IN "${CMAKE_CURRENT_SOURCE_DIR}/plugins/vault/manifest.json")
set(MISTY_VAULT_MANIFEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/vault-manifest.json")

add_library(vault MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/vault/plugin.cpp"
)
set_target_properties(vault PROPERTIES
    PREFIX ""
    OUTPUT_NAME "vault"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
    LIBRARY_OUTPUT_DIRECTORY "${MISTY_VAULT_VARIANT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${MISTY_VAULT_VARIANT_OUTPUT_DIR}"
)
target_include_directories(vault PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(vault PRIVATE MISTY_PLUGIN_BUILD=1)

configure_file("${MISTY_VAULT_MANIFEST_IN}" "${MISTY_VAULT_MANIFEST_OUT}" @ONLY)
add_custom_target(vault_manifest ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_VAULT_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${MISTY_VAULT_MANIFEST_OUT}"
        "${MISTY_VAULT_OUTPUT_DIR}/manifest.json"
    DEPENDS vault
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/vault/plugin.json")
    add_custom_target(vault_details ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTY_VAULT_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/vault/plugin.json"
            "${MISTY_VAULT_OUTPUT_DIR}/plugin.json"
        DEPENDS vault
    )
endif()
