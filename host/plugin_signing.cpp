#include "plugin_signing.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include "misty_plugin.h"
#include "core/system/util.h"
#include "plugin_config.h"

namespace fs = std::filesystem;

namespace misty::core {
namespace {

std::string read_text_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string base64_encode(const unsigned char* data, std::size_t size) {
    if (!data || size == 0) {
        return {};
    }
    std::string out;
    out.resize(4 * ((size + 2) / 3));
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                        data,
                                        static_cast<int>(size));
    if (written <= 0) {
        return {};
    }
    out.resize(static_cast<std::size_t>(written));
    return out;
}

std::vector<unsigned char> base64_decode(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), value.end());
    if (value.empty()) {
        return {};
    }

    std::vector<unsigned char> out((value.size() * 3) / 4 + 3, 0);
    const int decoded = EVP_DecodeBlock(out.data(),
                                        reinterpret_cast<const unsigned char*>(value.data()),
                                        static_cast<int>(value.size()));
    if (decoded < 0) {
        return {};
    }

    std::size_t out_size = static_cast<std::size_t>(decoded);
    while (!value.empty() && value.back() == '=') {
        value.pop_back();
        if (out_size > 0) {
            --out_size;
        }
    }
    out.resize(out_size);
    return out;
}

std::string manifest_payload(const nlohmann::json& manifest) {
    nlohmann::json payload = manifest;
    if (payload.contains("plugin") && payload["plugin"].is_object()) {
        payload["plugin"].erase("signature");
    }
    return payload.dump();
}

std::string file_sha256(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return {};
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    std::array<char, 32768> buffer{};
    while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<std::size_t>(file.gcount())) != 1) {
            EVP_MD_CTX_free(ctx);
            return {};
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    EVP_MD_CTX_free(ctx);
    std::ostringstream out;
    out << std::hex;
    for (unsigned int i = 0; i < hash_len; ++i) {
        out.width(2);
        out.fill('0');
        out << static_cast<int>(hash[i]);
    }
    return out.str();
}

EVP_PKEY* load_private_key(const fs::path& path) {
    FILE* file = std::fopen(path.string().c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    EVP_PKEY* key = PEM_read_PrivateKey(file, nullptr, nullptr, nullptr);
    std::fclose(file);
    return key;
}

EVP_PKEY* load_public_key_from_pem(const std::string& pem) {
    if (pem.empty()) {
        return nullptr;
    }
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) {
        return nullptr;
    }
    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return key;
}

bool sign_bytes(EVP_PKEY* private_key,
                const std::string& payload,
                std::string* signature_base64) {
    if (!private_key || !signature_base64) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    bool ok = false;
    size_t signature_len = 0;
    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, private_key) == 1 &&
        EVP_DigestSign(ctx, nullptr, &signature_len,
                       reinterpret_cast<const unsigned char*>(payload.data()),
                       payload.size()) == 1) {
        std::vector<unsigned char> signature(signature_len);
        if (EVP_DigestSign(ctx, signature.data(), &signature_len,
                           reinterpret_cast<const unsigned char*>(payload.data()),
                           payload.size()) == 1) {
            signature.resize(signature_len);
            *signature_base64 = base64_encode(signature.data(), signature.size());
            ok = !signature_base64->empty();
        }
    }

    EVP_MD_CTX_free(ctx);
    return ok;
}

bool verify_bytes(EVP_PKEY* public_key,
                  const std::string& payload,
                  const std::string& signature_base64) {
    if (!public_key) {
        return false;
    }

    const auto signature = base64_decode(signature_base64);
    if (signature.empty()) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    const bool ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, public_key) == 1 &&
                    EVP_DigestVerify(ctx,
                                     signature.data(),
                                     signature.size(),
                                     reinterpret_cast<const unsigned char*>(payload.data()),
                                     payload.size()) == 1;
    EVP_MD_CTX_free(ctx);
    return ok;
}

