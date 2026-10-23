#include "preview.hpp"
#include "deflate.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace lmfilament {
namespace {
using V3 = std::array<double, 3>;
V3 sub(V3 a, V3 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(V3 a, V3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 cross(V3 a, V3 b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
V3 norm(V3 a) { double l = std::sqrt(dot(a, a)); if (l == 0) throw std::runtime_error("degenerate vector"); return {a[0] / l, a[1] / l, a[2] / l}; }

struct Vtx { double x, y, z; V3 world, normal, color; bool has_color; };  // x,y,z in camera space

Vtx lerp(const Vtx& a, const Vtx& b, double t) {
  Vtx r;
  r.x = a.x + (b.x - a.x) * t; r.y = a.y + (b.y - a.y) * t; r.z = a.z + (b.z - a.z) * t;
  for (int i = 0; i < 3; i++) {
    r.world[i] = a.world[i] + (b.world[i] - a.world[i]) * t;
    r.normal[i] = a.normal[i] + (b.normal[i] - a.normal[i]) * t;
    r.color[i] = a.color[i] + (b.color[i] - a.color[i]) * t;
  }
  r.has_color = a.has_color;
  return r;
}
double to_srgb(double v) { v = std::clamp(v, 0.0, 1.0); return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055; }
void put32(std::vector<unsigned char>& o, uint32_t v) { for (int s : {24, 16, 8, 0}) o.push_back(static_cast<unsigned char>(v >> s)); }
uint32_t crc32(const unsigned char* p, size_t n) {
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int b = 0; b < 8; b++) c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1))); }
  return c ^ 0xffffffffu;
}
void chunk(std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& data) {
  put32(out, uint32_t(data.size()));
  const size_t begin = out.size();
  for (int i = 0; i < 4; i++) out.push_back(static_cast<unsigned char>(type[i]));
  out.insert(out.end(), data.begin(), data.end());
  put32(out, crc32(out.data() + begin, out.size() - begin));
}
}  // namespace

