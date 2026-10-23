#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace lmfilament {
struct Mesh {
  std::vector<float> positions;   // xyz
  std::vector<float> normals;     // xyz
  std::vector<float> uvs;         // uv
  std::vector<uint32_t> indices;  // triangle list
  std::vector<uint32_t> colors;   // optional packed RGBA8 per vertex
  std::vector<int16_t> tangents;  // packed quaternion (short4) per vertex, empty until computed
  size_t vertex_count() const { return positions.size() / 3; }
  size_t triangle_count() const { return indices.size() / 3; }
};
Mesh make_box(float x, float y, float z);
Mesh make_sphere(float radius, int segments);
Mesh make_plane(float width, float depth);
// The upstream hellotriangle triangle: unit circle at 0/120/240 degrees, red/green/blue vertex colors.
Mesh make_triangle();
// Wavefront OBJ (v/vt/vn/f; polygons are fan-triangulated; one output vertex per face corner).
Mesh load_obj(const std::string& path);
// Fills mesh.tangents from normals + uvs + positions with Filament's SurfaceOrientation.
void compute_tangents(Mesh& mesh);
void write_obj(const std::string& path, const Mesh& mesh);
}