std::vector<std::string> trusted_public_keys() {
    std::vector<std::string> keys;
    const std::string embedded = trim_copy(MISTY_PLUGIN_TRUST_PUBKEY_PEM);
    if (!embedded.empty()) {
        keys.push_back(embedded);
    }

    std::vector<fs::path> roots;
    if (const std::string user_trust_dir = trim_copy(MISTY_PLUGIN_USER_TRUST_DIR); !user_trust_dir.empty()) {
        roots.emplace_back(user_trust_dir);
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        roots.emplace_back(fs::path(home) / "misty" / "public" / "keys");
    }

    for (const auto& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
            continue;
        }
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (ec || !entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            if (ext == ".pem" || ext == ".pub") {
                const std::string pem = trim_copy(read_text_file(entry.path()));
                if (!pem.empty()) {
                    keys.push_back(pem);
                }
            }
        }
    }

    return keys;
}

} // namespace

std::string plugin_api_version() {
    return MISTY_PLUGIN_API_VERSION;
}

std::string plugin_build_id() {
    return MISTY_PLUGIN_BUILD_ID;
}

bool plugin_requires_signature() {
    return MISTY_REQUIRE_SIGNED_PLUGINS != 0;
}

std::string sha256_for_file(const fs::path& path) {
    return file_sha256(path);
}

HostPlatform current_host_platform() {
    HostPlatform p;
#ifdef _WIN32
    p.os = "windows";
#elif __APPLE__
    p.os = "macos";
#elif __linux__
    p.os = "linux";
#else
    p.os = "unknown";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    p.arch = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    p.arch = "x86_64";
#else
    p.arch = "unknown";
#endif

#if defined(_MSC_VER)
    p.runtime = "msvc";
#elif defined(_LIBCPP_VERSION)
    p.runtime = "libc++";
#elif defined(__GLIBCXX__)
    p.runtime = "libstdc++";
#else
    p.runtime = "unknown";
#endif

    return p;
}

std::optional<ManifestVariant> select_variant(
    const std::vector<ManifestVariant>& variants,
    const HostPlatform& host) {
    const ManifestVariant* runtime_agnostic = nullptr;
    for (const auto& variant : variants) {
        if (variant.os != host.os || variant.arch != host.arch) {
            continue;
        }
        if (variant.runtime.empty()) {
            if (!runtime_agnostic) {
                runtime_agnostic = &variant;
            }
            continue;
        }
        if (variant.runtime == host.runtime) {
            return variant;
        }
    }
    if (runtime_agnostic) {
        return *runtime_agnostic;
    }
    return std::nullopt;
}

PluginVerificationResult verify_plugin_manifest(nlohmann::json& manifest,
                                                const fs::path&,
                                                const fs::path& library_path,
                                                const ManifestVariant& selected) {
    PluginVerificationResult result;
    const auto plugin_json = manifest.value("plugin", nlohmann::json::object());

    const HostPlatform host = current_host_platform();
    if (!selected.os.empty() && selected.os != host.os) {
        result.diagnostics.push_back("Selected plugin variant OS does not match this host.");
        return result;
    }
    if (!selected.arch.empty() && selected.arch != host.arch) {
        result.diagnostics.push_back("Selected plugin variant architecture does not match this host.");
        return result;
    }

    if (!selected.sha256.empty()) {
        const std::string actual = file_sha256(library_path);
        if (actual.empty()) {
            result.diagnostics.push_back("Failed to compute plugin library SHA-256.");
            return result;
        }
        if (actual != selected.sha256) {
            result.diagnostics.push_back("Plugin library SHA-256 does not match manifest.");
            return result;
        }
        result.hash_verified = true;
    }

    if (!selected.plugin_api_version.empty() &&
        selected.plugin_api_version != plugin_api_version()) {
        result.diagnostics.push_back("Plugin API version does not match this Misty build.");
        return result;
    }
    if (!selected.build_id.empty() && selected.build_id != plugin_build_id()) {
        result.diagnostics.push_back("Plugin build id does not match this Misty build.");
        return result;
    }

    const auto signature = plugin_json.value("signature", nlohmann::json::object());
    if (!signature.is_object() || signature.empty()) {
        return result;
    }

    result.has_signature = true;
    result.signer = trim_copy(signature.value("signer", std::string()));
    const std::string algorithm = trim_copy(signature.value("algorithm", std::string()));
    const std::string value = trim_copy(signature.value("value", std::string()));

    if (algorithm != "ed25519") {
        result.diagnostics.push_back("Unsupported plugin signature algorithm.");
        return result;
    }
    if (!result.hash_verified) {
        result.diagnostics.push_back("Signed plugins must include a matching SHA-256 for each variant.");
        return result;
    }
    if (selected.plugin_api_version.empty() || selected.build_id.empty()) {
        result.diagnostics.push_back("Signed plugin variants must include plugin_api_version and build_id.");
        return result;
    }
    if (value.empty()) {
        result.diagnostics.push_back("Plugin signature value is empty.");
        return result;
    }

    const std::string payload = manifest_payload(manifest);
    const auto keys = trusted_public_keys();
    if (keys.empty()) {
        result.diagnostics.push_back("No trusted plugin public keys are configured.");
        return result;
    }

    for (const auto& pem : keys) {
        EVP_PKEY* key = load_public_key_from_pem(pem);
        if (!key) {
            continue;
        }
        const bool verified = verify_bytes(key, payload, value);
        EVP_PKEY_free(key);
        if (verified) {
            result.signature_verified = true;
            return result;
        }
    }

    result.diagnostics.push_back("Plugin signature verification failed.");
    return result;
}