void render_png(const PreviewScene& sc, int w, int h, const std::string& path) {
  if (path.empty()) throw std::runtime_error("output path must not be empty");
  if (w < 1 || h < 1 || w > 4096 || h > 4096) throw std::runtime_error("image dimensions must be between 1 and 4096");
  constexpr int SS = 2;
  const int W = w * SS, H = h * SS;
  std::vector<float> rgb(size_t(W) * H * 3);
  for (size_t i = 0; i < size_t(W) * H; i++) for (int c = 0; c < 3; c++) rgb[3 * i + c] = sc.sky[c];
  std::vector<double> depth(size_t(W) * H, std::numeric_limits<double>::infinity());

  const V3 eye = sc.eye, fwd = norm(sub(sc.target, sc.eye));
  V3 right = cross(fwd, {0, 1, 0});
  if (dot(right, right) < 1e-12) right = {1, 0, 0};  // looking straight up/down
  right = norm(right);
  const V3 up = cross(right, fwd);
  const double focal = 0.5 * H / std::tan(sc.fov_degrees * M_PI / 360.0), near = 0.05;
  const bool headlight = sc.lights.empty();

  for (const auto& item : sc.items) {
    const auto& m = item.mesh;
    for (size_t t = 0; t < m.triangle_count(); t++) {
      std::vector<Vtx> poly;
      for (int k = 0; k < 3; k++) {
        const uint32_t i = m.indices[3 * t + k];
        V3 wp{m.positions[3 * i] + item.translation[0], m.positions[3 * i + 1] + item.translation[1], m.positions[3 * i + 2] + item.translation[2]};
        const V3 d = sub(wp, eye);
        Vtx v;
        v.x = dot(d, right); v.y = dot(d, up); v.z = dot(d, fwd);
        v.world = wp;
        v.normal = {m.normals[3 * i], m.normals[3 * i + 1], m.normals[3 * i + 2]};
        v.has_color = !m.colors.empty();
        if (v.has_color) { const uint32_t c = m.colors[i]; v.color = {(c & 255) / 255.0, ((c >> 8) & 255) / 255.0, ((c >> 16) & 255) / 255.0}; }
        else v.color = {item.base_color[0], item.base_color[1], item.base_color[2]};
        poly.push_back(v);
      }
      // Near-plane clip (Sutherland-Hodgman).
      std::vector<Vtx> clipped;
      for (size_t a = 0; a < poly.size(); a++) {
        const Vtx& p = poly[a]; const Vtx& q = poly[(a + 1) % poly.size()];
        const bool pin = p.z >= near, qin = q.z >= near;
        if (pin) clipped.push_back(p);
        if (pin != qin) clipped.push_back(lerp(p, q, (near - p.z) / (q.z - p.z)));
      }
      if (clipped.size() < 3) continue;
      for (size_t f = 1; f + 1 < clipped.size(); f++) {
        const Vtx tri[3] = {clipped[0], clipped[f], clipped[f + 1]};
        double sx[3], sy[3];
        for (int k = 0; k < 3; k++) { sx[k] = W * 0.5 + tri[k].x * focal / tri[k].z; sy[k] = H * 0.5 - tri[k].y * focal / tri[k].z; }
        const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sy[1] - sy[0]) * (sx[2] - sx[0]);
        if (std::abs(area) < 1e-9) continue;
        const int x0 = std::max(0, int(std::floor(std::min({sx[0], sx[1], sx[2]})))), x1 = std::min(W - 1, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
        const int y0 = std::max(0, int(std::floor(std::min({sy[0], sy[1], sy[2]})))), y1 = std::min(H - 1, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
          const double px = x + 0.5, py = y + 0.5;
          const double a = ((sx[1] - px) * (sy[2] - py) - (sy[1] - py) * (sx[2] - px)) / area;
          const double b = ((sx[2] - px) * (sy[0] - py) - (sy[2] - py) * (sx[0] - px)) / area;
          const double c = 1 - a - b;
          if (a < -1e-9 || b < -1e-9 || c < -1e-9) continue;
          const double wa = a / tri[0].z, wb = b / tri[1].z, wc = c / tri[2].z, sum = wa + wb + wc;
          const double z = 1 / sum;
          const size_t at = size_t(y) * W + x;
          if (z >= depth[at]) continue;
          depth[at] = z;
          auto mix = [&](const V3& p, const V3& q, const V3& r) { V3 o; for (int i = 0; i < 3; i++) o[i] = (p[i] * wa + q[i] * wb + r[i] * wc) / sum; return o; };
          V3 color = mix(tri[0].color, tri[1].color, tri[2].color), out;
          if (tri[0].has_color) out = color;  // unlit vertex colors
          else {
            V3 n = mix(tri[0].normal, tri[1].normal, tri[2].normal);
            V3 wp = mix(tri[0].world, tri[1].world, tri[2].world);
            if (dot(n, n) < 1e-12) n = norm(cross(sub(tri[1].world, tri[0].world), sub(tri[2].world, tri[0].world)));
            n = norm(n);
            const V3 toeye = norm(sub(eye, wp));
            if (dot(n, toeye) < 0) n = {-n[0], -n[1], -n[2]};  // two-sided
            double lit[3] = {0.12, 0.12, 0.12};
            auto add = [&](V3 l, double k, const std::array<float, 3>& lc) { const double d = std::max(0.0, dot(n, l)) * k; for (int i = 0; i < 3; i++) lit[i] += d * lc[i]; };
            if (headlight) add(toeye, 0.85, {1, 1, 1});
            for (const auto& L : sc.lights) {
              if (L.directional) add(norm({-L.vec[0], -L.vec[1], -L.vec[2]}), std::min(1.5, L.intensity / 110000.0), L.color);
              else {
                const V3 d{L.vec[0] - wp[0], L.vec[1] - wp[1], L.vec[2] - wp[2]};
                const double r2 = std::max(1e-4, dot(d, d));
                // Preview exposure: lux = I / (4 pi r^2), scaled so ~40 lux is full brightness.
                add(norm(d), std::min(1.5, L.intensity / (4 * M_PI * r2) / 40.0), L.color);
              }
            }
            for (int i = 0; i < 3; i++) out[i] = 1 - std::exp(-1.3 * color[i] * lit[i]);  // mild tone map: keeps strong suns from clipping
          }
          for (int i = 0; i < 3; i++) rgb[3 * at + i] = float(out[i]);
        }
      }
    }
  }
  // Resolve supersampling in linear light, then sRGB-encode once.
  std::vector<unsigned char> raw;
  raw.reserve(size_t(h) * (size_t(w) * 3 + 1));
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) for (int c = 0; c < 3; c++) {
      double s = 0;
      for (int dy = 0; dy < SS; dy++) for (int dx = 0; dx < SS; dx++) s += rgb[(size_t(y * SS + dy) * W + (x * SS + dx)) * 3 + c];
      raw.push_back(static_cast<unsigned char>(std::lround(to_srgb(s / (SS * SS)) * 255)));
    }
  }
  std::vector<unsigned char> png{137, 80, 78, 71, 13, 10, 26, 10}, ihdr;
  put32(ihdr, uint32_t(w)); put32(ihdr, uint32_t(h)); ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
  chunk(png, "IHDR", ihdr); chunk(png, "sRGB", {0}); chunk(png, "IDAT", deflate_zlib(raw)); chunk(png, "IEND", {});
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot open " + path);
  out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
  if (!out) throw std::runtime_error("write failed: " + path);
}
}  // namespace lmfilament
