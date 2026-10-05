#include <iostream>
#include <algorithm>
#include <thread>
#include <future>
#include <random>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    double x, y, r;
  };
  struct hits_t {
    size_t c1, c2, intersection;
  };
  struct areas {
    double firstCircle, secondCircle, intersection;
  };
  areas area(circle_t c1, circle_t c2, size_t thrds, size_t tests, size_t seed);
  hits_t calc(circle_t c1, circle_t c2, size_t tests, size_t seed);
  bool isInside(double x, double y, circle_t c);
}

int main()
{}

bool kuznetsov::isInside(double x, double y, circle_t c)
{
  return (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) <= c.r * c.r;
}

kuznetsov::hits_t kuznetsov::calc(circle_t c1, circle_t c2, size_t tests, size_t seed)
{
  const double xmin = std::min(c1.x - c1.r, c2.x - c2.r);
  const double xmax = std::max(c1.x + c1.r, c2.x + c2.r);
  const double ymin = std::min(c1.y - c1.r, c2.y - c2.r);
  const double ymax = std::max(c1.y + c1.r, c2.y + c2.r);

  std::default_random_engine eng(seed);
  std::uniform_real_distribution< double > distX(xmin, xmax);
  std::uniform_real_distribution< double > distY(ymin, ymax);
  
  size_t resC1 = 0;
  size_t resC2 = 0;
  size_t intersection = 0;

  for (size_t i = 0; i < tests; ++i) {
    double x = distX(eng);
    double y = distY(eng);
    bool in1 = isInside(x, y, c1), in2 = isInside(x, y, c2);
    resC1 += in1;
    resC2 += in2;
    intersection += in1 && in2;
  }

  return {resC1, resC2, intersection};
}

kuznetsov::areas kuznetsov::area(circle_t c1, circle_t c2, size_t thrds, size_t tests, size_t seed)
{
  size_t testOnThread = tests / thrds;
  size_t lastTests = tests % thrds;
  hits_t sumuraize {0, 0, 0};

  std::vector< std::future< hits_t > > res;
  res.reserve(thrds);

  for(size_t i = 0; i < thrds; ++i) {
    res.emplace_back(std::async(std::launch::async, calc, c1, c2,
      testOnThread + (i < lastTests), seed + i));
  }
  const double xmin = std::min(c1.x - c1.r, c2.x - c2.r);
  const double xmax = std::max(c1.x + c1.r, c2.x + c2.r);
  const double ymin = std::min(c1.y - c1.r, c2.y - c2.r);
  const double ymax = std::max(c1.y + c1.r, c2.y + c2.r);
  double rectSquare = (ymax - ymin) * (xmax - xmin);
  for(size_t i = 0; i < thrds; ++i) {
    hits_t r = res[i].get();
    sumuraize.c1 += r.c1;
    sumuraize.c2 += r.c2;
    sumuraize.intersection += r.intersection;
  }
  areas result {0, 0,0 };
  result.firstCircle = sumuraize.c1 * rectSquare / tests;
  result.secondCircle = sumuraize.c2 * rectSquare / tests;
  result.intersection = sumuraize.intersection * rectSquare / tests;

  return result;
}


