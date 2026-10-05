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

  kuznetsov::circle_t c1 {}, c2{};
  if (!kuznetsov::getQuartet(std::cin, c1)) {
    std::cerr << "Bad parameters of first figure\n";
    return 3;
  }
  if (!kuznetsov::getQuartet(std::cin, c2)) {
    std::cerr << "Bad parameters of second figure\n";
    return 3;
  }

  kuznetsov::areas ar {};
  try {
    ar = kuznetsov::area(c1, c2, threads, tests, seed);
  } catch (const std::runtime_error& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  std::cout << ar.firstCircle + ar.secondCircle << ' ' << ar.intersection << '\n';
}

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


