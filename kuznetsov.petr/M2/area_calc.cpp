#include "area_calc.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <future>
#include <random>
#include <stdexcept>
#include <vector>

bool kuznetsov::isInside(double x, double y, const ellipse_t& c)
{
  const double dx = x - c.x;
  const double dy = y - c.y;
  const double cs = std::cos(c.angle);
  const double sn = std::sin(c.angle);
  const double u = (dx * cs + dy * sn) / c.rx;
  const double v = (dy * cs - dx * sn) / c.ry;
  return u * u + v * v <= 1.0;
}

size_t kuznetsov::calc(const ellipse_t* b, const ellipse_t* e, rect_t rect, size_t tests, size_t seed)
{
  std::default_random_engine eng(seed);
  const double range = static_cast< double >(eng.max() - eng.min());
  const double kx = (rect.xmax - rect.xmin) / range;
  const double ky = (rect.ymax - rect.ymin) / range;

  size_t res{};
  for (size_t i = 0; i < tests; ++i) {
    const double x = rect.xmin + static_cast< double >(eng() - eng.min()) * kx;
    const double y = rect.ymin + static_cast< double >(eng() - eng.min()) * ky;
    bool inside = false;
    for (const ellipse_t* it = b; it != e && !inside; ++it) {
      inside = isInside(x, y, *it);
    }
    res += inside;
  }
  return res;
}

double kuznetsov::area(const ellipse_t* b, const ellipse_t* e, size_t thrds, size_t tsts, size_t seed)
{
  if (thrds == 0 || tsts == 0) {
    throw std::invalid_argument("threads and tests must be positive");
  }
  if (b == e) {
    return 0.0;
  }
  constexpr size_t max_cores = 1000;
  thrds = std::min(thrds, max_cores);
  const rect_t rect = bounds(b, e);
  const size_t test_on_thread = tsts / thrds;
  const size_t last_tests = tsts % thrds;

  std::vector< std::future< size_t > > res;
  res.reserve(thrds);

  for (size_t i = 0; i < thrds; ++i) {
    res.emplace_back(std::async(std::launch::async, calc, b, e, rect, test_on_thread + (i < last_tests), seed + i));
  }

  double sum{};
  for (size_t i = 0; i < thrds; ++i) {
    sum += res[i].get();
  }

  const double rect_square = (rect.xmax - rect.xmin) * (rect.ymax - rect.ymin);
  return sum * rect_square / tsts;
}

kuznetsov::rect_t kuznetsov::bounds(const ellipse_t* b, const ellipse_t* e)
{
  rect_t r{};
  for (const ellipse_t* it = b; it != e; ++it) {
    const double c = std::cos(it->angle);
    const double s = std::sin(it->angle);
    const double rx = it->rx;
    const double ry = it->ry;
    const double hx = std::sqrt(rx * rx * c * c + ry * ry * s * s);
    const double hy = std::sqrt(rx * rx * s * s + ry * ry * c * c);
    const rect_t cur{it->x - hx, it->x + hx, it->y - hy, it->y + hy};
    if (it == b) {
      r = cur;
    } else {
      r.xmin = std::min(r.xmin, cur.xmin);
      r.xmax = std::max(r.xmax, cur.xmax);
      r.ymin = std::min(r.ymin, cur.ymin);
      r.ymax = std::max(r.ymax, cur.ymax);
    }
  }
  return r;
}
