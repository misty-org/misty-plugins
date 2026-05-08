#define MISTY_PLUGIN_BUILD 1

#include "misty_plugin.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "stb_image.h"

namespace {

constexpr const char* kPanelId = "preview-manager.panel";
constexpr std::size_t kMaxSelectedPath = 4096;

// ---------------------------------------------------------------------------
// Format detection
// ---------------------------------------------------------------------------

bool str_iequal(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b)))
            return false;
    }
    return *a == *b;
}

enum class FileFormat { Other, Image, Pdf };

FileFormat detect_format(const char* path) {
    const char* dot = std::strrchr(path, '.');
    if (!dot) return FileFormat::Other;

    const char* ext = dot + 1;
    if (str_iequal(ext, "png") || str_iequal(ext, "jpg") ||
        str_iequal(ext, "jpeg") || str_iequal(ext, "bmp") ||
        str_iequal(ext, "gif") || str_iequal(ext, "psd") ||
        str_iequal(ext, "tga") || str_iequal(ext, "hdr") ||
        str_iequal(ext, "pic") || str_iequal(ext, "pnm") ||
        str_iequal(ext, "pgm") || str_iequal(ext, "ppm"))
        return FileFormat::Image;

    if (str_iequal(ext, "pdf"))
        return FileFormat::Pdf;

    return FileFormat::Other;
}

const char* filename_from_path(const char* path) {
    const char* slash = std::strrchr(path, '/');
#ifdef _WIN32
    const char* bslash = std::strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    return slash ? slash + 1 : path;
}

// ---------------------------------------------------------------------------
// PDF support (shells out to mutool)
// ---------------------------------------------------------------------------

constexpr int kMaxPdfPages = 5;
constexpr int kPdfRenderDpi = 150;
constexpr const char* kPdfTempDir = "/tmp/misty_pdf_preview";

std::string shell_quote(const char* s) {
    std::string r = "'";
    for (; *s; ++s) {
        if (*s == '\'') r += "'\\''";
        else r += *s;
    }
    r += "'";
    return r;
}

const char* find_mutool() {
    static const char* cached = nullptr;
    static bool searched = false;
    if (searched) return cached;
    searched = true;

    if (std::system("command -v mutool > /dev/null 2>&1") == 0) {
        cached = "mutool";
        return cached;
    }
#ifdef __APPLE__
    if (std::system("test -x /opt/homebrew/bin/mutool") == 0) {
        cached = "/opt/homebrew/bin/mutool";
        return cached;
    }
    if (std::system("test -x /usr/local/bin/mutool") == 0) {
        cached = "/usr/local/bin/mutool";
        return cached;
    }
#endif
    return nullptr;
}

int pdf_page_count(const char* mutool, const char* path) {
    std::string cmd = std::string(mutool) + " info " +
                      shell_quote(path) + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 0;

    int pages = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), pipe)) {
        if (std::sscanf(line, "Pages: %d", &pages) == 1) break;
    }
    pclose(pipe);
    return pages;
}

bool render_pdf_page_to_file(const char* mutool, const char* pdf,
                              int page_1, const char* out) {
    std::string cmd = std::string(mutool) + " draw -o " + shell_quote(out) +
                      " -F png -r " + std::to_string(kPdfRenderDpi) + " " +
                      shell_quote(pdf) + " " + std::to_string(page_1) +
                      " 2>/dev/null";
    return std::system(cmd.c_str()) == 0;
}

void cleanup_pdf_temp() {
    for (int i = 1; i <= kMaxPdfPages; ++i) {
        char p[512];
        std::snprintf(p, sizeof(p), "%s/page%d.png", kPdfTempDir, i);
        std::remove(p);
    }
    std::remove(kPdfTempDir);
}

// ---------------------------------------------------------------------------
// Binary detection (used by text preview)
// ---------------------------------------------------------------------------

bool is_binary(const char* buffer, size_t size) {
    size_t null_count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] == '\0') null_count++;
    }
    return size > 0 && (double)null_count / size >= 0.3;
}

// ---------------------------------------------------------------------------
// Preview state
// ---------------------------------------------------------------------------

struct PreviewState {
    misty::Host host;
    misty::Texture texture;

    char file_path[512] = {};
    FileFormat format = FileFormat::Other;
    float zoom = 1.0f;
    bool fit_to_width = true;

