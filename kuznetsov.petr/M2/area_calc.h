#ifndef AREA_CALC_H
#define AREA_CALC_H
#include <cstddef>
#include <string>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    int r, x, y;
  };

  struct set_t {
    std::vector< circle_t > circles;
  };

  struct rect_t {
    double xmin, xmax, ymin, ymax;
  };

  rect_t bounds(const circle_t* b, const circle_t* e);
  double area(const circle_t* b, const circle_t* e, size_t thrds, size_t tsts, size_t seed);
  size_t calc(const circle_t* b, const circle_t* e, rect_t rect, size_t tests, size_t seed);
  bool isInside(double x, double y, const circle_t& c);
}

#endif
