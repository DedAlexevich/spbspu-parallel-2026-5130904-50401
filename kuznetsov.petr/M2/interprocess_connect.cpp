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