    int current_page = 0;
    int total_pages = 0;
    int rendered_page = -1;
};

PreviewState g_state;

void reset_preview_state() {
    g_state.texture.destroy();
    g_state.file_path[0] = '\0';
    g_state.format = FileFormat::Other;
    g_state.zoom = 1.0f;
    g_state.fit_to_width = true;
    g_state.current_page = 0;
    g_state.total_pages = 0;
    g_state.rendered_page = -1;
    cleanup_pdf_temp();
}

// ---------------------------------------------------------------------------
// Loaders
// ---------------------------------------------------------------------------

bool load_texture(misty::Host& host, const char* path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) return false;

    g_state.texture = misty::Texture(host, w, h, pixels);
    stbi_image_free(pixels);
    return g_state.texture.valid();
}

bool load_image(misty::Host& host, const char* path) {
    if (!load_texture(host, path)) return false;
    g_state.current_page = 0;
    g_state.total_pages = 0;
    return true;
}

bool load_pdf(misty::Host& host, const char* path) {
    const char* mutool = find_mutool();
    if (!mutool) {
        host.notify_error("Preview",
            "PDF preview requires mupdf-tools.\n"
            "macOS: brew install mupdf\n"
            "Linux: apt install mupdf-tools");
        return false;
    }

    int pages = pdf_page_count(mutool, path);
    if (pages <= 0) return false;

    g_state.total_pages = std::min(pages, kMaxPdfPages);
    g_state.current_page = 0;
    g_state.rendered_page = -1;

    char mkdir_cmd[512];
    std::snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", kPdfTempDir);
    std::system(mkdir_cmd);

    char out_png[512];
    std::snprintf(out_png, sizeof(out_png), "%s/page1.png", kPdfTempDir);
    if (!render_pdf_page_to_file(mutool, path, 1, out_png)) return false;

    if (!load_texture(host, out_png)) return false;
    g_state.rendered_page = 0;
    return true;
}

bool load_other(misty::Host& host, const char* path) {
    (void)host;
    (void)path;
    return false;
}

