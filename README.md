# kavel-cpp

Generate AI images from C++ with **no API key and no account**.

```cpp
#include <kavel/kavel.hpp>
#include <iostream>

int main() {
    kavel::Options o;
    o.aspect_ratio = "16:9";
    auto r = kavel::generate("matte black ceramic mug on pale oak, soft window light", o);
    if (r.ok) std::cout << r.image.url << "\n";   // https://cdn.kavel.ai/uploads/kie/image/....webp
    else std::cerr << kavel::to_string(r.reason) << ": " << r.message << "\n";
}
```

Every other image client wants a key from OpenAI, fal or Replicate before it runs once. This one
calls the anonymous tier of [Kavel](https://www.kavel.ai/?utm_source=conan&utm_medium=package), an
online AI image and video studio, with a client id it invents instead of an account you register.
C++17, depends on libcurl and nlohmann_json.

```bash
cmake -B build -DKAVEL_BUILD_EXAMPLES=ON && cmake --build build
./build/kavel_generate "an isometric coffee shop, pastel palette"
```

## Results you can branch on

Nothing throws. `r.reason` is `quota` (wait, or pass a key), `rejected` (reword the prompt),
`sign_in`, `auth`, `timeout` or `other`.

## The free tier

`kavel::credits(remaining, grant)` reads what a fresh client id is granted, and the call costs nothing.
On 2026-10-02 the grant was 5 credits, one generated image spent all five, and one IP got two images that
day. Free output is 1K and watermarked, and free runs queue before the model starts, so the default
deadline is six minutes.

## Editing a photo

```cpp
kavel::Options o;
if (const char* k = std::getenv("KAVEL_API_KEY")) o.api_key = k;
auto r = kavel::edit("https://example.com/portrait.jpg",
                     "shoulder-length layered haircut, keep the same face, skin and lighting", o);
```

An edit costs more than the free grant, so it needs a key from
[kavel.ai/settings/apikeys](https://www.kavel.ai/settings/apikeys?utm_source=conan&utm_medium=package).
Without one it returns `quota` before anything is charged.

## License

MIT. Generated with [Kavel AI](https://www.kavel.ai/?utm_source=conan&utm_medium=package).
