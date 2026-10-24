#include "compute.h"

kuznetsov::detail::Clicker::Clicker():
  start_(std::chrono::high_resolution_clock::now())
{}

double kuznetsov::detail::Clicker::millisec() const
{
  using std::chrono::high_resolution_clock;
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  auto t = high_resolution_clock::now();
  return duration_cast< milliseconds >(t - start_).count();
}

kuznetsov::detail::result_t kuznetsov::detail::runArea(const Mapping& map,
  size_t count, size_t threads, size_t tries, size_t seed)
{
  const circle_t* first = map.circles();
  Clicker cl;
  const double value = area(first, first + count, threads, tries, seed);
  const double ms = cl.millisec();
  return result_t{ms, value};
}




