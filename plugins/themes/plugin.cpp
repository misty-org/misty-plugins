#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kPanelId = "themes.panel";

struct ThemeField {
    const char* token;
    const char* label;
    char hex[16] = "#000000";
};

struct ThemesState {
    std::array<ThemeField, 12> fields = {{
        {"window_bg", "Window background", "#111113"},
        {"panel_bg", "Panel background", "#18181B"},
        {"panel_alt_bg", "Elevated panel", "#27272A"},
        {"border", "Border", "#27272A"},
        {"text", "Primary text", "#D4D4D8"},
        {"text_muted", "Muted text", "#71717A"},
        {"accent", "Accent", "#3B82F6"},
        {"accent_hover", "Accent hover", "#2563EB"},
        {"selection", "Selection", "#3B82F659"},
        {"success", "Success", "#29BB88"},
        {"warning", "Warning", "#F7A134"},
        {"error", "Error", "#EF4444"},
    }};
    std::string status_message = "Load a preset or edit token hex values.";
    bool status_is_error = false;
    bool synced_from_host = false;
};

ThemesState g_state;

char hex_digit(unsigned int value) {
    return static_cast<char>(value < 10 ? ('0' + value) : ('A' + (value - 10)));
}

void encode_hex(const float rgba[4], char out[16]) {
    const auto to_byte = [](float value) -> unsigned int {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return static_cast<unsigned int>(clamped * 255.0f + 0.5f);
    };

    const unsigned int r = to_byte(rgba[0]);
    const unsigned int g = to_byte(rgba[1]);
    const unsigned int b = to_byte(rgba[2]);
    const unsigned int a = to_byte(rgba[3]);

    out[0] = '#';
    out[1] = hex_digit((r >> 4) & 0xF);
    out[2] = hex_digit(r & 0xF);
    out[3] = hex_digit((g >> 4) & 0xF);
    out[4] = hex_digit(g & 0xF);
    out[5] = hex_digit((b >> 4) & 0xF);
    out[6] = hex_digit(b & 0xF);
    if (a < 255) {
        out[7] = hex_digit((a >> 4) & 0xF);
        out[8] = hex_digit(a & 0xF);
        out[9] = '\0';
    } else {
        out[7] = '\0';
    }
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

bool parse_hex_color(const char* input, float rgba[4]) {
    if (!input || !rgba) {
        return false;
    }

    std::string value(input);
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), value.end());
    if (!value.empty() && value.front() == '#') {
        value.erase(value.begin());
    }

    if (value.size() != 6 && value.size() != 8) {
        return false;
    }

    auto parse_byte = [&](std::size_t index) -> int {
        const int hi = hex_value(value[index]);
        const int lo = hex_value(value[index + 1]);
        return (hi < 0 || lo < 0) ? -1 : ((hi << 4) | lo);
    };

    const int r = parse_byte(0);
    const int g = parse_byte(2);
    const int b = parse_byte(4);
    const int a = value.size() == 8 ? parse_byte(6) : 255;
    if (r < 0 || g < 0 || b < 0 || a < 0) {
        return false;
    }

    rgba[0] = static_cast<float>(r) / 255.0f;
    rgba[1] = static_cast<float>(g) / 255.0f;
    rgba[2] = static_cast<float>(b) / 255.0f;
    rgba[3] = static_cast<float>(a) / 255.0f;
    return true;
}

void sync_from_host(misty::Host& host) {
    for (auto& field : g_state.fields) {
        float rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        if (host.get_theme_color(field.token, rgba)) {
            encode_hex(rgba, field.hex);
        }
    }
}

void set_status(std::string message, bool is_error) {
    g_state.status_message = std::move(message);
    g_state.status_is_error = is_error;
}

void apply_preset(misty::Host& host, const char* preset_name, const char* label) {
    if (!host.apply_theme_preset(preset_name)) {
        set_status("Could not apply the requested preset.", true);
        host.notify_error("Themes", "Could not apply the requested preset.");
        return;
    }

    sync_from_host(host);
    set_status(std::string("Applied ") + label + ".", false);
    host.notify_success("Themes", g_state.status_message.c_str());
}

void apply_edits(misty::Host& host) {
    for (const auto& field : g_state.fields) {
        float rgba[4] = {};
        if (!parse_hex_color(field.hex, rgba)) {
            set_status(std::string("Invalid color for ") + field.label + ".", true);
            host.notify_error("Themes", g_state.status_message.c_str());
            return;
        }
        if (!host.set_theme_color(field.token, rgba)) {
            set_status(std::string("Could not apply token ") + field.label + ".", true);
            host.notify_error("Themes", g_state.status_message.c_str());
            return;
        }
    }

    set_status("Applied custom theme edits.", false);
    host.notify_success("Themes", "Applied custom theme edits.");
}

void open_panel_command(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    g_state.synced_from_host = false;
    char current_view[64] = {};
    if (host.copy_current_view_id(current_view, sizeof(current_view)) &&
        host.open_panel_in_view(kPanelId, current_view, misty::ViewOpenMode::Tab)) {
        return;
    }

    if (!host.open_panel(kPanelId)) {
        host.notify_error("Themes", "Could not open the Themes panel.");
    }
}

void render_panel(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);

    if (!g_state.synced_from_host) {
        sync_from_host(host);
        g_state.synced_from_host = true;
    }

    ui.text("Themes");
    ui.text_wrapped("Build and apply Misty color themes with named tokens. Start with Gruvbox, Tokyo Night, or Catppuccin, then fine-tune the hex values below.");
    ui.spacing();

    if (ui.button("Apply Gruvbox", 140.0f, 34.0f)) {
        apply_preset(host, "gruvbox-dark", "Gruvbox Dark");
    }
    ui.same_line();
    if (ui.button("Apply Tokyo Night", 160.0f, 34.0f)) {
        apply_preset(host, "tokyo-night", "Tokyo Night");
    }
    ui.same_line();
    if (ui.button("Apply Catppuccin", 160.0f, 34.0f)) {
        apply_preset(host, "catppuccin-mocha", "Catppuccin Mocha");
    }

    ui.spacing();
    if (ui.button("Reset Misty Dark", 160.0f, 34.0f)) {
        apply_preset(host, "misty-dark", "Misty Dark");
    }
    ui.same_line();
    if (ui.button("Reload Current", 140.0f, 34.0f)) {
        sync_from_host(host);
        set_status("Reloaded theme tokens from the host.", false);
    }

    ui.spacing();
    if (ui.begin_child("##themes_tokens", 0.0f, 320.0f, true)) {
        for (auto& field : g_state.fields) {
            ui.text(field.label);
            const std::string input_id = std::string("##") + field.token;
            ui.input_text(input_id.c_str(), field.hex, sizeof(field.hex));
            ui.spacing();
        }
    }
    ui.end_child();

    ui.spacing();
    if (ui.button("Apply Edits", 120.0f, 36.0f)) {
        apply_edits(host);
    }
    ui.same_line();
    ui.text_wrapped("Hex accepts #RRGGBB or #RRGGBBAA.");

    ui.spacing();
    ui.separator();
    ui.spacing();
    ui.text(g_state.status_is_error ? "Status: Error" : "Status: Ready");
    ui.text_wrapped(g_state.status_message.c_str());
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "themes.open";
    command.title = "Open Themes";
    command.default_shortcut = "Primary+Alt+T";
    command.invoke = &open_panel_command;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Themes";
    panel.default_open = 0;
    panel.default_width = 520.0f;
    panel.default_height = 620.0f;
    panel.render = &render_panel;
    return registry.register_panel(panel) ? 1 : 0;
}
