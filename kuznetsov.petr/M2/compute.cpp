#include "compute.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <ratio>
#include <string>
#include <utility>

#include "area_calc.hpp"
#include "interprocess_connect.hpp"

kuznetsov::detail::Clicker::Clicker():
  start_(std::chrono::steady_clock::now())
{}

double kuznetsov::detail::Clicker::millisec() const
{
  auto t = std::chrono::steady_clock::now();
  return std::chrono::duration< double, std::milli >(t - start_).count();
}

kuznetsov::detail::result_t kuznetsov::detail::runArea(
    const Mapping& map, std::shared_future< result_t > prev, size_t count, size_t threads, size_t tries, size_t seed)
{
  if (prev.valid()) {
    prev.wait();
  }
  const ellipse_t* const first = map.shapes();
  const Clicker cl;
  const double value = area(first, first + count, threads, tries, seed);
  const double ms = cl.millisec();
  return result_t{ms, value};
}

kuznetsov::response_t kuznetsov::detail::startCalc(
    const request_t& req, size_t seed, pending_t& pending, finished_t& finished)
{
  response_t res{req.calc_id, calc_state_t::failed, 0.0, 0.0};
  try {
    const char* const end = std::find(req.shm_name, req.shm_name + shm_name_size, '\0');
    const std::string name(req.shm_name, end);
    Mapping map = openSetShm(name.c_str(), req.count);
    std::shared_future< result_t > prev;
    if (!pending.empty()) {
      prev = pending.rbegin()->second;
    }
    auto ins = pending.emplace(req.calc_id, std::shared_future< result_t >{});
    try {
      ins.first->second
          = std::async(std::launch::async, runArea, std::move(map), prev, req.count, req.threads, req.tries, seed)
                .share();
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
    const result_t r = it->second.get();
    res.state = calc_state_t::done;
    res.ms = r.ms;
    res.area = r.area;
  } catch (const std::exception& e) {
    std::cerr << "calculation failed: " << e.what() << '\n';
  }
  pending.erase(it);
  return res;
}

kuznetsov::response_t kuznetsov::detail::checkCalc(
    size_t id, bool block, size_t wait_ms, pending_t& pending, finished_t& finished)
{
  auto done = finished.find(id);
  if (done != finished.end()) {
    return done->second;
  }
  auto it = pending.find(id);
  if (it == pending.end()) {
    return response_t{id, calc_state_t::failed, 0.0, 0.0};
  }
  if (!block && it->second.wait_for(std::chrono::milliseconds(wait_ms)) != std::future_status::ready) {
    return response_t{id, calc_state_t::progress, 0.0, 0.0};
  }
  return finishCalc(id, pending, finished);
}

kuznetsov::response_t kuznetsov::detail::handleRequest(
    const request_t& req, size_t seed, pending_t& pending, finished_t& finished)
{
  switch (req.kind) {
  case request_kind_t::calc:
    return startCalc(req, seed, pending, finished);
  case request_kind_t::status:
    return checkCalc(req.calc_id, false, 0, pending, finished);
  case request_kind_t::timeout:
    return checkCalc(req.calc_id, false, req.wait_ms, pending, finished);
  case request_kind_t::wait:
    return checkCalc(req.calc_id, true, 0, pending, finished);
  case request_kind_t::kill:
    return response_t{req.calc_id, calc_state_t::canceled, 0.0, 0.0};
  }
  return response_t{req.calc_id, calc_state_t::failed, 0.0, 0.0};
}

void kuznetsov::computeProcess(int rd, int wr, size_t seed)
{
  detail::pending_t pending;
  detail::finished_t finished;
  request_t req{};
  int err = 0;
  while (recvAll(err, rd, reinterpret_cast< char* >(&req), sizeof(req)) == sizeof(req)) {
    const response_t res = detail::handleRequest(req, seed, pending, finished);
    if (sendAll(err, wr, reinterpret_cast< const char* >(&res), sizeof(res)) != sizeof(res)) {
      break;
    }
    if (req.kind == request_kind_t::kill) {
      std::_Exit(0);
    }
  }
}
