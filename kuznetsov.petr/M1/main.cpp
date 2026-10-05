#include <iostream>
#include <thread>
#include <random>
#include <stdexcept>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    double x, y, r;
  };
  struct areas {
    double firstCircle, secondCircle, intersection;
  };
  areas area(circle_t c1, circle_t c2, size_t threads, size_t tests);
  size_t calc(circle_t c, size_t tests, size_t seed);
  bool isInside(double x, double y, circle_t c);
  bool isIntersect(circle_t c1, circle_t c2);
}

int main()
{}

bool kuznetsov::isInside(double x, double y, circle_t c)
{
  return (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) <= c.r * c.r;
}

size_t kuznetsov::calc(circle_t c, size_t tests, size_t seed)
{
  std::default_random_engine eng(seed);
  std::uniform_real_distribution< double > distX(c.x - c.r, c.x + c.r);
  std::uniform_real_distribution< double > distY(c.y - c.r, c.y + c.r);
  size_t res = 0;
  for (size_t i = 0; i < tests; ++i) {
    res += isInside(distX(eng), distY(eng), c);
  }
  return res;
}


