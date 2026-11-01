#ifndef INTERPROCESS_CONNECT_HPP
#define INTERPROCESS_CONNECT_HPP
#include <cstddef>
#include <string>
#include <system_error>

#include "area_calc.hpp"

namespace kuznetsov {
  constexpr size_t shm_name_size = 64;

  enum class request_kind_t { calc, status, wait, timeout, kill };

  enum class calc_state_t { progress, done, failed, canceled };

  struct request_t {
    request_kind_t kind;
    size_t calc_id;
    size_t threads;
    size_t tries;
    size_t count;
    size_t wait_ms;
    char shm_name[shm_name_size];
  };

  struct response_t {
    size_t calc_id;
    calc_state_t state;
    double ms;
    double area;
  };

  size_t sendAll(int& err, int wr, const char* b, size_t k);
  size_t recvAll(int& err, int rd, char* b, size_t k);
  bool exchange(int wr, int rd, const request_t& req, response_t& res);

  class Mapping {
  public:
    Mapping(void* ptr, size_t size);
    Mapping(Mapping&& other) noexcept;
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    Mapping& operator=(Mapping&&) = delete;
    ~Mapping();
    const ellipse_t* shapes() const;

  private:
    void* ptr_;
    size_t size_;
  };

  void writeSetShm(const std::string& name, const set_t& shapes);
  Mapping openSetShm(const char* name, size_t count);

  namespace detail {
    class FdGuard {
    public:
      explicit FdGuard(int fd);
      FdGuard(const FdGuard&) = delete;
      FdGuard& operator=(const FdGuard&) = delete;
      ~FdGuard();

    private:
      int fd_;
    };

    class ShmUnlinker {
    public:
      explicit ShmUnlinker(const char* name);
      ShmUnlinker(const ShmUnlinker&) = delete;
      ShmUnlinker& operator=(const ShmUnlinker&) = delete;
      ~ShmUnlinker();

    private:
      const char* name_;
    };

    std::system_error sysError(const char* what);
    size_t shmBytes(size_t count);
  }

}

#endif
