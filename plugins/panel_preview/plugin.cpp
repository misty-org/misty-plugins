#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

namespace {

constexpr const char* kPanelId = "panel-preview.panel";
constexpr const char* kSessionExpiredSceneId = "panel-preview.session-expired-modal";
constexpr const char* kGenericErrorSceneId = "panel-preview.generic-error-modal";
constexpr const char* kDestructiveConfirmSceneId = "panel-preview.destructive-confirm-modal";
constexpr const char* kLoadingSceneId = "panel-preview.loading-modal";

void open_preview_panel(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (host.valid()) {
        host.set_preview_scene(kSessionExpiredSceneId);
        host.open_panel(kPanelId);
    }
}

void render_session_expired_scene(misty::PluginUI& ui, misty::Host& host) {
    ui.text("Native Scenes");
    ui.text_wrapped("Pick a native Misty modal scene to render as an overlay over this preview panel.");
    ui.spacing();
    ui.separator();
    ui.spacing();

    ui.text("Auth");
    if (ui.button("Session Expired", 180.0f, 0.0f)) {
        host.set_preview_scene(kSessionExpiredSceneId);
    }
    ui.spacing();

    ui.text("Errors");
    if (ui.button("Generic Error", 180.0f, 0.0f)) {
        host.set_preview_scene(kGenericErrorSceneId);
    }
    ui.spacing();

    ui.text("Confirmations");
    if (ui.button("Destructive Confirm", 180.0f, 0.0f)) {
        host.set_preview_scene(kDestructiveConfirmSceneId);
    }
    ui.spacing();

    ui.text("Loading");
    if (ui.button("Loading Modal", 180.0f, 0.0f)) {
        host.set_preview_scene(kLoadingSceneId);
    }
}

void render_preview_panel(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);

    ui.text("Panel Preview");
    ui.text_wrapped(
        "A lightweight development scene for checking panel composition without running a real auth failure."
    );
    ui.spacing();
    ui.separator();
    ui.spacing();

    render_session_expired_scene(ui, host);
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "panel-preview.open";
    command.title = "Open Panel Preview";
    command.invoke = &open_preview_panel;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Panel Preview";
    panel.default_open = 1;
    panel.render = &render_preview_panel;
    return registry.register_panel(panel) ? 1 : 0;
}
