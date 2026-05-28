#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* kPanelId = "ytdlp.panel";
constexpr std::size_t kMaxUrlLength = 2048;

struct OutputPreset {
    const char* label;
    const char* summary;
    bool audio_only;
    const char* ytdlp_args;
};

enum class OutputPresetId {
    Mp3 = 0,
    M4a = 1,
    Mp4 = 2,
    Webm = 3,
};

enum class DestinationPresetId {
    Smart = 0,
    Downloads = 1,
    Music = 2,
    Movies = 3,
};

struct JobRequest {
    std::string url;
    OutputPresetId preset = OutputPresetId::Mp3;
    DestinationPresetId destination = DestinationPresetId::Smart;
    bool playlist_mode = false;
};

struct PluginState {
    char url[kMaxUrlLength] = {};
    OutputPresetId preset = OutputPresetId::Mp3;
    DestinationPresetId destination = DestinationPresetId::Smart;
    bool playlist_mode = false;

    std::atomic<bool> busy = false;
    std::mutex mu;
    std::string status_message = "Paste a YouTube URL to get started.";
    bool status_is_error = false;
    std::string output_dir;
    std::string output_hint;
    std::string last_log_excerpt;
    bool pending_notification = false;
    bool pending_notification_is_error = false;
    std::string pending_notification_message;
};

PluginState g_state;

constexpr std::array<OutputPreset, 4> kOutputPresets = {{
    {"MP3", "Extract audio as MP3 for music players.", true, "-x --audio-format mp3 --audio-quality 0"},
    {"M4A", "Extract audio as AAC/M4A.", true, "-x --audio-format m4a --audio-quality 0"},
    {"MP4", "Download the best MP4-compatible video.", false, "-f \"bv*[ext=mp4]+ba[ext=m4a]/b[ext=mp4]/b\" --merge-output-format mp4"},
    {"WEBM", "Download a web-optimized video file.", false, "-f \"bv*+ba/b\" --merge-output-format webm"},
}};

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

const OutputPreset& current_preset() {
    return kOutputPresets[static_cast<std::size_t>(g_state.preset)];
}

std::string home_dir() {
    const char* home = std::getenv("HOME");
    return (home && *home) ? std::string(home) : std::string();
}

std::string find_executable_command(const char* bare_name,
                                    const std::vector<std::string>& fallbacks = {}) {
    if (!bare_name || *bare_name == '\0') {
        return {};
    }

#ifdef _WIN32
    std::string where_cmd = std::string("where ") + bare_name + " >nul 2>nul";
#else
    std::string where_cmd = std::string("command -v ") + bare_name + " >/dev/null 2>&1";
#endif
    if (std::system(where_cmd.c_str()) == 0) {
        return bare_name;
    }

    for (const auto& path : fallbacks) {
        if (path.empty()) {
            continue;
        }
#ifdef _WIN32
        if (std::system((std::string("if exist ") + shell_quote(path) + " exit /b 0").c_str()) == 0) {
            return path;
        }
#else
        if (std::system((std::string("test -x ") + shell_quote(path)).c_str()) == 0) {
            return path;
        }
#endif
    }

    return {};
}

std::string find_ytdlp_command() {
    static bool searched = false;
    static std::string cached;
    if (searched) {
        return cached;
    }
    searched = true;

    cached = find_executable_command(
        "yt-dlp",
        {
#ifdef __APPLE__
            "/opt/homebrew/bin/yt-dlp",
            "/usr/local/bin/yt-dlp",
#endif
        });
    if (!cached.empty()) {
        return cached;
    }

#ifdef _WIN32
    if (std::system("python -m yt_dlp --version >nul 2>nul") == 0) {
        cached = "python -m yt_dlp";
        return cached;
    }
#else
    if (std::system("python3 -m yt_dlp --version >/dev/null 2>&1") == 0) {
        cached = "python3 -m yt_dlp";
        return cached;
    }
#endif

    return cached;
}

std::string find_ffmpeg_command() {
    static bool searched = false;
    static std::string cached;
    if (searched) {
        return cached;
    }
    searched = true;

    cached = find_executable_command(
        "ffmpeg",
        {
#ifdef __APPLE__
            "/opt/homebrew/bin/ffmpeg",
            "/usr/local/bin/ffmpeg",
#endif
        });
    return cached;
}

