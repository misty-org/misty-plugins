#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(MISTY_PLUGIN_BUILD)
#    define MISTY_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define MISTY_PLUGIN_EXPORT
#  endif
#else
#  define MISTY_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#define MISTY_PLUGIN_ABI_VERSION 4u

typedef enum MistyNotificationLevel {
    MISTY_NOTIFICATION_INFO = 0,
    MISTY_NOTIFICATION_SUCCESS = 1,
    MISTY_NOTIFICATION_ERROR = 2
} MistyNotificationLevel;

typedef enum MistyWindowType {
    MISTY_WINDOW_TYPE_PANEL = 0,
    MISTY_WINDOW_TYPE_EXTERNAL = 1
} MistyWindowType;

typedef enum MistyViewOpenMode {
    MISTY_VIEW_OPEN_MODE_INLINE = 0,
    MISTY_VIEW_OPEN_MODE_TAB = 1,
    MISTY_VIEW_OPEN_MODE_SPLIT = 2
} MistyViewOpenMode;

typedef struct MistyViewCapabilities {
    int tabs;
    int split;
} MistyViewCapabilities;

typedef struct MistyHostApi {
    uint32_t version;
    int  (*open_panel)(void* host, const char* id);
    int  (*close_panel)(void* host, const char* id);
    int  (*is_panel_open)(void* host, const char* id);
    int  (*invoke_command)(void* host, const char* id);
    int  (*copy_current_view_id)(void* host, char* buffer, size_t size);
    void (*notify)(void* host, int level, const char* title, const char* message);
    uint32_t (*create_texture)(void* host, int width, int height, const unsigned char* rgba_pixels);
    void (*destroy_texture)(void* host, uint32_t texture_id);
    int  (*copy_selected_file_path)(void* host, char* buffer, size_t size);
    void (*set_preview_scene)(void* host, const char* scene_id);
    int  (*get_view_capabilities)(void* host, const char* view_id, MistyViewCapabilities* out_caps);
    int  (*open_panel_in_view)(void* host, const char* panel_id, const char* view_id, int open_mode);
    int  (*get_theme_color)(void* host, const char* token_name, float* out_rgba4);
    int  (*set_theme_color)(void* host, const char* token_name, const float* rgba4);
    int  (*apply_theme_preset)(void* host, const char* preset_name);
} MistyHostApi;

typedef struct MistyUiApi {
    uint32_t version;
    void (*text)(void* ui, const char* t);
    void (*text_wrapped)(void* ui, const char* t);
    int  (*button)(void* ui, const char* label, float width, float height);
    void (*same_line)(void* ui);
    void (*separator)(void* ui);
    void (*spacing)(void* ui);
    void (*image)(void* ui, uint32_t texture_id, float width, float height);
    void (*get_content_region_avail)(void* ui, float* width, float* height);
    int  (*begin_child)(void* ui, const char* id, float width, float height, int border);
    void (*end_child)(void* ui);
    int  (*input_text)(void* ui, const char* label, char* buffer, size_t size);
} MistyUiApi;

typedef struct MistyInvokeContext {
    uint32_t version;
    void* host_handle;
    const MistyHostApi* host_api;
} MistyInvokeContext;

typedef struct MistyRenderContext {
    uint32_t version;
    void* host_handle;
    const MistyHostApi* host_api;
    void* ui_handle;
    const MistyUiApi* ui_api;
} MistyRenderContext;

typedef void (*MistyCommandInvokeFn)(const MistyInvokeContext* ctx, void* user_data);
typedef void (*MistyPanelRenderFn)(const MistyRenderContext* ctx, void* user_data);

typedef struct MistyCommandReg {
    uint32_t version;
    const char* id;
    const char* title;
    const char* default_shortcut;
    MistyCommandInvokeFn invoke;
    void* user_data;
} MistyCommandReg;

typedef struct MistyPanelReg {
    uint32_t version;
    const char* id;
    const char* title;
    int default_open;
    int window_type;
    float default_width;
    float default_height;
    MistyPanelRenderFn render;
    void* user_data;
} MistyPanelReg;

typedef struct MistyRegistryApi {
    uint32_t version;
    int (*register_command)(void* registry, const MistyCommandReg* cmd);
    int (*register_panel)(void* registry, const MistyPanelReg* panel);
} MistyRegistryApi;

typedef struct MistyPluginContext {
    uint32_t version;
    void* host_handle;
    const MistyHostApi* host_api;
    void* registry_handle;
    const MistyRegistryApi* registry_api;
} MistyPluginContext;

MISTY_PLUGIN_EXPORT uint32_t misty_plugin_abi_version(void);
MISTY_PLUGIN_EXPORT int      misty_plugin_register(const MistyPluginContext* ctx);

typedef uint32_t (*MistyPluginAbiVersionFn)(void);
typedef int      (*MistyPluginRegisterFn)(const MistyPluginContext*);

#ifdef __cplusplus
}
#endif
