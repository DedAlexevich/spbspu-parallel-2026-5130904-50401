#include "interprocess_connect.h"

#include <utility>
#include <sys/mman.h>
#include <unistd.h>

kuznetsov::detail::FdGuard::FdGuard(int fd):
  fd_(fd)
{}

kuznetsov::detail::FdGuard::~FdGuard()
{
  close(fd_);
}

kuznetsov::Mapping::Mapping(void* ptr, size_t size):
  ptr_(ptr),
  size_(size)
{}

kuznetsov::Mapping::Mapping(Mapping&& other) noexcept:
  ptr_(std::exchange(other.ptr_, nullptr)),
  size_(std::exchange(other.size_, 0))
{}

kuznetsov::Mapping::~Mapping()
{
  if (ptr_) {
    munmap(ptr_, size_);
  }
}

const kuznetsov::circle_t* kuznetsov::Mapping::circles() const
{
  return static_cast< const circle_t* >(ptr_);
}

std::system_error kuznetsov::detail::sysError(const char* what)
{
  return std::system_error(errno, std::generic_category(), what);
}

size_t kuznetsov::detail::shmBytes(size_t count)
{
  return count * sizeof(circle_t);
}

size_t kuznetsov::sendAll(int& err, int wr, const char* b, size_t k)
{
  size_t r = 0;
  while (r < k) {
    err = write(wr, b + r, k - r);
    if (err <= 0) {
      break;
    }
    r += err;
  }
  return r;
}

size_t kuznetsov::recvAll(int& err, int rd, char* b, size_t k)
{
  size_t r = 0;
  while (r < k) {
    err = read(rd, b + r, k - r);
    if (err <= 0) {
      break;
    }
    r += err;
  }
  return r;
}

bool kuznetsov::exchange(int wr, int rd, const request_t& req, response_t& res)
{
  int err = 0;
  if (sendAll(err, wr, reinterpret_cast< const char* >(&req), sizeof(req)) != sizeof(req)) {
    return false;
  }
  return recvAll(err, rd, reinterpret_cast< char* >(&res), sizeof(res)) == sizeof(res);
}
