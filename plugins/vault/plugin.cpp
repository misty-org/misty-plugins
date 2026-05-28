#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

namespace {

constexpr const char* kPanelId = "vault.panel";

void open_vault(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (!host.open_panel_in_view(kPanelId, "dock", misty::ViewOpenMode::Tab)) {
        host.open_panel(kPanelId);
    }
}

void render_vault(const MistyRenderContext* ctx, void*) {
    misty::PluginUI ui(ctx);
    ui.text("Vault");
    ui.text_wrapped("Restic-powered backups now live as a plugin. Dock-hosted backup controls will land here.");
    ui.spacing();
    ui.separator();
    ui.spacing();
    ui.text_wrapped("This keeps Vault out of Misty core while preserving a dedicated place for backup workflows.");
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "vault.open";
    command.title = "Open Vault";
    command.default_shortcut = "";
    command.invoke = &open_vault;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Vault";
    panel.default_open = 0;
    panel.window_type = static_cast<int>(misty::WindowType::Panel);
    panel.default_width = 720.0f;
    panel.default_height = 520.0f;
    panel.render = &render_vault;
    return registry.register_panel(panel) ? 1 : 0;
}
