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
// Solids of revolution around +Y, centered at the origin. `segments` >= 3 around the axis.
Mesh make_cylinder(float radius, float height, int segments);
Mesh make_cone(float radius, float height, int segments);      // apex at +height/2
Mesh make_capsule(float radius, float length, int segments);   // `length` = straight part between the hemispheres
Mesh make_icosphere(float radius, int subdivisions);
// Capped cylinder of `radius` from p0 to p1, baked in world space (24 segments). Throws when p0 == p1.
Mesh make_tube(const float p0[3], const float p1[3], float radius);
// Flat double-sided annulus in the XZ plane (y = 0).
Mesh make_ring(float inner, float outer, int segments);            // 0..5
// Voxel text from a built-in 5x7 font (A-Z, 0-9 and . , ! ? - : ' / + = ( ) _; lowercase is upper-cased, unknown -> '?').
// Lower-left of the first glyph at the origin, in the XY plane, `height` tall, one pixel deep (+Z).
Mesh make_text(const std::string& text, float height);
// The upstream hellotriangle triangle: unit circle at 0/120/240 degrees, red/green/blue vertex colors.
Mesh make_triangle();
// Wavefront OBJ (v/vt/vn/f; polygons are fan-triangulated; one output vertex per face corner).
Mesh load_obj(const std::string& path);
// Fills mesh.tangents from normals + uvs + positions with Filament's SurfaceOrientation.
void compute_tangents(Mesh& mesh);
void write_obj(const std::string& path, const Mesh& mesh);
}
