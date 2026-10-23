#include "area_calc.h"

#include <algorithm>
#include <future>
#include <random>

bool kuznetsov::isInside(double x, double y, circle_t c)
{
  const double dx = (x - c.x);
  const double dy = (y - c.y);
  return dx * dx + dy * dy <= c.r * c.r;
}

size_t kuznetsov::calc(const set_t& set, rect_t rect, size_t tests, size_t seed)
{
  std::default_random_engine eng(seed);
  std::uniform_real_distribution< double > dist_x(rect.xmin, rect.xmax);
  std::uniform_real_distribution< double > dist_y(rect.ymin, rect.ymax);

  size_t res{};
  for (size_t i = 0; i < tests; ++i) {
    const double x = dist_x(eng);
    const double y = dist_y(eng);
    const size_t cnt = std::count_if(set.circles.cbegin(), set.circles.cend(),
      [x, y](const circle_t& c) {
        return isInside(x, y, c);
      });
    res += cnt > 0;
  }
  return res;
}

double kuznetsov::area(const set_t& set, size_t thrds, size_t tests, size_t seed)
{
  if (set.circles.empty()) {
    return 0.0;
  }
  constexpr size_t max_cores = 1000;
  thrds = std::min(thrds, max_cores);
  rect_t rect = bounds(set);
  const size_t test_on_thread = tests / thrds;
  const size_t last_tests = tests % thrds;

  std::vector< std::future< size_t > > res;
  res.reserve(thrds);

  for (size_t i = 0; i < thrds; ++i) {
    res.emplace_back(std::async(
      std::launch::async, calc, std::cref(set), rect, test_on_thread + (i < last_tests), seed + i));
  }

  double sum{};
  for (size_t i = 0; i < thrds; ++i) {
    sum += res[i].get();
  }

  const double rect_square = (rect.xmax - rect.xmin) * (rect.ymax - rect.ymin);
  return sum * rect_square / tests;
}

kuznetsov::rect_t kuznetsov::bounds(const set_t& set)
{
  const circle_t& f = set.circles.front();
  rect_t r{static_cast< double >(f.x) - f.r,
    static_cast< double >(f.x) + f.r,
    static_cast< double >(f.y) - f.r,
    static_cast< double >(f.y) + f.r};
  for (auto it = set.circles.cbegin(); it != set.circles.cend(); ++it) {
    r.xmin = std::min(r.xmin, static_cast< double >(it->x) - it->r);
    r.xmax = std::max(r.xmax, static_cast< double >(it->x) + it->r);
    r.ymin = std::min(r.ymin, static_cast< double >(it->y) - it->r);
    r.ymax = std::max(r.ymax, static_cast< double >(it->y) + it->r);
  }
  return r;
}
