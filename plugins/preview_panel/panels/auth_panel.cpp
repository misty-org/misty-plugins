#include "auth_panel.h"

namespace misty::preview_panel {

void AuthPanel::render(PluginUI& ui, Host& host) const {
    (void)host;

    ui.text("Auth Panel");
    ui.text_wrapped("Use this scene to prototype auth-related layouts in the sandbox before moving the final version into Misty.");
    ui.spacing();
    ui.separator();
    ui.spacing();

    ui.text("Starter content");
    ui.text_wrapped("Replace this with your login form, helper text, buttons, and any auth-specific layout experiments.");
    ui.spacing();

    ui.button("Continue", 180.0f, 0.0f);
    ui.spacing();

    ui.text_wrapped("Keep each preview panel self-contained so the selector can render it without extra wiring mistakes.");
}

const PreviewPanel& auth_panel() {
    static const AuthPanel panel;
    return panel;
}

} // namespace misty::preview_panel
