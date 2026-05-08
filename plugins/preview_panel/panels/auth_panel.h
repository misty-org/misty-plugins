#pragma once

#include "preview_panel.h"

namespace misty::preview_panel {

class AuthPanel final : public PreviewPanel {
public:
    static constexpr const char* kSceneId = "preview-panel.auth-panel";

    const char* panel_id() const override { return kSceneId; }
    const char* title() const override { return "Auth Panel"; }
    void render(PluginUI& ui, Host& host) const override;
};

const PreviewPanel& auth_panel();

} // namespace misty::preview_panel
