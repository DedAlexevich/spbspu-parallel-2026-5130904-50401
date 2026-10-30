#ifndef AREA_CALC_HPP
#define AREA_CALC_HPP
#include <cstddef>
#include <vector>

namespace kuznetsov {
  struct ellipse_t {
    double angle;
    int rx, ry, x, y;
    bool is_circle;
  };

  struct set_t {
    std::vector< const ellipse_t* > shapes;
  };

  struct rect_t {
    double xmin, xmax, ymin, ymax;
  };

  rect_t bounds(const ellipse_t* b, const ellipse_t* e);
  double area(const ellipse_t* b, const ellipse_t* e, size_t thrds, size_t tsts, size_t seed);
  size_t calc(const ellipse_t* b, const ellipse_t* e, rect_t rect, size_t tests, size_t seed);
  bool isInside(double x, double y, const ellipse_t& el);
}

#endif
