#include "compute.h"

#include <algorithm>
#include <iostream>

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

kuznetsov::response_t kuznetsov::detail::startCalc(
  const request_t& req, size_t seed, pending_t& pending, finished_t& finished)
{
  response_t res{req.calc_id, calc_state_t::failed, 0.0, 0.0};
  try {
    const char* end = std::find(req.shm_name, req.shm_name + shm_name_size, '\0');
    const std::string name(req.shm_name, end);
    Mapping map = openSetShm(name.c_str(), req.count);
    auto ins = pending.emplace(req.calc_id, std::future< result_t >{});
    try {
      ins.first->second = std::async(std::launch::async,
                        runArea, std::move(map), req.count, req.threads, req.tries, seed);
    } catch (...) {
      pending.erase(ins.first);
      throw;
    }
    res.state = calc_state_t::progress;
  } catch (const std::exception& e) {
    std::cerr << "calculation start failed: " << e.what() << '\n';
    finished.emplace(req.calc_id, res);
  }
  return res;
}

kuznetsov::response_t kuznetsov::detail::finishCalc(size_t id, pending_t& pending, finished_t& finished)
{
  auto it = pending.find(id);
  auto ins = finished.emplace(id, response_t{id, calc_state_t::failed, 0.0, 0.0});
  response_t& res = ins.first->second;
  try {
    result_t r = it->second.get();
    res.state = calc_state_t::done;
    res.ms = r.ms;
    res.area = r.area;
  } catch (const std::exception& e) {
    std::cerr << "calculation failed: " << e.what() << '\n';
  }
  pending.erase(it);
  return res;
}

kuznetsov::response_t kuznetsov::detail::checkCalc(size_t id, bool block, pending_t& pending, finished_t& finished)
{
  auto done = finished.find(id);
  if (done != finished.end()) {
    return done->second;
  }
  auto it = pending.find(id);
  if (it == pending.end()) {
    return response_t{id, calc_state_t::failed, 0.0, 0.0};
  }
  if (!block && it->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return response_t{id, calc_state_t::progress, 0.0, 0.0};
  }
  return finishCalc(id, pending, finished);
}


