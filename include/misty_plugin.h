#pragma once

#include <cstddef>
#include <cstdint>

#include "misty_plugin_api.h"

namespace misty {

enum class NotificationLevel {
    Info    = MISTY_NOTIFICATION_INFO,
    Success = MISTY_NOTIFICATION_SUCCESS,
    Error   = MISTY_NOTIFICATION_ERROR,
};

// ---------------------------------------------------------------------------
// Inline wrapper around MistyHostApi. Compiled fresh inside the plugin, so
// nothing about this class crosses the .dylib/.dll boundary.
// ---------------------------------------------------------------------------
class Host {
public:
    Host() = default;
    Host(void* handle, const MistyHostApi* api)
        : h_(handle), api_(api) {}
    explicit Host(const MistyInvokeContext* ctx)
        : h_(ctx ? ctx->host_handle : nullptr),
          api_(ctx ? ctx->host_api : nullptr) {}
    explicit Host(const MistyRenderContext* ctx)
        : h_(ctx ? ctx->host_handle : nullptr),
          api_(ctx ? ctx->host_api : nullptr) {}

    bool open_panel(const char* id)    { return api_->open_panel(h_, id) != 0; }
    bool close_panel(const char* id)   { return api_->close_panel(h_, id) != 0; }
    bool is_panel_open(const char* id) { return api_->is_panel_open(h_, id) != 0; }
    bool invoke_command(const char* id){ return api_->invoke_command(h_, id) != 0; }

    bool copy_current_view_id(char* buffer, std::size_t size) {
        return api_->copy_current_view_id(h_, buffer, size) != 0;
    }
    bool copy_selected_file_path(char* buffer, std::size_t size) {
        return api_->copy_selected_file_path(h_, buffer, size) != 0;
    }

    void notify(NotificationLevel level, const char* title, const char* message) {
        api_->notify(h_, static_cast<int>(level), title, message);
    }
    void notify_info(const char* title, const char* message) {
        notify(NotificationLevel::Info, title, message);
    }
    void notify_success(const char* title, const char* message) {
        notify(NotificationLevel::Success, title, message);
    }
    void notify_error(const char* title, const char* message) {
        notify(NotificationLevel::Error, title, message);
    }

    std::uint32_t create_texture(int width, int height, const unsigned char* rgba_pixels) {
        return api_->create_texture(h_, width, height, rgba_pixels);
    }
    void destroy_texture(std::uint32_t texture_id) {
        api_->destroy_texture(h_, texture_id);
    }
    void set_preview_scene(const char* scene_id) {
        api_->set_preview_scene(h_, scene_id);
    }

    void* handle() const { return h_; }
    const MistyHostApi* api() const { return api_; }
    bool valid() const { return h_ != nullptr && api_ != nullptr; }

private:
    void* h_ = nullptr;
    const MistyHostApi* api_ = nullptr;
};

// ---------------------------------------------------------------------------
// Inline wrapper around MistyUiApi.
// ---------------------------------------------------------------------------
class PluginUI {
public:
    PluginUI() = default;
    PluginUI(void* handle, const MistyUiApi* api)
        : u_(handle), api_(api) {}
    explicit PluginUI(const MistyRenderContext* ctx)
        : u_(ctx ? ctx->ui_handle : nullptr),
          api_(ctx ? ctx->ui_api : nullptr) {}

    void text(const char* t)         { api_->text(u_, t); }
    void text_wrapped(const char* t) { api_->text_wrapped(u_, t); }
    bool button(const char* label, float width = 0, float height = 0) {
        return api_->button(u_, label, width, height) != 0;
    }
    void same_line() { api_->same_line(u_); }
    void separator() { api_->separator(u_); }
    void spacing()   { api_->spacing(u_); }

