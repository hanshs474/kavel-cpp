// Generate and edit AI images from C++ with no API key and no account.
//
//   auto r = kavel::generate("matte black ceramic mug on pale oak, soft window light");
//   if (r.ok) std::cout << r.image.url << "\n";
//   else std::cerr << kavel::to_string(r.reason) << ": " << r.message << "\n";
//
// Without a key this calls the anonymous tier of https://www.kavel.ai, which
// meters a small free allowance against a client id this library invents rather
// than an account you register. Set Options::api_key (or KAVEL_API_KEY) to run
// on your account.
#pragma once

#include <string>

namespace kavel {

enum class Reason { none, quota, rejected, sign_in, auth, timeout, other };

const char* to_string(Reason r);

struct Image {
    std::string url;          // permanent, cacheable CDN link
    bool watermarked = false; // whether a mark was actually drawn
};

struct Result {
    bool ok = false;
    Image image;
    Reason reason = Reason::none;
    std::string message;
};

struct Options {
    std::string aspect_ratio = "1:1"; // "1:1", "16:9", "9:16", "4:3" or "3:4"
    std::string api_key;              // key from https://www.kavel.ai/settings/apikeys
    std::string model;                // engine override, honoured only with a key
    int poll_seconds = 5;
    int timeout_seconds = 360;
    std::string base_url = "https://www.kavel.ai";
};

// Turn a prompt into a new image. Naming the light, the material and the
// composition moves the result far more than adding adjectives.
Result generate(const std::string& prompt, const Options& options = {});

// Rewrite the image at a public url. An edit costs more than the free
// allowance, so without a key this returns Reason::quota before anything is charged.
Result edit(const std::string& source_url, const std::string& instruction, const Options& options = {});

// What a fresh anonymous client id is granted. Free to call.
bool credits(int& remaining, int& grant, const Options& options = {});

} // namespace kavel