std::string destination_label(DestinationPresetId destination) {
    switch (destination) {
        case DestinationPresetId::Downloads: return "Downloads";
        case DestinationPresetId::Music: return "Music";
        case DestinationPresetId::Movies: return "Movies";
        case DestinationPresetId::Smart:
        default: return "Smart";
    }
}

std::string default_output_dir(const JobRequest& request) {
    const std::string home = home_dir();
    if (home.empty()) {
        return "misty-downloads";
    }

    const bool audio_only = kOutputPresets[static_cast<std::size_t>(request.preset)].audio_only;
    fs::path base;
    switch (request.destination) {
        case DestinationPresetId::Downloads:
            base = fs::path(home) / "Downloads";
            break;
        case DestinationPresetId::Music:
            base = fs::path(home) / "Music";
            break;
        case DestinationPresetId::Movies:
            base = fs::path(home) / "Movies";
            break;
        case DestinationPresetId::Smart:
        default:
            base = audio_only ? fs::path(home) / "Music" : fs::path(home) / "Movies";
            break;
    }

    return (base / "Misty Downloads").string();
}

std::string output_template_for(const std::string& directory) {
    return (fs::path(directory) / "%(title).200B [%(id)s].%(ext)s").string();
}

std::string temp_log_path() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (fs::temp_directory_path() /
            ("misty-ytdlp-" + std::to_string(static_cast<long long>(now)) + ".log")).string();
}

std::string read_tail(const std::string& path, std::size_t max_lines = 12) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) {
        return {};
    }

    const std::size_t start = lines.size() > max_lines ? lines.size() - max_lines : 0;
    std::string result;
    for (std::size_t i = start; i < lines.size(); ++i) {
        const std::string trimmed = trim_copy(lines[i]);
        if (trimmed.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += '\n';
        }
        result += trimmed;
    }
    return result;
}

void set_status(std::string message, bool is_error) {
    std::lock_guard<std::mutex> lock(g_state.mu);
    g_state.status_message = std::move(message);
    g_state.status_is_error = is_error;
}

void queue_notification(bool is_error, std::string message) {
    std::lock_guard<std::mutex> lock(g_state.mu);
    g_state.pending_notification = true;
    g_state.pending_notification_is_error = is_error;
    g_state.pending_notification_message = std::move(message);
}

bool ensure_dependencies_ready() {
    const std::string ytdlp = find_ytdlp_command();
    if (ytdlp.empty()) {
        set_status("yt-dlp was not found. Install yt-dlp to enable downloads.", true);
        queue_notification(true, "yt-dlp is not installed.");
        return false;
    }

    const std::string ffmpeg = find_ffmpeg_command();
    if (ffmpeg.empty()) {
        set_status("FFmpeg was not found. Install FFmpeg so yt-dlp can extract audio and merge video.", true);
        queue_notification(true, "FFmpeg is required for Misty's yt-dlp plugin.");
        return false;
    }

    return true;
}

std::string build_download_command(const JobRequest& request,
                                   const std::string& output_dir,
                                   const std::string& log_path) {
    const std::string ytdlp = find_ytdlp_command();
    const std::string ffmpeg = find_ffmpeg_command();
    const OutputPreset& preset = kOutputPresets[static_cast<std::size_t>(request.preset)];

    std::string cmd = ytdlp;
    cmd += " --newline --no-simulate --no-progress";
    cmd += request.playlist_mode ? " --yes-playlist" : " --no-playlist";
    cmd += " --ffmpeg-location ";
    cmd += shell_quote(ffmpeg);
    cmd += " -o ";
    cmd += shell_quote(output_template_for(output_dir));
    cmd += " ";
    cmd += preset.ytdlp_args;
    cmd += " ";
    cmd += shell_quote(request.url);
#ifdef _WIN32
    cmd += " > ";
    cmd += shell_quote(log_path);
    cmd += " 2>&1";
#else
    cmd += " > ";
    cmd += shell_quote(log_path);
    cmd += " 2>&1";
#endif
    return cmd;
}

