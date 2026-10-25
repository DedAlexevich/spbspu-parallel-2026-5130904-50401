#include "interprocess_connect.h"

#include <algorithm>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
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

kuznetsov::detail::ShmUnlinker::ShmUnlinker(const char* name):
  name_(name)
{}

kuznetsov::detail::ShmUnlinker::~ShmUnlinker()
{
  shm_unlink(name_);
}

const kuznetsov::ellipse_t* kuznetsov::Mapping::shapes() const
{
  return static_cast< const ellipse_t* >(ptr_);
}

std::system_error kuznetsov::detail::sysError(const char* what)
{
  return std::system_error(errno, std::generic_category(), what);
}

size_t kuznetsov::detail::shmBytes(size_t count)
{
  return count * sizeof(ellipse_t);
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

void kuznetsov::writeSetShm(const std::string& name, const set_t& set)
{
  const size_t bytes = detail::shmBytes(set.shapes.size());
  int fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    throw detail::sysError("shm_open");
  }

  try {
    detail::FdGuard fg(fd);
    if (ftruncate(fd, static_cast< off_t >(bytes)) < 0) {
      throw detail::sysError("ftruncate");
    }

    void* ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
      throw detail::sysError("mmap");
    }

    Mapping map(ptr, bytes);
    std::copy(set.shapes.cbegin(), set.shapes.cend(), static_cast< ellipse_t* >(ptr));
  } catch (...) {
    shm_unlink(name.c_str());
    throw;
  }
}

kuznetsov::Mapping kuznetsov::openSetShm(const char* name, size_t count)
{
  const size_t bytes = detail::shmBytes(count);
  int fd = shm_open(name, O_RDONLY, 0);

  if (fd < 0) {
    throw detail::sysError("shm_open");
  }
  detail::FdGuard fg(fd);

  void* ptr = mmap(nullptr, bytes, PROT_READ, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    throw detail::sysError("mmap");
  }
  return Mapping(ptr, bytes);
}
