#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <new>
#include <string>
#include <vector>

namespace kuznetsov {
  struct circle_t {
    int x, y, r;
  };

  struct hits_t {
    size_t any, all;
  };

  struct areas_t {
    double covered, intersection;
  };

  struct rect_t {
    double xmin, xmax, ymin, ymax;
  };

  rect_t bounds(const std::vector< circle_t >& figs);
  areas_t area(const std::vector< circle_t >& figs, size_t thrds, size_t tests, size_t seed);
  hits_t calc(const std::vector< circle_t >& crls, rect_t rect, size_t tests, size_t seed);
  bool isInside(double x, double y, circle_t c);
  bool getQuartet(std::istream& is, circle_t& c);
}

int main(int argc, char** argv)
{
  constexpr int thread_index = 1;
  constexpr int test_index = 2;
  constexpr int seed_index = 3;

  constexpr int good_exit = 0;
  constexpr int incorrect_input = 1;
  constexpr int bad_alloc = 2;
  constexpr int other_error = 3;

  long long threads = 0;
  int tests = 0, seed = 0;
  constexpr int min_args = 3, max_args = 4;

  if (argc < min_args || argc > max_args) {
    std::cerr << "Not enough arguments\n";
    return incorrect_input;
  }

  try {
    threads = std::stoll(argv[thread_index]);
    tests = std::stoi(argv[test_index]);
    if (argc == seed_index + 1) {
      seed = std::stoi(argv[seed_index]);
    }
    threads = threads > 0 ? threads : 1;
  } catch (const std::invalid_argument& ia) {
    std::cerr << ia.what() << '\n';
    return incorrect_input;
  } catch (const std::out_of_range& oor) {
    std::cerr << oor.what() << '\n';
    return incorrect_input;
  }

  if (threads < 0 || tests <= 0 || seed < 0) {
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
    return incorrect_input;
  }

  kuznetsov::areas_t ar{};
  try {
    ar = kuznetsov::area(circles, threads, tests, seed);
  } catch (const std::runtime_error& e) {
    std::cerr << e.what() << '\n';
    return other_error;
  } catch (const std::bad_alloc& e) {
    std::cerr << e.what() << '\n';
    return bad_alloc;
  }
  std::cout << ar.covered << ' ' << ar.intersection << '\n';
  return good_exit;
}

bool kuznetsov::isInside(double x, double y, circle_t c)
{
  return (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) <= c.r * c.r;
}

kuznetsov::hits_t kuznetsov::calc(const std::vector< circle_t >& crls, rect_t rect, size_t tests, size_t seed)
{
  std::default_random_engine eng(seed);
  std::uniform_real_distribution< double > dist_x(rect.xmin, rect.xmax);
  std::uniform_real_distribution< double > dist_y(rect.ymin, rect.ymax);

  hits_t res{};
  for (size_t i = 0; i < tests; ++i) {
    const double x = dist_x(eng);
    const double y = dist_y(eng);
    const size_t cnt = std::count_if(crls.cbegin(), crls.cend(),
        [x, y](const circle_t& c)
        {
          return isInside(x, y, c);
        });
    res.any += cnt > 0;
    res.all += cnt == crls.size();
  }

  return res;
}

kuznetsov::areas_t kuznetsov::area(const std::vector< circle_t >& figs, size_t thrds, size_t tests, size_t seed)
{
  if (figs.empty()) {
    return {0.0, 0.0};
  }

  rect_t rect = bounds(figs);
  const size_t test_on_thread = tests / thrds;
  const size_t last_tests = tests % thrds;

  std::vector< std::future< hits_t > > res;
  res.reserve(thrds);

  for (size_t i = 0; i < thrds; ++i) {
    res.emplace_back(
        std::async(std::launch::async, calc, std::cref(figs), rect, test_on_thread + (i < last_tests), seed + i));
  }

  hits_t sum{0, 0};
  for (size_t i = 0; i < thrds; ++i) {
    const hits_t r = res[i].get();
    sum.any += r.any;
    sum.all += r.all;
  }

  const double rect_square = (rect.xmax - rect.xmin) * (rect.ymax - rect.ymin);
  return {sum.any * rect_square / tests, sum.all * rect_square / tests};
}

bool kuznetsov::getQuartet(std::istream& is, circle_t& c)
{
  if ((is >> std::ws).eof()) {
    return false;
  }

  int a{}, b{}, x{}, y{};
  if (!(is >> a >> b >> x >> y) || a <= 0) {
    throw std::invalid_argument("invalid input of quartet");
  }
  c.r = a;
  c.x = x;
  c.y = y;
  return true;
}

kuznetsov::rect_t kuznetsov::bounds(const std::vector< circle_t >& figs)
{
  const circle_t& f = figs.front();
  rect_t r{static_cast< double >(f.x) - f.r, static_cast< double >(f.x) + f.r, static_cast< double >(f.y) - f.r,
      static_cast< double >(f.y) + f.r};
  for (const circle_t& c : figs) {
    r.xmin = std::min(r.xmin, static_cast< double >(c.x) - c.r);
    r.xmax = std::max(r.xmax, static_cast< double >(c.x) + c.r);
    r.ymin = std::min(r.ymin, static_cast< double >(c.y) - c.r);
    r.ymax = std::max(r.ymax, static_cast< double >(c.y) + c.r);
  }
  return r;
}
