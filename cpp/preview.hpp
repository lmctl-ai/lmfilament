#pragma once
// CPU preview renderer. Filament itself runs on its noop backend (no GPU driver in a static binary), so
// pictures come from this small z-buffer rasterizer: Lambert shading from the scene's lights, base color from
// the material's baseColor parameter, unlit vertex colors for meshes that carry them. It is an approximation
// for documentation and smoke tests, not Filament's physically based renderer.
#include "mesh.hpp"
#include "texture.hpp"
#include <array>
#include <string>
#include <vector>
namespace lmfilament {
// Local transform T*Rz*Ry*Rx*S (degrees), optionally parented to another node (index into PreviewScene::nodes).
struct XForm { std::array<double, 3> t{0, 0, 0}, r{0, 0, 0}, s{1, 1, 1}; int parent = -1; };
// `texture` (owned by the host, may be null) replaces base_color with a sampled albedo; `unlit` draws the albedo as is (shadingModel : unlit).
struct PreviewItem { Mesh mesh; XForm xf; bool visible = true; std::array<float, 3> base_color{0.8f, 0.8f, 0.8f}; const Texture* texture = nullptr; bool unlit = false; };
struct PreviewLight { bool directional = true; std::array<float, 3> color{1, 1, 1}, vec{0, -1, 0}; float intensity = 0; float falloff = 0; };
struct PreviewScene {
  std::vector<PreviewItem> items;
  std::vector<XForm> nodes;
  bool shadows = false;   // shadow map for the first directional light
  std::vector<PreviewLight> lights;
  std::array<float, 3> sky{0.05f, 0.06f, 0.09f};
  float fov_degrees = 45;
  std::array<double, 3> eye{0, 0, 0}, target{0, 0, -1};
};
void render_png(const PreviewScene& scene, int width, int height, const std::string& path);
// 8-bit RGB, row-major, top row first.
void write_png_rgb(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb);
}
