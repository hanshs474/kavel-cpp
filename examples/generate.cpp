// kavel_generate "an isometric coffee shop, pastel palette"
#include <iostream>
#include <string>

#include "kavel/kavel.hpp"

int main(int argc, char** argv) {
    std::string prompt;
    for (int i = 1; i < argc; ++i) prompt += (i > 1 ? " " : "") + std::string(argv[i]);
    if (prompt.empty()) { std::cerr << "usage: kavel_generate \"your prompt\"\n"; return 2; }
    auto r = kavel::generate(prompt);
    if (!r.ok) { std::cerr << kavel::to_string(r.reason) << ": " << r.message << "\n"; return 1; }
    std::cout << r.image.url << "\n";
}
