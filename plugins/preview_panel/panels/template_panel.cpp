#include "template_panel.h"

namespace misty::preview_panel {

void TemplatePanel::render(PluginUI& ui, Host& host) const {
    (void)host;

    ui.text("Template");
    ui.text_wrapped("Use this file as the starting point for a new sandbox-only preview scene. Keep the panel fast and simple here, then copy the final implementation into Misty once the layout feels right.");
    ui.spacing();
    ui.separator();
    ui.spacing();

    ui.text("Starter content");
    ui.text_wrapped("Replace this section with your own labels, buttons, inputs, and layout experiments.");
    ui.spacing();

    ui.button("Primary Action", 180.0f, 0.0f);
    ui.spacing();

    ui.text_wrapped("Copy this file, rename the class, and register it in plugin.cpp so it appears in the preview chooser.");
}

const PreviewPanel& template_panel() {
    static const TemplatePanel panel;
    return panel;
}

} // namespace misty::preview_panel
