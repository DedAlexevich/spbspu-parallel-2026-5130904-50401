#include "cli.h"

kuznetsov::detail::IOGuard::IOGuard(std::basic_ios< char >& s):
  s_(s),
  width_(s.width()),
  precision_(s.precision()),
  fmt_(s.flags()),
  fill_(s.fill())
{}

kuznetsov::detail::IOGuard::~IOGuard()
{
  s_.width(width_);
  s_.precision(precision_);
  s_.fill(fill_);
  s_.flags(fmt_);
}

bool kuznetsov::detail::atLineEnd(std::istream& in)
{
  int ch = in.peek();
  while (ch == ' ' || ch == '\t') {
    in.get();
    ch = in.peek();
  }
  return ch == '\n' || ch == EOF;
}

void kuznetsov::detail::requireLineEnd(std::istream& in)
{
  if (!in || !atLineEnd(in)) {
    throw std::invalid_argument("bad arguments");
  }
}
