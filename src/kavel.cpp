#include "kavel/kavel.hpp"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace kavel {
namespace {

using json = nlohmann::json;

Result fail(Reason r, const std::string& msg) {
    Result res;
    res.reason = r;
    res.message = "kavel: " + msg;
    return res;
}

std::string anon_id() {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char* hex = "0123456789abcdef";
    std::string s = "cpp-";
    for (int i = 0; i < 16; ++i) s += hex[rng() % 16];
    return s;
}

std::string key_of(const Options& o) {
    if (!o.api_key.empty()) return o.api_key;
    const char* env = std::getenv("KAVEL_API_KEY");
    return env ? env : "";
}

size_t on_body(char* data, size_t size, size_t n, void* out) {
    static_cast<std::string*>(out)->append(data, size * n);
    return size * n;
}

std::string escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += static_cast<char>(c);
        else { char buf[4]; std::snprintf(buf, sizeof buf, "%%%02X", c); out += buf; }
    }
    return out;
}

bool contains(const std::string& s, const char* sub) { return s.find(sub) != std::string::npos; }

// Refusals answer HTTP 200 with code -1 and a human message.
bool request(const std::string& url, const std::vector<std::string>& headers, const std::string* body,
             json& data, Result& err) {
    CURL* curl = curl_easy_init();
    if (!curl) { err = fail(Reason::other, "curl init failed"); return false; }
    std::string raw;
    curl_slist* hs = nullptr;
    for (const auto& h : headers) hs = curl_slist_append(hs, h.c_str());
    if (body) hs = curl_slist_append(hs, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    if (body) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hs);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) { err = fail(Reason::other, curl_easy_strerror(rc)); return false; }

    json env = json::parse(raw, nullptr, false);
    if (env.is_discarded() || !env.is_object()) { err = fail(Reason::other, "unexpected response"); return false; }
    if (env.value("code", -1) != 0) {
        std::string msg = env.value("message", std::string("request refused"));
        std::string low = msg;
        for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        Reason r = contains(low, "invalid api key") ? Reason::auth
                 : contains(low, "insufficient credits") ? Reason::quota
                 : (contains(low, "sign in") || contains(low, "subscription")) ? Reason::sign_in
                 : Reason::other;
        err = fail(r, msg);
        return false;
    }
    data = env.contains("data") && env["data"].is_object() ? env["data"] : json::object();
    return true;
}

Result run(const json& payload, const Options& o) {
    const std::string key = key_of(o);
    std::vector<std::string> auth{key.empty() ? "x-anon-id: " + anon_id() : "Authorization: Bearer " + key};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(o.timeout_seconds > 0 ? o.timeout_seconds : 360);

    json d;
    Result err;
    const std::string body = payload.dump();
    if (!request(o.base_url + "/api/ai/generate", auth, &body, d, err)) return err;
    // The quota wall answers 200 with code 0 and wall: true.
    if (d.value("wall", false)) {
        const std::string why = d.value("reason", std::string());
        if (why == "anon_ip_daily") return fail(Reason::quota, "this machine has used its free credits for today");
        if (why == "anon_unmetered_video") return fail(Reason::sign_in, "this needs a signed-in account");
        return fail(Reason::quota, "the free allowance does not cover this run");
    }
    const std::string task = d.value("id", std::string());
    if (task.empty()) return fail(Reason::other, "the service returned no task id");

    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::seconds(o.poll_seconds > 0 ? o.poll_seconds : 5));
        json p;
        Result perr;
        bool ok;
        if (!key.empty()) {
            const std::string q = json{{"taskId", task}}.dump();
            ok = request(o.base_url + "/api/ai/query", auth, &q, p, perr);
        } else {
            ok = request(o.base_url + "/api/ai/anon-query?taskId=" + escape(task) + "&provider=kie&mediaType=image",
                         auth, nullptr, p, perr);
        }
        // Free runs only start when a poll crosses the end of the queue, so a
        // dropped poll is not a failed job: keep going.
        if (!ok) continue;
        if (p.contains("cleanImages") && p["cleanImages"].is_array() && !p["cleanImages"].empty()) {
            Result r; r.ok = true; r.image = {p["cleanImages"][0].get<std::string>(), false}; return r;
        }
        if (p.contains("images") && p["images"].is_array() && !p["images"].empty()) {
            bool wm = p.contains("watermarked") && p["watermarked"].is_array() && !p["watermarked"].empty()
                      && p["watermarked"][0].is_boolean() && p["watermarked"][0].get<bool>();
            Result r; r.ok = true; r.image = {p["images"][0].get<std::string>(), wm}; return r;
        }
        const std::string st = p.value("status", std::string());
        if (st == "failed" || st == "error") return fail(Reason::rejected, "prompt refused; reword it");
    }
    return fail(Reason::timeout, "no image before the deadline");
}

} // namespace

const char* to_string(Reason r) {
    switch (r) {
        case Reason::none: return "none";
        case Reason::quota: return "quota";
        case Reason::rejected: return "rejected";
        case Reason::sign_in: return "sign_in";
        case Reason::auth: return "auth";
        case Reason::timeout: return "timeout";
        default: return "other";
    }
}

Result generate(const std::string& prompt, const Options& o) {
    if (prompt.find_first_not_of(" \t\n") == std::string::npos) return fail(Reason::other, "prompt is required");
    const std::string model = (!key_of(o).empty() && !o.model.empty()) ? o.model : "kavel-image-v1";
    return run({{"provider", "kie"}, {"mediaType", "image"}, {"model", model}, {"scene", "text-to-image"},
                {"prompt", prompt}, {"options", {{"aspect_ratio", o.aspect_ratio.empty() ? "1:1" : o.aspect_ratio}}}}, o);
}

Result edit(const std::string& source_url, const std::string& instruction, const Options& o) {
    if (source_url.rfind("http://", 0) != 0 && source_url.rfind("https://", 0) != 0)
        return fail(Reason::other, "source_url must be a public http(s) url");
    if (instruction.find_first_not_of(" \t\n") == std::string::npos) return fail(Reason::other, "instruction is required");
    const std::string model = (!key_of(o).empty() && !o.model.empty()) ? o.model : "nano-banana-2-lite";
    return run({{"provider", "kie"}, {"mediaType", "image"}, {"model", model}, {"scene", "image-to-image"},
                {"prompt", instruction}, {"options", {{"image_input", json::array({source_url})}}}}, o);
}

bool credits(int& remaining, int& grant, const Options& o) {
    json d;
    Result err;
    if (!request(o.base_url + "/api/ai/anon-credits", {"x-anon-id: " + anon_id()}, nullptr, d, err)) return false;
    remaining = d.value("remaining", 0);
    grant = d.value("grant", 0);
    return true;
}

} // namespace kavel
