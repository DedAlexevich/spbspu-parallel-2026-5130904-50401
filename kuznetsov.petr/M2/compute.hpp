#ifndef COMPUTE_HPP
#define COMPUTE_HPP
#include <chrono>
#include <cstddef>
#include <future>
#include <map>

#include "interprocess_connect.hpp"
namespace kuznetsov {
  void computeProcess(int rd, int wr, size_t seed);

  namespace detail {
    struct result_t {
      double ms;
      double area;
    };

    class Clicker {
    public:
      Clicker();
      double millisec() const;

    private:
      std::chrono::time_point< std::chrono::steady_clock > start_;
    };

    using pending_t = std::map< size_t, std::shared_future< result_t > >;
    using finished_t = std::map< size_t, response_t >;

    result_t runArea(const Mapping& map, std::shared_future< result_t > prev, size_t count, size_t threads,
        size_t tries, size_t seed);
    response_t startCalc(const request_t& req, size_t seed, pending_t& pending, finished_t& finished);
    response_t finishCalc(size_t id, pending_t& pending, finished_t& finished);
    response_t checkCalc(size_t id, bool block, size_t wait_ms, pending_t& pending, finished_t& finished);
    response_t handleRequest(const request_t& req, size_t seed, pending_t& pending, finished_t& finished);
  }

}

#endif