void open_directory_in_shell(const std::string& directory) {
    if (directory.empty()) {
        return;
    }

#ifdef _WIN32
    const std::string cmd = "explorer " + shell_quote(directory);
#elif defined(__APPLE__)
    const std::string cmd = "open " + shell_quote(directory);
#else
    const std::string cmd = "xdg-open " + shell_quote(directory) + " >/dev/null 2>&1 &";
#endif
    std::system(cmd.c_str());
}

void start_download_job() {
    if (g_state.busy.exchange(true)) {
        return;
    }

    JobRequest request;
    request.url = trim_copy(g_state.url);
    request.preset = g_state.preset;
    request.destination = g_state.destination;
    request.playlist_mode = g_state.playlist_mode;

    if (request.url.empty()) {
        g_state.busy = false;
        set_status("Paste a YouTube URL before starting a download.", true);
        queue_notification(true, "Paste a YouTube URL before starting a download.");
        return;
    }

    if (!ensure_dependencies_ready()) {
        g_state.busy = false;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_state.mu);
        g_state.output_dir = default_output_dir(request);
        g_state.output_hint = output_template_for(g_state.output_dir);
        g_state.last_log_excerpt.clear();
        g_state.status_message = "Downloading with yt-dlp...";
        g_state.status_is_error = false;
    }

    std::thread([request]() {
        const std::string output_dir = default_output_dir(request);
        std::error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec) {
            set_status("Could not create the output folder.", true);
            queue_notification(true, "Could not create the output folder for yt-dlp downloads.");
            g_state.busy = false;
            return;
        }

        const std::string log_path = temp_log_path();
        const std::string command = build_download_command(request, output_dir, log_path);
        const int exit_code = std::system(command.c_str());
        const std::string tail = read_tail(log_path);
        std::error_code remove_ec;
        fs::remove(log_path, remove_ec);

        {
            std::lock_guard<std::mutex> lock(g_state.mu);
            g_state.output_dir = output_dir;
            g_state.output_hint = output_template_for(output_dir);
            g_state.last_log_excerpt = tail;
        }

        if (exit_code != 0) {
            set_status("Download failed. See the log snippet below for details.", true);
            queue_notification(true, "yt-dlp download failed.");
            g_state.busy = false;
            return;
        }

        set_status("Download finished successfully.", false);
        queue_notification(false, "yt-dlp finished downloading to " + output_dir);
        g_state.busy = false;
    }).detach();
}

void flush_pending_notification(misty::Host& host) {
    std::string message;
    bool is_error = false;

    {
        std::lock_guard<std::mutex> lock(g_state.mu);
        if (!g_state.pending_notification) {
            return;
        }
        g_state.pending_notification = false;
        message = g_state.pending_notification_message;
        is_error = g_state.pending_notification_is_error;
        g_state.pending_notification_message.clear();
    }

    if (message.empty()) {
        return;
    }

    if (is_error) {
        host.notify_error("yt-dlp", message.c_str());
    } else {
        host.notify_success("yt-dlp", message.c_str());
    }
}

void open_panel_command(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    char current_view[64] = {};
    if (host.copy_current_view_id(current_view, sizeof(current_view)) &&
        host.open_panel_in_view(kPanelId, current_view, misty::ViewOpenMode::Tab)) {
        return;
    }

    if (!host.open_panel(kPanelId)) {
        host.notify_error("yt-dlp", "Could not open the yt-dlp panel.");
    }
}

void render_option_button(misty::PluginUI& ui,
                          const char* id_prefix,
                          const char* label,
                          bool selected,
                          float width,
                          bool* activated) {
    std::string button_label = selected
        ? "[" + std::string(label) + "]##" + id_prefix + label
        : std::string(label) + "##" + id_prefix + label;
    if (ui.button(button_label.c_str(), width, 34.0f) && activated) {
        *activated = true;
    }
}

