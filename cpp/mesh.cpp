#include "mesh.hpp"
#include <geometry/SurfaceOrientation.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace lmfilament {
namespace {
void quad(Mesh& m, const float p[4][3], const float n[3]) {
  const float a[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]}, b[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
  const float c[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  const bool ccw = c[0] * n[0] + c[1] * n[1] + c[2] * n[2] > 0;  // counter-clockwise seen from outside
  const uint32_t base = static_cast<uint32_t>(m.vertex_count());
  static const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  for (int i = 0; i < 4; i++) {
    m.positions.insert(m.positions.end(), p[i], p[i] + 3);
    m.normals.insert(m.normals.end(), n, n + 3);
    m.uvs.insert(m.uvs.end(), uv[i], uv[i] + 2);
  }
  for (uint32_t i : (ccw ? std::initializer_list<uint32_t>{0, 1, 2, 0, 2, 3} : std::initializer_list<uint32_t>{0, 2, 1, 0, 3, 2})) m.indices.push_back(base + i);
}
}  // namespace
Mesh make_box(float x, float y, float z) {
  const float h[3] = {x / 2, y / 2, z / 2};
  Mesh m;
  // Each face: normal axis n, then in-plane axes (u, v) with u x v = n so winding is CCW from outside.
  static const int faces[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 2, 0}, {1, 0, 2}, {2, 0, 1}, {2, 1, 0}};
  for (int f = 0; f < 6; f++) {
    const int n = faces[f][0], u = faces[f][1], v = faces[f][2];
    const float sign = (f % 2 == 0) ? 1.f : -1.f;
    float p[4][3], nn[3] = {0, 0, 0};
    nn[n] = sign;
    static const float su[4] = {-1, 1, 1, -1}, sv[4] = {-1, -1, 1, 1};
    for (int i = 0; i < 4; i++) {
      p[i][n] = sign * h[n];
      p[i][u] = su[i] * h[u];
      p[i][v] = sv[i] * h[v];
    }
    quad(m, p, nn);
  }
  return m;
}
Mesh make_sphere(float radius, int segments) {
  Mesh m;
  const int rings = segments, slices = segments * 2;
  for (int r = 0; r <= rings; r++) {
    const float v = float(r) / rings, phi = v * float(M_PI);
    for (int s = 0; s <= slices; s++) {
      const float u = float(s) / slices, theta = u * 2 * float(M_PI);
      const float nx = std::sin(phi) * std::cos(theta), ny = std::cos(phi), nz = std::sin(phi) * std::sin(theta);
      m.positions.insert(m.positions.end(), {radius * nx, radius * ny, radius * nz});
      m.normals.insert(m.normals.end(), {nx, ny, nz});
      m.uvs.insert(m.uvs.end(), {u, v});
    }
  }
  for (int r = 0; r < rings; r++)
    for (int s = 0; s < slices; s++) {
      const uint32_t a = r * (slices + 1) + s, b = a + slices + 1;
      for (uint32_t i : {a, a + 1, b, a + 1, b + 1, b}) m.indices.push_back(i);
    }
  return m;
}
Mesh make_plane(float width, float depth) {
  Mesh m;
  const float hx = width / 2, hz = depth / 2;
  const float p[4][3] = {{-hx, 0, hz}, {hx, 0, hz}, {hx, 0, -hz}, {-hx, 0, -hz}};
  const float n[3] = {0, 1, 0};
  quad(m, p, n);
  return m;
}
Mesh make_triangle() {
  Mesh m;
  const double two_pi_3 = 2 * M_PI / 3;
  const float p[3][3] = {{1, 0, 0}, {float(std::cos(two_pi_3)), float(std::sin(two_pi_3)), 0}, {float(std::cos(2 * two_pi_3)), float(std::sin(2 * two_pi_3)), 0}};
  const float uv[3][2] = {{0, 0}, {1, 0}, {0, 1}};
  for (int i = 0; i < 3; i++) {
    m.positions.insert(m.positions.end(), p[i], p[i] + 3);
    m.normals.insert(m.normals.end(), {0, 0, 1});
    m.uvs.insert(m.uvs.end(), uv[i], uv[i] + 2);
  }
  m.colors = {0xffff0000u, 0xff00ff00u, 0xff0000ffu};  // verbatim from upstream hellotriangle.cpp
  m.indices = {0, 1, 2};
  return m;
}
Mesh load_obj(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::vector<float> v, vt, vn;
  Mesh m;
  std::string line;
  auto resolve = [](int idx, size_t n) { return idx > 0 ? size_t(idx - 1) : size_t(long(n) + idx); };
  while (std::getline(in, line)) {
    std::istringstream ls(line); std::string tag; ls >> tag;
    if (tag == "v") { float x, y, z; ls >> x >> y >> z; v.insert(v.end(), {x, y, z}); }
    else if (tag == "vt") { float a, b = 0; ls >> a >> b; vt.insert(vt.end(), {a, b}); }
    else if (tag == "vn") { float x, y, z; ls >> x >> y >> z; vn.insert(vn.end(), {x, y, z}); }
    else if (tag == "f") {
      std::vector<uint32_t> face; std::string tok;
      while (ls >> tok) {
        int a = 0, b = 0, c = 0; char s1;
        std::istringstream ts(tok); ts >> a;
        if (ts.peek() == '/') { ts >> s1; if (ts.peek() != '/') ts >> b; if (ts.peek() == '/') { ts >> s1 >> c; } }
        const size_t vi = resolve(a, v.size() / 3);
        if (a == 0 || vi >= v.size() / 3) throw std::runtime_error("OBJ face index out of range in " + path);
        m.positions.insert(m.positions.end(), v.begin() + 3 * vi, v.begin() + 3 * vi + 3);
        if (c && resolve(c, vn.size() / 3) < vn.size() / 3) { auto i = resolve(c, vn.size() / 3); m.normals.insert(m.normals.end(), vn.begin() + 3 * i, vn.begin() + 3 * i + 3); }
        else m.normals.insert(m.normals.end(), {0, 0, 0});  // filled by flat normals below
        if (b && resolve(b, vt.size() / 2) < vt.size() / 2) { auto i = resolve(b, vt.size() / 2); m.uvs.insert(m.uvs.end(), vt.begin() + 2 * i, vt.begin() + 2 * i + 2); }
        else m.uvs.insert(m.uvs.end(), {0, 0});
        face.push_back(uint32_t(m.vertex_count() - 1));
      }
      for (size_t k = 2; k < face.size(); k++) m.indices.insert(m.indices.end(), {face[0], face[k - 1], face[k]});
    }
  }
  if (m.indices.empty()) throw std::runtime_error("no faces in " + path);
  // Flat normals for corners that had none.
  for (size_t t = 0; t < m.triangle_count(); t++) {
    float* n[3]; const float* p[3];
    for (int k = 0; k < 3; k++) { n[k] = &m.normals[3 * m.indices[3 * t + k]]; p[k] = &m.positions[3 * m.indices[3 * t + k]]; }
    if (n[0][0] != 0 || n[0][1] != 0 || n[0][2] != 0) continue;
    const float a[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]}, b[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
    float c[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    const float len = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    if (len > 0) for (float& x : c) x /= len;
    for (int k = 0; k < 3; k++) for (int j = 0; j < 3; j++) n[k][j] = c[j];
  }
  return m;
}
void compute_tangents(Mesh& m) {
  using namespace filament::math;
  static_assert(sizeof(float3) == 12 && sizeof(float2) == 8 && sizeof(uint3) == 12);
  if (m.vertex_count() == 0 || m.indices.empty()) throw std::runtime_error("mesh is empty");
  filament::geometry::SurfaceOrientation::Builder b;
  b.vertexCount(m.vertex_count())
      .normals(reinterpret_cast<const float3*>(m.normals.data()))
      .uvs(reinterpret_cast<const float2*>(m.uvs.data()))
      .positions(reinterpret_cast<const float3*>(m.positions.data()))
      .triangleCount(m.triangle_count())
      .triangles(reinterpret_cast<const uint3*>(m.indices.data()));
  auto* so = b.build();
  m.tangents.assign(m.vertex_count() * 4, 0);
  so->getQuats(reinterpret_cast<short4*>(m.tangents.data()), m.vertex_count());
  delete so;
}
void write_obj(const std::string& path, const Mesh& m) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot open " + path);
  out << "# lmfilament\n";
  for (size_t i = 0; i < m.vertex_count(); i++) out << "v " << m.positions[3 * i] << ' ' << m.positions[3 * i + 1] << ' ' << m.positions[3 * i + 2] << '\n';
  for (size_t i = 0; i < m.vertex_count(); i++) out << "vt " << m.uvs[2 * i] << ' ' << m.uvs[2 * i + 1] << '\n';
  for (size_t i = 0; i < m.vertex_count(); i++) out << "vn " << m.normals[3 * i] << ' ' << m.normals[3 * i + 1] << ' ' << m.normals[3 * i + 2] << '\n';
  for (size_t t = 0; t < m.triangle_count(); t++) {
    out << 'f';
    for (int k = 0; k < 3; k++) { const auto i = m.indices[3 * t + k] + 1; out << ' ' << i << '/' << i << '/' << i; }
    out << '\n';
  }
  if (!out) throw std::runtime_error("write failed: " + path);
}
}  // namespace lmfilament
