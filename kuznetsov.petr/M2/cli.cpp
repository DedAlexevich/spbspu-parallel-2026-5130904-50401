#include "cli.h"

#include <iomanip>
#include <iostream>
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

std::istream& kuznetsov::detail::operator>>(std::istream& in, NameArg&& arg)
{
  IOGuard g(in);
  std::istream::sentry s(in, true);
  if (!s) {
    return in;
  }
  std::string value;
  if (!(in >> LineArg< std::string >{value})) {
    return in;
  }
  if (isValidName(value)) {
    arg.ref = value;
  } else {
    in.setstate(std::ios_base::failbit);
  }
  return in;
}

kuznetsov::detail::NameArg kuznetsov::detail::nameArg(std::string& ref)
{
  return NameArg{ref};
}

void kuznetsov::detail::printCircle(std::ostream& out, const circle_t& c)
{
  out << c.r << " (" << c.x << ' ' << c.y << ")\n";
}

void kuznetsov::detail::printRect(std::ostream& out, const rect_t& r)
{
  IOGuard g(out);
  out << std::fixed << std::setprecision(0);
  out << '(' << r.xmin << ' ' << r.ymin << ") (" << r.xmax << ' ' << r.ymax << ")\n";
}

void kuznetsov::detail::printStatus(std::ostream& out, const std::string& name, const calc_t& calc)
{
  switch (calc.state) {
  case calc_state_t::progress:
    out << calc.proc << " <IN PROGRESS>\n";
    break;
  case calc_state_t::done: {
    IOGuard g(out);
    out << std::fixed << std::setprecision(3);
    out << calc.proc << ' ' << calc.ms << ' ' << calc.area << '\n';
    break;
  }
  case calc_state_t::failed:
    std::cerr << "calculation " << name << " failed\n";
    break;
  }
}
