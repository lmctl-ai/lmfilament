#include "mesh.hpp"
#include <geometry/SurfaceOrientation.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
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

namespace {
struct Pt { float r, y, nr, ny; };  // profile point: radius, height, normal (radial, vertical)
// Revolve profile segments around +Y. Triangles are wound so the geometric normal agrees with the vertex normals.
void revolve(Mesh& m, const std::vector<std::pair<Pt, Pt>>& segs, int segments) {
  const float two_pi = 2 * float(M_PI);
  auto vert = [&](const Pt& p, int k, float v) {
    const float a = two_pi * float(k) / segments, ca = std::cos(a), sa = std::sin(a);
    m.positions.insert(m.positions.end(), {p.r * ca, p.y, p.r * sa});
    float n[3] = {p.nr * ca, p.ny, p.nr * sa};
    m.normals.insert(m.normals.end(), n, n + 3);
    m.uvs.insert(m.uvs.end(), {float(k) / segments, v});
    return uint32_t(m.vertex_count() - 1);
  };
  auto tri = [&](uint32_t a, uint32_t b, uint32_t c) {
    auto P = [&](uint32_t i) { return &m.positions[3 * i]; };
    const float *pa = P(a), *pb = P(b), *pc = P(c);
    const float u[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]}, w[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
    const float g[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
    if (g[0] * g[0] + g[1] * g[1] + g[2] * g[2] < 1e-14f) return;  // degenerate (touches the axis)
    const float* na = &m.normals[3 * a];
    if (g[0] * na[0] + g[1] * na[1] + g[2] * na[2] >= 0) m.indices.insert(m.indices.end(), {a, b, c});
    else m.indices.insert(m.indices.end(), {a, c, b});
  };
  for (const auto& [A, B] : segs)
    for (int k = 0; k < segments; k++) {
      const uint32_t a0 = vert(A, k, 0), a1 = vert(A, k + 1, 0), b0 = vert(B, k, 1), b1 = vert(B, k + 1, 1);
      tri(a0, b0, a1); tri(a1, b0, b1);
    }
}
void check_segments(int s) { if (s < 3 || s > 512) throw std::runtime_error("segments must be between 3 and 512"); }
}  // namespace
Mesh make_cylinder(float r, float h, int s) {
  check_segments(s); Mesh m; const float t = h / 2;
  revolve(m, {{{0, t, 0, 1}, {r, t, 0, 1}}, {{r, t, 1, 0}, {r, -t, 1, 0}}, {{r, -t, 0, -1}, {0, -t, 0, -1}}}, s);
  return m;
}
Mesh make_cone(float r, float h, int s) {
  check_segments(s); Mesh m; const float t = h / 2, len = std::sqrt(h * h + r * r), nr = h / len, ny = r / len;
  revolve(m, {{{0, t, nr, ny}, {r, -t, nr, ny}}, {{r, -t, 0, -1}, {0, -t, 0, -1}}}, s);
  return m;
}
Mesh make_capsule(float r, float length, int s) {
  check_segments(s); Mesh m; const float t = length / 2; const int n = std::max(2, s / 4);
  std::vector<std::pair<Pt, Pt>> segs;
  auto top = [&](int i) { const float a = (float(M_PI) / 2) * float(i) / n; return Pt{r * std::sin(a), t + r * std::cos(a), std::sin(a), std::cos(a)}; };
  for (int i = 0; i < n; i++) segs.push_back({top(i), top(i + 1)});
  segs.push_back({{r, t, 1, 0}, {r, -t, 1, 0}});
  auto bot = [&](int i) { const float a = (float(M_PI) / 2) * float(i) / n; return Pt{r * std::cos(a), -t - r * std::sin(a), std::cos(a), -std::sin(a)}; };
  for (int i = 0; i < n; i++) segs.push_back({bot(i), bot(i + 1)});
  revolve(m, segs, s);
  return m;
}
Mesh make_tube(const float p0[3], const float p1[3], float radius) {
  const float d[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
  const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (len == 0) throw std::runtime_error("tube endpoints must differ");
  Mesh m = make_cylinder(radius, len, 24);
  const float dir[3] = {d[0] / len, d[1] / len, d[2] / len};
  // Rodrigues rotation taking +Y to dir: axis = Y x dir = (dir.z, 0, -dir.x), cos = dir.y.
  const float c = dir[1], ax = dir[2], az = -dir[0], s = std::sqrt(ax * ax + az * az);
  float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  if (s > 1e-6f) {
    const float x = ax / s, z = az / s, k = 1 - c;   // unit axis (x, 0, z)
    const float R2[9] = {c + x * x * k, -z * s, x * z * k,
                         z * s, c, -x * s,
                         x * z * k, x * s, c + z * z * k};
    std::copy(R2, R2 + 9, R);
  } else if (c < 0) {
    const float R2[9] = {1, 0, 0, 0, -1, 0, 0, 0, -1};
    std::copy(R2, R2 + 9, R);
  }
  const float mid[3] = {(p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2};
  auto xf = [&](std::vector<float>& v, bool translate) {
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
      const float x = v[i], y = v[i + 1], z = v[i + 2];
      v[i] = R[0] * x + R[1] * y + R[2] * z + (translate ? mid[0] : 0);
      v[i + 1] = R[3] * x + R[4] * y + R[5] * z + (translate ? mid[1] : 0);
      v[i + 2] = R[6] * x + R[7] * y + R[8] * z + (translate ? mid[2] : 0);
    }
  };
  xf(m.positions, true); xf(m.normals, false);
  return m;
}
Mesh make_ring(float inner, float outer, int segments) {
  check_segments(segments);
  if (!(outer > inner) || inner <= 0) throw std::runtime_error("ring needs 0 < inner < outer");
  Mesh m;
  for (int side = 0; side < 2; side++) {
    const uint32_t base = uint32_t(m.vertex_count());
    const float ny = side ? -1.f : 1.f;
    for (int k = 0; k <= segments; k++) {
      const float a = 2 * float(M_PI) * float(k) / segments, c = std::cos(a), s = std::sin(a);
      for (float r : {inner, outer}) {
        m.positions.insert(m.positions.end(), {r * c, 0.f, r * s});
        m.normals.insert(m.normals.end(), {0.f, ny, 0.f});
        m.uvs.insert(m.uvs.end(), {float(k) / segments, r == inner ? 0.f : 1.f});
      }
    }
    for (int k = 0; k < segments; k++) {
      const uint32_t i = base + 2 * k;
      if (side == 0) m.indices.insert(m.indices.end(), {i, i + 1, i + 2, i + 2, i + 1, i + 3});
      else m.indices.insert(m.indices.end(), {i, i + 2, i + 1, i + 2, i + 3, i + 1});
    }
  }
  return m;
}
Mesh make_icosphere(float radius, int subdivisions) {
  if (subdivisions < 0 || subdivisions > 5) throw std::runtime_error("icosphere subdivisions must be between 0 and 5");
  const float t = (1 + std::sqrt(5.f)) / 2;
  std::vector<std::array<float, 3>> v = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto& p : v) { const float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); for (float& x : p) x /= l; }
  std::vector<std::array<uint32_t, 3>> f = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
  for (int i = 0; i < subdivisions; i++) {
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> cache;
    auto mid = [&](uint32_t a, uint32_t b) {
      auto key = std::minmax(a, b);
      auto it = cache.find({key.first, key.second});
      if (it != cache.end()) return it->second;
      std::array<float, 3> p{(v[a][0] + v[b][0]) / 2, (v[a][1] + v[b][1]) / 2, (v[a][2] + v[b][2]) / 2};
      const float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]); for (float& x : p) x /= l;
      v.push_back(p); return cache[{key.first, key.second}] = uint32_t(v.size() - 1);
    };
    std::vector<std::array<uint32_t, 3>> nf;
    for (auto& tr : f) { const uint32_t a = mid(tr[0], tr[1]), b = mid(tr[1], tr[2]), c = mid(tr[2], tr[0]); nf.push_back({tr[0], a, c}); nf.push_back({tr[1], b, a}); nf.push_back({tr[2], c, b}); nf.push_back({a, b, c}); }
    f = nf;
  }
  Mesh m;
  for (auto& p : v) {
    m.positions.insert(m.positions.end(), {p[0] * radius, p[1] * radius, p[2] * radius});
    m.normals.insert(m.normals.end(), {p[0], p[1], p[2]});
    m.uvs.insert(m.uvs.end(), {0.5f + std::atan2(p[2], p[0]) / (2 * float(M_PI)), 0.5f - std::asin(std::clamp(p[1], -1.f, 1.f)) / float(M_PI)});
  }
  for (auto& tr : f) m.indices.insert(m.indices.end(), tr.begin(), tr.end());
  return m;
}
namespace {
// 5x7 glyphs, rows top to bottom, '#' = lit.
struct Glyph { char c; const char* rows[7]; };
const Glyph kFont[] = {
 {'A',{".###.","#...#","#...#","#####","#...#","#...#","#...#"}},{'B',{"####.","#...#","#...#","####.","#...#","#...#","####."}},
 {'C',{".####","#....","#....","#....","#....","#....",".####"}},{'D',{"####.","#...#","#...#","#...#","#...#","#...#","####."}},
 {'E',{"#####","#....","#....","####.","#....","#....","#####"}},{'F',{"#####","#....","#....","####.","#....","#....","#...."}},
 {'G',{".####","#....","#....","#.###","#...#","#...#",".###."}},{'H',{"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
 {'I',{"#####","..#..","..#..","..#..","..#..","..#..","#####"}},{'J',{"..###","...#.","...#.","...#.","...#.","#..#.",".##.."}},
 {'K',{"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},{'L',{"#....","#....","#....","#....","#....","#....","#####"}},
 {'M',{"#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},{'N',{"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
 {'O',{".###.","#...#","#...#","#...#","#...#","#...#",".###."}},{'P',{"####.","#...#","#...#","####.","#....","#....","#...."}},
 {'Q',{".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#"}},{'R',{"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
 {'S',{".####","#....","#....",".###.","....#","....#","####."}},{'T',{"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
 {'U',{"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},{'V',{"#...#","#...#","#...#","#...#","#...#",".#.#.","..#.."}},
 {'W',{"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},{'X',{"#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"}},
 {'Y',{"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},{'Z',{"#####","....#","...#.","..#..",".#...","#....","#####"}},
 {'0',{".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},{'1',{"..#..",".##..","..#..","..#..","..#..","..#..",".###."}},
 {'2',{".###.","#...#","....#","...#.","..#..",".#...","#####"}},{'3',{"####.","....#","....#",".###.","....#","....#","####."}},
 {'4',{"...#.","..##.",".#.#.","#..#.","#####","...#.","...#."}},{'5',{"#####","#....","####.","....#","....#","#...#",".###."}},
 {'6',{"..##.",".#...","#....","####.","#...#","#...#",".###."}},{'7',{"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
 {'8',{".###.","#...#","#...#",".###.","#...#","#...#",".###."}},{'9',{".###.","#...#","#...#",".####","....#","...#.",".##.."}},
 {'.',{".....",".....",".....",".....",".....",".##..",".##.."}},{',',{".....",".....",".....",".....",".##..","..#..",".#..."}},
 {'!',{"..#..","..#..","..#..","..#..","..#..",".....","..#.."}},{'?',{".###.","#...#","....#","...#.","..#..",".....","..#.."}},
 {'-',{".....",".....",".....","#####",".....",".....","....."}},{':',{".....",".##..",".##..",".....",".##..",".##..","....."}},
 {'\'',{"..#..","..#..",".#...",".....",".....",".....","....."}},{'/',{"....#","....#","...#.","..#..",".#...","#....","#...."}},
 {'+',{".....","..#..","..#..","#####","..#..","..#..","....."}},{'=',{".....",".....","#####",".....","#####",".....","....."}},
 {'(',{"...#.","..#..",".#...",".#...",".#...","..#..","...#."}},{')',{".#...","..#..","...#.","...#.","...#.","..#..",".#..."}},
 {'_',{".....",".....",".....",".....",".....",".....","#####"}},
};
const Glyph* find_glyph(char c) {
  if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  for (const auto& g : kFont) if (g.c == c) return &g;
  return nullptr;
}
void add_box(Mesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
  const float X[2] = {x0, x1}, Y[2] = {y0, y1}, Z[2] = {z0, z1};
  for (int axis = 0; axis < 3; axis++) for (int side = 0; side < 2; side++) {
    float p[4][3], n[3] = {0, 0, 0}; n[axis] = side ? 1.f : -1.f;
    const int u = (axis + 1) % 3, v = (axis + 2) % 3;
    const float* A[3] = {X, Y, Z};
    for (int i = 0; i < 4; i++) { p[i][axis] = A[axis][side]; p[i][u] = A[u][(i == 1 || i == 2) ? 1 : 0]; p[i][v] = A[v][(i >= 2) ? 1 : 0]; }
    quad(m, p, n);
  }
}
}  // namespace
Mesh make_text(const std::string& text, float height) {
  Mesh m; const float px = height / 7.f; float x = 0;
  for (char ch : text) {
    if (ch == ' ') { x += 6 * px; continue; }
    const Glyph* g = find_glyph(ch); if (!g) g = find_glyph('?');
    for (int row = 0; row < 7; row++) for (int col = 0; col < 5; col++) if (g->rows[row][col] == '#')
      add_box(m, x + col * px, (6 - row) * px, 0.f, x + (col + 1) * px, (7 - row) * px, px);
    x += 6 * px;
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
