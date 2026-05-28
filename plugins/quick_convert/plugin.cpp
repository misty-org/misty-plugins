#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

constexpr const char* kPanelId = "quick-convert.panel";
constexpr std::size_t kMaxSelectedPath = 4096;

struct ConvertPreset {
    const char* label;
    const char* ext;
    const char* args;
};

enum class MediaKind {
    Unknown,
    Image,
    Audio,
    Video,
};

struct ConvertState {
    char selected_path[kMaxSelectedPath] = {};
    MediaKind kind = MediaKind::Unknown;
    std::string status_message;
    bool status_is_error = false;
    std::string last_output_path;
};

ConvertState g_state;

constexpr std::array<ConvertPreset, 4> kImagePresets = {{
    {"PNG",  "png",  ""},
    {"JPG",  "jpg",  ""},
    {"WEBP", "webp", ""},
    {"AVIF", "avif", ""},
}};

constexpr std::array<ConvertPreset, 4> kAudioPresets = {{
    {"MP3",  "mp3",  "-vn -c:a libmp3lame -q:a 2"},
    {"WAV",  "wav",  "-vn"},
    {"FLAC", "flac", "-vn"},
    {"AAC",  "m4a",  "-vn -c:a aac -b:a 192k"},
}};

constexpr std::array<ConvertPreset, 4> kVideoPresets = {{
    {"MP4",  "mp4",  "-c:v libx264 -crf 23 -preset medium -c:a aac -b:a 192k"},
    {"MOV",  "mov",  "-c:v libx264 -crf 23 -preset medium -c:a aac -b:a 192k"},
    {"WEBM", "webm", "-c:v libvpx-vp9 -crf 32 -b:v 0 -c:a libopus"},
    {"GIF",  "gif",  "-vf fps=10,scale=960:-1:flags=lanczos"},
}};

bool str_iequal(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b))) {
            return false;
        }
    }
    return *a == *b;
}

MediaKind detect_media_kind(const char* path) {
    const char* dot = std::strrchr(path, '.');
    if (!dot) {
        return MediaKind::Unknown;
    }

    const char* ext = dot + 1;
    if (str_iequal(ext, "png") || str_iequal(ext, "jpg") || str_iequal(ext, "jpeg") ||
        str_iequal(ext, "webp") || str_iequal(ext, "gif") || str_iequal(ext, "bmp") ||
        str_iequal(ext, "tiff") || str_iequal(ext, "avif") || str_iequal(ext, "heic")) {
        return MediaKind::Image;
    }
    if (str_iequal(ext, "mp3") || str_iequal(ext, "wav") || str_iequal(ext, "flac") ||
        str_iequal(ext, "m4a") || str_iequal(ext, "aac") || str_iequal(ext, "ogg")) {
        return MediaKind::Audio;
    }
    if (str_iequal(ext, "mp4") || str_iequal(ext, "mov") || str_iequal(ext, "mkv") ||
        str_iequal(ext, "avi") || str_iequal(ext, "webm") || str_iequal(ext, "m4v")) {
        return MediaKind::Video;
    }
    return MediaKind::Unknown;
}

const char* media_kind_label(MediaKind kind) {
    switch (kind) {
        case MediaKind::Image: return "Image";
        case MediaKind::Audio: return "Audio";
        case MediaKind::Video: return "Video";
        default: return "Unknown";
    }
}

std::string shell_quote(const std::string& value) {
#ifdef _WIN32
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '\"') {
            out += "\\\"";
        } else {
            out += ch;
        }
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out += ch;
        }
    }
    out += "'";
    return out;
#endif
}

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string find_ffmpeg() {
    static bool searched = false;
    static std::string cached;
    if (searched) {
        return cached;
    }
    searched = true;

#ifdef _WIN32
    if (std::system("where ffmpeg >nul 2>nul") == 0) {
        cached = "ffmpeg";
        return cached;
    }
#else
    if (std::system("command -v ffmpeg >/dev/null 2>&1") == 0) {
        cached = "ffmpeg";
        return cached;
    }
#endif

#ifdef __APPLE__
    if (std::system("test -x /opt/homebrew/bin/ffmpeg") == 0) {
        cached = "/opt/homebrew/bin/ffmpeg";
        return cached;
    }
    if (std::system("test -x /usr/local/bin/ffmpeg") == 0) {
        cached = "/usr/local/bin/ffmpeg";
        return cached;
    }
#endif

    return cached;
}

std::string selected_filename() {
    return fs::path(g_state.selected_path).filename().string();
}

std::string next_output_path(const char* ext) {
    fs::path input(g_state.selected_path);
    fs::path parent = input.parent_path();
    const std::string stem = input.stem().string();
    fs::path candidate = parent / (stem + "_converted." + ext);
    int suffix = 2;
    while (fs::exists(candidate)) {
        candidate = parent / (stem + "_converted_" + std::to_string(suffix) + "." + ext);
        ++suffix;
    }
    return candidate.string();
}

void set_status(std::string message, bool is_error) {
    g_state.status_message = std::move(message);
    g_state.status_is_error = is_error;
}

void sync_selected_file(misty::Host& host) {
    char selected[kMaxSelectedPath] = {};
    if (!host.copy_selected_file_path(selected, sizeof(selected)) || selected[0] == '\0') {
        return;
    }
    if (std::strcmp(selected, g_state.selected_path) == 0) {
        return;
    }

    std::strncpy(g_state.selected_path, selected, sizeof(g_state.selected_path) - 1);
    g_state.selected_path[sizeof(g_state.selected_path) - 1] = '\0';
    g_state.kind = detect_media_kind(g_state.selected_path);
    g_state.status_message.clear();
    g_state.status_is_error = false;
    g_state.last_output_path.clear();
}