bool sign_plugin_manifest(const fs::path& plugin_dir,
                          const fs::path& private_key_path,
                          const std::string& signer,
                          std::string* error) {
    const fs::path manifest_path = plugin_dir / "manifest.json";
    if (!fs::exists(manifest_path)) {
        if (error) {
            *error = "manifest.json was not found in the plugin directory.";
        }
        return false;
    }

    nlohmann::json manifest;
    try {
        std::ifstream file(manifest_path);
        file >> manifest;
    } catch (const std::exception& e) {
        if (error) {
            *error = std::string("Failed to parse manifest.json: ") + e.what();
        }
        return false;
    }

    if (!manifest.contains("plugin") || !manifest["plugin"].is_object()) {
        if (error) *error = "Manifest is missing the plugin object.";
        return false;
    }
    auto& plugin_json = manifest["plugin"];

    if (!plugin_json.contains("variants") || !plugin_json["variants"].is_array() ||
        plugin_json["variants"].empty()) {
        if (error) *error = "Manifest.plugin.variants must be a non-empty array.";
        return false;
    }

    for (auto& variant : plugin_json["variants"]) {
        if (!variant.is_object()) {
            if (error) *error = "Each entry in plugin.variants must be an object.";
            return false;
        }
        const std::string library_rel = trim_copy(variant.value("library", std::string()));
        if (library_rel.empty()) {
            if (error) *error = "A variant is missing its library path.";
            return false;
        }
        const fs::path library_path = (plugin_dir / library_rel).lexically_normal();
        if (!fs::exists(library_path)) {
            if (error) *error = "Variant library was not found: " + library_rel;
            return false;
        }
        const std::string sha = file_sha256(library_path);
        if (sha.empty()) {
            if (error) *error = "Failed to compute SHA-256 for variant: " + library_rel;
            return false;
        }
        variant["sha256"] = sha;
    }

    plugin_json["abi_version"] = MISTY_PLUGIN_ABI_VERSION;
    plugin_json.erase("signature");

    EVP_PKEY* private_key = load_private_key(private_key_path);
    if (!private_key) {
        if (error) {
            *error = "Failed to load the private signing key.";
        }
        return false;
    }

    const std::string payload = manifest_payload(manifest);
    std::string signature_base64;
    const bool signed_ok = sign_bytes(private_key, payload, &signature_base64);
    EVP_PKEY_free(private_key);
    if (!signed_ok) {
        if (error) {
            *error = "Failed to sign the plugin manifest.";
        }
        return false;
    }

    manifest["plugin"]["signature"] = {
        {"algorithm", "ed25519"},
        {"signer", signer.empty() ? "local" : signer},
        {"value", signature_base64},
    };

    try {
        std::ofstream file(manifest_path, std::ios::binary | std::ios::trunc);
        file << manifest.dump(2) << '\n';
    } catch (const std::exception& e) {
        if (error) {
            *error = std::string("Failed to write manifest.json: ") + e.what();
        }
        return false;
    }

    return true;
}

} // namespace misty::core
