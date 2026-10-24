#ifndef AREA_CALC_H
#define AREA_CALC_H
#include <cstddef>
#include <string>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    std::string name;
    int r, x, y;
  };

  struct set_t {
    std::string name;
    std::vector< circle_t > circles;
  };

  struct rect_t {
    int xmin, xmax, ymin, ymax;
  };

  rect_t bounds(const set_t& set);
  double area(const set_t& set, size_t thrds, size_t tests, size_t seed);
  size_t calc(const set_t& set, rect_t rect, size_t tests, size_t seed);
  bool isInside(double x, double y, const circle_t& c);
}

#endif
