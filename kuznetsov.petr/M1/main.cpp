#include <algorithm>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    int x, y, r;
  };
  struct hits_t {
    size_t any, all;
  };
  struct areas {
    double covered, intersection;
  };
  struct rect_t {
    double xmin, xmax, ymin, ymax;
  };
  rect_t bounds(const std::vector< circle_t >& figs);
  areas area(const std::vector< circle_t >& figs, size_t thrds, size_t tests, size_t seed);
  hits_t calc(const std::vector< circle_t >& crls, rect_t rect, size_t tests, size_t seed);
  bool isInside(double x, double y, circle_t c);
  bool getQuartet(std::istream& is, circle_t& c);
}

int main(int argc, char** argv)
{
  int threads = 0, tests = 0, seed = 0;

  if (argc < 3) {
    std::cerr << "Not enough arguments\n";
    return 1;
  }
  try {
    threads = std::stoi(argv[1]);
    tests = std::stoi(argv[2]);
    if (argc == 4) {
      seed = std::stoi(argv[3]);
    }
  } catch (const std::invalid_argument& ia) {
    std::cerr << ia.what() << '\n';
    return 1;
  } catch (const std::out_of_range& oor) {
    std::cerr << oor.what() << '\n';
    return 2;
  }

  if (threads <= 0 || tests <= 0 || seed < 0) {
    std::cerr << "threads, tests and seed must be positive\n";
    return 1;
  }

  std::vector< kuznetsov::circle_t > circles;
  kuznetsov::circle_t c{};
  try {
    while (kuznetsov::getQuartet(std::cin, c)) {
      circles.push_back(c);
    }
  } catch (const std::invalid_argument& ia) {
    std::cerr << ia.what() << '\n';
    return 3;
  }

  kuznetsov::areas ar{};
  try {
    ar = kuznetsov::area(circles, threads, tests, seed);
  } catch (const std::runtime_error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  std::cout << ar.covered << ' ' << ar.intersection << '\n';
}

bool kuznetsov::isInside(double x, double y, circle_t c)
{
  return (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) <= c.r * c.r;
}

kuznetsov::hits_t kuznetsov::calc(
  const std::vector< circle_t >& crls, rect_t rect, size_t tests, size_t seed)
{
  std::default_random_engine eng(seed);
  std::uniform_real_distribution< double > distX(rect.xmin, rect.xmax);
  std::uniform_real_distribution< double > distY(rect.ymin, rect.ymax);

  hits_t res{};
  for (size_t i = 0; i < tests; ++i) {
    double x = distX(eng);
    double y = distY(eng);
    size_t cnt = std::count_if(crls.cbegin(), crls.cend(), [x, y](const circle_t& c) {
      return isInside(x, y, c);
    });
    res.any += cnt > 0;
    res.all += cnt == crls.size();
  }

  return res;
}

kuznetsov::areas kuznetsov::area(
  const std::vector< circle_t >& figs, size_t thrds, size_t tests, size_t seed)
{
  if (figs.empty()) {
    return {0.0, 0.0};
  }

  rect_t rect = bounds(figs);
  size_t testOnThread = tests / thrds;
  size_t lastTests = tests % thrds;

  std::vector< std::future< hits_t > > res;
  res.reserve(thrds);

  for (size_t i = 0; i < thrds; ++i) {
    res.emplace_back(std::async(std::launch::async,
      calc,
      std::cref(figs),
      rect,
      testOnThread + (i < lastTests),
      seed + i + 1));
  }

  hits_t sum{0, 0};
  for (size_t i = 0; i < thrds; ++i) {
    hits_t r = res[i].get();
    sum.any += r.any;
    sum.all += r.all;
  }

  double rectSquare = (rect.xmax - rect.xmin) * (rect.ymax - rect.ymin);
  return {sum.any * rectSquare / tests, sum.all * rectSquare / tests};
}

bool kuznetsov::getQuartet(std::istream& is, circle_t& c)
{
  if ((is >> std::ws).eof()) {
    return false;
  }

  int pm[4]{};
  if (!(is >> pm[0] >> pm[1] >> pm[2] >> pm[3]) || pm[0] <= 0) {
    throw std::invalid_argument("invalid input of quartet");
  }
  c.r = pm[0];
  c.x = pm[2];
  c.y = pm[3];
  return true;
}

kuznetsov::rect_t kuznetsov::bounds(const std::vector< circle_t >& figs)
{
  const circle_t& f = figs.front();
  rect_t r{static_cast< double >(f.x) - f.r,
    static_cast< double >(f.x) + f.r,
    static_cast< double >(f.y) - f.r,
    static_cast< double >(f.y) + f.r};
  for (const circle_t& c: figs) {
    r.xmin = std::min(r.xmin, static_cast< double >(c.x) - c.r);
    r.xmax = std::max(r.xmax, static_cast< double >(c.x) + c.r);
    r.ymin = std::min(r.ymin, static_cast< double >(c.y) - c.r);
    r.ymax = std::max(r.ymax, static_cast< double >(c.y) + c.r);
  }
  return r;
}