    void image(std::uint32_t texture_id, float width, float height) {
        api_->image(u_, texture_id, width, height);
    }
    void get_content_region_avail(float* width, float* height) {
        api_->get_content_region_avail(u_, width, height);
    }
    bool begin_child(const char* id, float width = 0, float height = 0, bool border = false) {
        return api_->begin_child(u_, id, width, height, border ? 1 : 0) != 0;
    }
    void end_child() { api_->end_child(u_); }

private:
    void* u_ = nullptr;
    const MistyUiApi* api_ = nullptr;
};

// ---------------------------------------------------------------------------
// Inline wrapper around MistyRegistryApi.
// ---------------------------------------------------------------------------
class Registry {
public:
    Registry() = default;
    Registry(void* handle, const MistyRegistryApi* api)
        : r_(handle), api_(api) {}
    explicit Registry(const MistyPluginContext* ctx)
        : r_(ctx ? ctx->registry_handle : nullptr),
          api_(ctx ? ctx->registry_api : nullptr) {}

    bool register_command(const MistyCommandReg& command) {
        return api_->register_command(r_, &command) != 0;
    }
    bool register_panel(const MistyPanelReg& panel) {
        return api_->register_panel(r_, &panel) != 0;
    }

private:
    void* r_ = nullptr;
    const MistyRegistryApi* api_ = nullptr;
};

// ---------------------------------------------------------------------------
// Backwards-compatible C++ aliases for the registration POD types so plugin
// code can write `misty::CommandRegistration` instead of `MistyCommandReg`.
// ---------------------------------------------------------------------------
using CommandRegistration = MistyCommandReg;
using PanelRegistration   = MistyPanelReg;
using CommandInvokeFn     = MistyCommandInvokeFn;
using PanelRenderFn       = MistyPanelRenderFn;

// ---------------------------------------------------------------------------
// RAII GPU texture handle.
// ---------------------------------------------------------------------------
class Texture {
public:
    Texture() = default;
    Texture(Host& host, int width, int height, const unsigned char* rgba_pixels)
        : host_(&host),
          id_(host.create_texture(width, height, rgba_pixels)),
          width_(width),
          height_(height) {}

    ~Texture() { destroy(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept
        : host_(other.host_),
          id_(other.id_),
          width_(other.width_),
          height_(other.height_) {
        other.id_ = 0;
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            destroy();
            host_ = other.host_;
            id_ = other.id_;
            width_ = other.width_;
            height_ = other.height_;
            other.id_ = 0;
        }
        return *this;
    }

    void destroy() {
        if (id_ != 0 && host_) {
            host_->destroy_texture(id_);
            id_ = 0;
        }
    }

    std::uint32_t id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return id_ != 0; }
    explicit operator bool() const { return valid(); }

private:
    Host* host_ = nullptr;
    std::uint32_t id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace misty

// ---------------------------------------------------------------------------
// Convenience macro for the plugin entry point. Plugin authors can write:
//
//     MISTY_PLUGIN_REGISTER(host, registry) {
//         ...
//         return registry.register_command(cmd) ? 1 : 0;
//     }
//
// The macro hides the extern "C" boilerplate and constructs the C++ wrappers
// from the C context for you.
// ---------------------------------------------------------------------------
#define MISTY_PLUGIN_REGISTER(host_var, registry_var)                         \
    static int misty_plugin_register_impl(misty::Host& host_var,              \
                                          misty::Registry& registry_var);     \
    extern "C" MISTY_PLUGIN_EXPORT uint32_t misty_plugin_abi_version(void) {  \
        return MISTY_PLUGIN_ABI_VERSION;                                      \
    }                                                                         \
    extern "C" MISTY_PLUGIN_EXPORT int                                        \
    misty_plugin_register(const MistyPluginContext* ctx) {                    \
        if (!ctx || !ctx->host_api || !ctx->registry_api) return 0;           \
        misty::Host host_var(ctx->host_handle, ctx->host_api);                \
        misty::Registry registry_var(ctx->registry_handle, ctx->registry_api);\
        return misty_plugin_register_impl(host_var, registry_var);            \
    }                                                                         \
    static int misty_plugin_register_impl(misty::Host& host_var,              \
                                          misty::Registry& registry_var)
