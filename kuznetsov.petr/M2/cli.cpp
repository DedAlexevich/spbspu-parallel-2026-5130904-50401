#include "cli.h"

#include <limits>

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

void kuznetsov::detail::skipLine(std::istream& in)
{
  in.clear();
  in.ignore(std::numeric_limits< std::streamsize >::max(), '\n');
}

bool kuznetsov::detail::isValidName(const std::string& name)
{
  const char c = name.empty() ? '\0' : name[0];
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


