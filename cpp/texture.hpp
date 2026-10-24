#pragma once
// 8-bit RGBA images for Material sampler parameters. Filament gets a copy as a filament::Texture; the CPU preview samples this
// copy with a lazily built mip chain (trilinear, or nearest on request) and the three wrap modes.
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace lmfilament {
struct Texture {
  enum class Wrap { Repeat, Clamp, Mirror };
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;                 // row 0 is the top of the image, which is v = 0
  bool linear = true;
  Wrap wrap = Wrap::Repeat;
  float scale_u = 1.f, scale_v = 1.f;        // preview-only UV multiplier
  struct Level { int w = 0, h = 0; std::vector<float> rgb; };
  mutable std::vector<Level> levels;
  mutable bool dirty = true;
  std::array<float, 3> sample(float u, float v, float lod) const;
  int mip_count() const;
  static Texture checker(int size, int cell, const std::array<float, 3>& a, const std::array<float, 3>& b);
};
// PNG, JPEG, BMP or TGA through stb_image; at most 8192 per side. Throws std::runtime_error.
Texture load_texture(const std::string& path);
}
