#define MISTY_PLUGIN_BUILD 1

#include <array>
#include <string>

#include "misty_plugin.h"
#include "panels/auth_panel.h"
#include "panels/template_panel.h"

namespace {

using misty::preview_panel::PreviewPanel;

constexpr const char* kPanelId = "preview-panel.panel";

const auto& preview_panels() {
    static const std::array<const PreviewPanel*, 2> panels = {
        &misty::preview_panel::template_panel(),
        &misty::preview_panel::auth_panel(),
    };
    return panels;
}

std::string& active_scene_id() {
    static std::string scene_id = misty::preview_panel::TemplatePanel::kSceneId;
    return scene_id;
}

const PreviewPanel* find_panel_by_scene_id(const std::string& scene_id) {
    for (const PreviewPanel* panel : preview_panels()) {
        if (panel && scene_id == panel->panel_id()) {
            return panel;
        }
    }
    return nullptr;
}

void open_preview_panel(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (host.valid()) {
        active_scene_id() = misty::preview_panel::TemplatePanel::kSceneId;
        host.open_panel(kPanelId);
    }
}

void render_scene_picker(misty::PluginUI& ui) {
    ui.text("Preview Scenes");
    ui.text_wrapped("These previews render entirely inside the plugin sandbox so you can iterate quickly, then copy the final version into Misty when it looks right.");
    ui.spacing();
    ui.separator();
    ui.spacing();

    for (const PreviewPanel* panel : preview_panels()) {
        if (!panel) {
            continue;
        }
        if (ui.button(panel->title(), 180.0f, 0.0f)) {
            active_scene_id() = panel->panel_id();
        }
    }
}

void render_preview_panel(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);

    ui.text("Preview Panel");
    ui.text_wrapped("Build panel ideas here in the sandbox first. Once the composition looks good, move the final code into Misty proper.");
    ui.spacing();
    ui.separator();
    ui.spacing();

    render_scene_picker(ui);

    if (const PreviewPanel* panel = find_panel_by_scene_id(active_scene_id())) {
        ui.spacing();
        ui.separator();
        ui.spacing();
        panel->render(ui, host);
    }
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "preview-panel.open";
    command.title = "Open Preview Panel";
    command.invoke = &open_preview_panel;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Preview Panel";
    panel.default_open = 1;
    panel.render = &render_preview_panel;
    return registry.register_panel(panel) ? 1 : 0;
}