bool load_file(misty::Host& host, const char* path) {
    FileFormat fmt = detect_format(path);

    reset_preview_state();

    std::strncpy(g_state.file_path, path, sizeof(g_state.file_path) - 1);
    g_state.file_path[sizeof(g_state.file_path) - 1] = '\0';
    g_state.format = fmt;
    g_state.host = host;
    g_state.zoom = 1.0f;
    g_state.fit_to_width = true;

    switch (fmt) {
        case FileFormat::Image:
            return load_image(g_state.host, path);
        case FileFormat::Pdf:
            return load_pdf(g_state.host, path);
        default:
            return load_other(g_state.host, path);
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void open_preview(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (!host.invoke_command("explorer.preview.toggle")) {
        host.notify_error("Preview", "Preview is only available in the Files view.");
        return;
    }
}

void zoom_preview_in(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (!host.invoke_command("explorer.preview.zoom_in")) {
        host.notify_error("Preview", "Preview is only available in the Files view.");
    }
}

void zoom_preview_out(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (!host.invoke_command("explorer.preview.zoom_out")) {
        host.notify_error("Preview", "Preview is only available in the Files view.");
    }
}

void reset_preview_zoom(const MistyInvokeContext* ctx, void*) {
    misty::Host host(ctx);
    if (!host.invoke_command("explorer.preview.zoom_reset")) {
        host.notify_error("Preview", "Preview is only available in the Files view.");
    }
}

void render_preview(const MistyRenderContext* ctx, void*) {
    misty::Host host(ctx);
    misty::PluginUI ui(ctx);

    char selected[kMaxSelectedPath];
    if (host.copy_selected_file_path(selected, sizeof(selected)) &&
        selected[0] != '\0' &&
        std::strcmp(selected, g_state.file_path) != 0) {
        g_state.host = host;
        load_file(g_state.host, selected);
    }

    if (!g_state.texture) {
        ui.text("No preview loaded.");
        ui.spacing();
        ui.text_wrapped(
            "Select a file in the file explorer and press Ctrl+] to preview.");
        ui.spacing();
        ui.text("Supported formats:");
        ui.text("  Images: PNG, JPG, BMP, GIF, PSD, TGA, HDR");
        ui.text("  Documents: PDF (requires mutool)");
        return;
    }

    if (g_state.format == FileFormat::Pdf &&
        g_state.current_page != g_state.rendered_page) {
        const char* mutool = find_mutool();
        if (mutool) {
            g_state.host = misty::Host(ctx);
            char out_png[512];
            std::snprintf(out_png, sizeof(out_png), "%s/page%d.png",
                          kPdfTempDir, g_state.current_page + 1);
            if (render_pdf_page_to_file(mutool, g_state.file_path,
                                         g_state.current_page + 1, out_png)) {
                load_texture(g_state.host, out_png);
                g_state.rendered_page = g_state.current_page;
            }
        }
    }

    // --- Toolbar ---
    const char* name = filename_from_path(g_state.file_path);
    ui.text(name);
    ui.same_line();

    char info[128];
    std::snprintf(info, sizeof(info), "  %dx%d",
                  g_state.texture.width(), g_state.texture.height());
    ui.text(info);

    if (ui.button(" - ##zoom_out", 28.0f)) {
        g_state.fit_to_width = false;
        g_state.zoom = std::max(0.1f, g_state.zoom - 0.1f);
    }
    ui.same_line();

    char zoom_label[32];
    std::snprintf(zoom_label, sizeof(zoom_label), "%d%%",
                  static_cast<int>(g_state.zoom * 100));
    ui.text(zoom_label);
    ui.same_line();

    if (ui.button(" + ##zoom_in", 28.0f)) {
        g_state.fit_to_width = false;
        g_state.zoom = std::min(10.0f, g_state.zoom + 0.1f);
    }
    ui.same_line();

    if (ui.button("Fit##zoom_fit", 40.0f)) {
        g_state.fit_to_width = true;
    }

    if (g_state.total_pages > 1) {
        ui.same_line();
        ui.separator();
        ui.same_line();

        if (ui.button("<##page_prev", 28.0f) && g_state.current_page > 0) {
            g_state.current_page--;
        }
        ui.same_line();

        char page_label[64];
        std::snprintf(page_label, sizeof(page_label), "Page %d / %d",
                      g_state.current_page + 1, g_state.total_pages);
        ui.text(page_label);
        ui.same_line();

        if (ui.button(">##page_next", 28.0f) &&
            g_state.current_page < g_state.total_pages - 1) {
            g_state.current_page++;
        }
    }

    ui.separator();

    // --- Image display ---
    float avail_w = 0, avail_h = 0;
    ui.get_content_region_avail(&avail_w, &avail_h);

    if (g_state.fit_to_width && avail_w > 0 && g_state.texture.width() > 0) {
        g_state.zoom =
            avail_w / static_cast<float>(g_state.texture.width());
    }

    float display_w = g_state.texture.width() * g_state.zoom;
    float display_h = g_state.texture.height() * g_state.zoom;

    ui.begin_child("##preview_canvas");
    ui.image(g_state.texture.id(), display_w, display_h);
    ui.end_child();
}

} // namespace

MISTY_PLUGIN_REGISTER(host, registry) {
    (void)host;

    misty::CommandRegistration command = {};
    command.id = "preview-manager.open";
    command.title = "Preview File";
    command.default_shortcut = "Primary+]";
    command.invoke = &open_preview;
    if (!registry.register_command(command)) {
        return 0;
    }

    command = {};
    command.id = "preview-manager.zoom-in";
    command.title = "Zoom Preview In";
    command.default_shortcut = "Primary+Shift+Equal";
    command.invoke = &zoom_preview_in;
    if (!registry.register_command(command)) {
        return 0;
    }

    command = {};
    command.id = "preview-manager.zoom-out";
    command.title = "Zoom Preview Out";
    command.default_shortcut = "Primary+Minus";
    command.invoke = &zoom_preview_out;
    if (!registry.register_command(command)) {
        return 0;
    }

    command = {};
    command.id = "preview-manager.zoom-reset";
    command.title = "Reset Preview Zoom";
    command.default_shortcut = "Primary+0";
    command.invoke = &reset_preview_zoom;
    if (!registry.register_command(command)) {
        return 0;
    }

    misty::PanelRegistration panel = {};
    panel.id = kPanelId;
    panel.title = "Preview";
    panel.default_open = 0;
    panel.render = &render_preview;
    return registry.register_panel(panel) ? 1 : 0;
}
