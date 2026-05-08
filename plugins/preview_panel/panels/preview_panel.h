#pragma once

#include "misty_plugin.h"

namespace misty::preview_panel {

class PreviewPanel {
public:
    virtual ~PreviewPanel() = default;

    virtual const char* panel_id() const = 0;
    virtual const char* title() const = 0;
    virtual void render(PluginUI& ui, Host& host) const = 0;
};

} // namespace misty::preview_panel