void render_panel(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);

    flush_pending_notification(host);

    ui.text("yt-dlp");
    ui.text_wrapped("Download YouTube videos, playlists, and audio straight from Misty.");
    ui.spacing();

    ui.input_text("Video or playlist URL", g_state.url, sizeof(g_state.url));
    ui.spacing();

    ui.text("Output format");
    for (std::size_t index = 0; index < kOutputPresets.size(); ++index) {
        if (index > 0) {
            ui.same_line();
        }
        bool activated = false;
        render_option_button(
            ui,
            "format_",
            kOutputPresets[index].label,
            static_cast<std::size_t>(g_state.preset) == index,
            82.0f,
            &activated);
        if (activated) {
            g_state.preset = static_cast<OutputPresetId>(index);
        }
    }

    ui.spacing();
    ui.text_wrapped(current_preset().summary);
    ui.spacing();

    ui.text("Save location");
    {
        const std::array<DestinationPresetId, 4> destinations = {
            DestinationPresetId::Smart,
            DestinationPresetId::Downloads,
            DestinationPresetId::Music,
            DestinationPresetId::Movies,
        };
        for (std::size_t index = 0; index < destinations.size(); ++index) {
            if (index > 0) {
                ui.same_line();
            }
            bool activated = false;
            const std::string label = destination_label(destinations[index]);
            render_option_button(
                ui,
                "destination_",
                label.c_str(),
                g_state.destination == destinations[index],
                92.0f,
                &activated);
            if (activated) {
                g_state.destination = destinations[index];
            }
        }
    }

    ui.spacing();
    {
        bool playlist_toggled = false;
        render_option_button(
            ui,
            "playlist_",
            g_state.playlist_mode ? "Playlist mode: On" : "Playlist mode: Off",
            g_state.playlist_mode,
            180.0f,
            &playlist_toggled);
        if (playlist_toggled) {
            g_state.playlist_mode = !g_state.playlist_mode;
        }
    }

    ui.spacing();
    if (ui.button(g_state.busy ? "Downloading..." : "Download", 140.0f, 38.0f) && !g_state.busy.load()) {
        start_download_job();
    }
    ui.same_line();
    if (ui.button("Open Folder", 120.0f, 38.0f)) {
        std::string output_dir;
        {
            std::lock_guard<std::mutex> lock(g_state.mu);
            output_dir = !g_state.output_dir.empty()
                ? g_state.output_dir
                : default_output_dir(JobRequest{
                    trim_copy(g_state.url),
                    g_state.preset,
                    g_state.destination,
                    g_state.playlist_mode,
                });
        }
        open_directory_in_shell(output_dir);
    }
    ui.same_line();
    if (ui.button("Clear URL", 110.0f, 38.0f) && !g_state.busy.load()) {
        g_state.url[0] = '\0';
    }

    std::string status_message;
    std::string output_dir;
    std::string output_hint;
    std::string last_log_excerpt;
    bool status_is_error = false;
    {
        std::lock_guard<std::mutex> lock(g_state.mu);
        status_message = g_state.status_message;
        status_is_error = g_state.status_is_error;
        output_dir = g_state.output_dir;
        output_hint = g_state.output_hint;
        last_log_excerpt = g_state.last_log_excerpt;
    }

    ui.spacing();
    ui.separator();
    ui.spacing();

    if (status_is_error) {
        ui.text("Status: Error");
    } else if (g_state.busy.load()) {
        ui.text("Status: Running");
    } else {
        ui.text("Status: Ready");
    }
    ui.text_wrapped(status_message.c_str());

    if (!output_dir.empty()) {
        ui.spacing();
        ui.text("Output folder");
        ui.text_wrapped(output_dir.c_str());
    }

    if (!output_hint.empty()) {
        ui.spacing();
        ui.text("Filename pattern");
        ui.text_wrapped(output_hint.c_str());
    }

    if (!last_log_excerpt.empty()) {
        ui.spacing();
        ui.text("Last yt-dlp log");
        if (ui.begin_child("##ytdlp_log", 0.0f, 140.0f, true)) {
            ui.text_wrapped(last_log_excerpt.c_str());
        }
        ui.end_child();
    }

    ui.spacing();
    ui.text("Requirements");
    ui.text_wrapped("Install both yt-dlp and FFmpeg on this machine. On macOS, `brew install yt-dlp ffmpeg` is the easiest setup.");
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "ytdlp.open";
    command.title = "Open yt-dlp";
    command.default_shortcut = "Primary+Shift+D";
    command.invoke = &open_panel_command;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "yt-dlp";
    panel.default_open = 0;
    panel.default_width = 680.0f;
    panel.default_height = 560.0f;
    panel.render = &render_panel;
    return registry.register_panel(panel) ? 1 : 0;
}