bool run_conversion(misty::Host& host, const ConvertPreset& preset) {
    if (g_state.selected_path[0] == '\0') {
        set_status("Select a file in Files before converting.", true);
        host.notify_error("Quick Convert", "Select a file in Files before converting.");
        return false;
    }

    const std::string ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) {
        set_status("FFmpeg is not installed. Install it first to use Quick Convert.", true);
        host.notify_error(
            "Quick Convert",
            "FFmpeg is required. Install it with your system package manager, then reopen Quick Convert.");
        return false;
    }

    const std::string output = next_output_path(preset.ext);
    std::string cmd = shell_quote(ffmpeg) + " -y -i " + shell_quote(g_state.selected_path);
    if (preset.args && preset.args[0] != '\0') {
        cmd += " ";
        cmd += preset.args;
    }
    cmd += " ";
    cmd += shell_quote(output);
#ifdef _WIN32
    cmd += " >nul 2>&1";
#else
    cmd += " >/dev/null 2>&1";
#endif

    const int code = std::system(cmd.c_str());
    if (code != 0) {
        set_status("Conversion failed. Check that FFmpeg supports the requested output format.", true);
        host.notify_error("Quick Convert", "Conversion failed. Check FFmpeg support and try a different format.");
        return false;
    }

    g_state.last_output_path = output;
    set_status("Created " + fs::path(output).filename().string(), false);
    host.notify_success("Quick Convert", g_state.last_output_path.c_str());
    return true;
}

template <std::size_t N>
void render_presets(misty::PluginUI& ui, misty::Host& host, const std::array<ConvertPreset, N>& presets) {
    for (std::size_t index = 0; index < presets.size(); ++index) {
        const auto& preset = presets[index];
        std::string label = std::string("Convert to ") + preset.label + "##" + preset.ext;
        if (ui.button(label.c_str(), 150.0f, 0.0f)) {
            run_conversion(host, preset);
        }
        if (index % 2 == 0 && index + 1 < presets.size()) {
            ui.same_line();
        }
    }
}

void open_panel_command(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    char current_view[64] = {};
    if (!host.copy_current_view_id(current_view, sizeof(current_view))) {
        host.notify_error("Quick Convert", "Could not determine the current view.");
        return;
    }

    if (!host.open_panel_in_view(kPanelId, current_view, misty::ViewOpenMode::Split)) {
        host.notify_error("Quick Convert", "Could not open the Quick Convert panel.");
    }
}

void render_panel(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);
    sync_selected_file(host);

    ui.text("Quick Convert");
    ui.text_wrapped("Convert the file currently selected in Files using your system FFmpeg install.");
    ui.spacing();

    const std::string ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) {
        ui.text("FFmpeg dependency");
        ui.text_wrapped("FFmpeg was not found on PATH. Install it with your system package manager, then reopen this panel.");
        ui.spacing();
#ifdef __APPLE__
        ui.text("macOS: brew install ffmpeg");
#elif defined(_WIN32)
        ui.text("Windows: install FFmpeg and add it to PATH");
#else
        ui.text("Linux: install ffmpeg with your package manager");
#endif
        ui.spacing();
    }

    if (g_state.selected_path[0] == '\0') {
        ui.text("No file selected.");
        ui.text_wrapped("Select an image, audio, or video file in the Files view to enable conversion presets.");
        return;
    }

    ui.separator();
    ui.spacing();
    ui.text("Selected file");
    ui.text_wrapped(selected_filename().c_str());
    ui.text_wrapped(g_state.selected_path);
    ui.spacing();

    std::string kind_line = std::string("Detected type: ") + media_kind_label(g_state.kind);
    ui.text(kind_line.c_str());

    if (!g_state.last_output_path.empty()) {
        ui.text("Last output");
        ui.text_wrapped(g_state.last_output_path.c_str());
    }
    if (!g_state.status_message.empty()) {
        ui.text(g_state.status_is_error ? "Status: error" : "Status: ready");
        ui.text_wrapped(g_state.status_message.c_str());
    }

    ui.spacing();
    ui.separator();
    ui.spacing();

    if (ffmpeg.empty()) {
        ui.text_wrapped("Install FFmpeg first, then the conversion presets below will work.");
    }

    switch (g_state.kind) {
        case MediaKind::Image:
            ui.text("Image presets");
            ui.text_wrapped("Create a converted copy next to the original file.");
            ui.spacing();
            render_presets(ui, host, kImagePresets);
            break;
        case MediaKind::Audio:
            ui.text("Audio presets");
            ui.text_wrapped("Exports keep the same base filename with a _converted suffix.");
            ui.spacing();
            render_presets(ui, host, kAudioPresets);
            break;
        case MediaKind::Video:
            ui.text("Video presets");
            ui.text_wrapped("Video conversions run through FFmpeg and write beside the source file.");
            ui.spacing();
            render_presets(ui, host, kVideoPresets);
            break;
        default:
            ui.text("Unsupported selection");
            ui.text_wrapped("Quick Convert currently supports common image, audio, and video formats.");
            break;
    }
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "quick-convert.open";
    command.title = "Open Quick Convert";
    command.default_shortcut = "Primary+Shift+C";
    command.invoke = &open_panel_command;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Quick Convert";
    panel.default_open = 0;
    panel.window_type = static_cast<int>(misty::WindowType::Panel);
    panel.default_width = 520.0f;
    panel.default_height = 420.0f;
    panel.render = &render_panel;
    return registry.register_panel(panel) ? 1 : 0;
}
