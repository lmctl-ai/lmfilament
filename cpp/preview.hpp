#pragma once
// CPU preview renderer. Filament itself runs on its noop backend (no GPU driver in a static binary), so
// pictures come from this small z-buffer rasterizer: Lambert shading from the scene's lights, base color from
// the material's baseColor parameter, unlit vertex colors for meshes that carry them. It is an approximation
// for documentation and smoke tests, not Filament's physically based renderer.
#include "mesh.hpp"
#include <array>
#include <string>
#include <vector>
namespace lmfilament {
struct PreviewItem { Mesh mesh; std::array<float, 3> translation{0, 0, 0}; std::array<float, 3> base_color{0.8f, 0.8f, 0.8f}; };
struct PreviewLight { bool directional = true; std::array<float, 3> color{1, 1, 1}, vec{0, -1, 0}; float intensity = 0; float falloff = 0; };
struct PreviewScene {
  std::vector<PreviewItem> items;
  std::vector<PreviewLight> lights;
  std::array<float, 3> sky{0.05f, 0.06f, 0.09f};
  float fov_degrees = 45;
  std::array<double, 3> eye{0, 0, 0}, target{0, 0, -1};
};
void render_png(const PreviewScene& scene, int width, int height, const std::string& path);
}
