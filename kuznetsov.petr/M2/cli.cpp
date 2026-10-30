#include "cli.hpp"

#include <cmath>
#include <math.h>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "area_calc.hpp"
#include "compute.hpp"
#include "interprocess_connect.hpp"

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
  const IOGuard g(in);
  const std::istream::sentry s(in, true);
  if (!s) {
    return in;
  }
  std::string value;
  if (!(in >> LineArg< std::string >{value})) {
    return in;
  }
  if (isValidName(value)) {
    arg.ref = std::move(value);
  } else {
    in.setstate(std::ios_base::failbit);
  }
  return in;
}

kuznetsov::detail::NameArg kuznetsov::detail::nameArg(std::string& ref)
{
  return NameArg{ref};
}

void kuznetsov::detail::printShape(std::ostream& out, const ellipse_t& el)
{
  if (el.is_circle) {
    out << el.rx << " (" << el.x << ' ' << el.y << ")\n";
    return;
  }
  const IOGuard g(out);
  out << el.rx << ' ' << el.ry << " (" << el.x << ' ' << el.y << ") ";
  out << std::fixed << std::setprecision(3) << el.angle << '\n';
}

void kuznetsov::detail::addShape(state_t& st, const std::string& name, const ellipse_t& el)
{
  if (st.shapes.count(name)) {
    throw std::invalid_argument("shape exists");
  }
  st.shapes.emplace(name, el);
}

void kuznetsov::detail::printRect(std::ostream& out, const rect_t& r)
{
  const IOGuard g(out);
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
    const IOGuard g(out);
    out << std::fixed << std::setprecision(3);
    out << calc.proc << ' ' << calc.ms << ' ' << calc.area << '\n';
    break;
  }
  case calc_state_t::failed:
    std::cerr << "calculation " << name << " failed\n";
    break;
  case calc_state_t::canceled:
    out << calc.proc << " <CANCELED>\n";
    break;
  }
}

void kuznetsov::detail::markDead(state_t& st, const std::string& name)
{
  proc_t& proc = st.procs.at(name);
  if (!proc.alive) {
    return;
  }
  proc.alive = false;
  close(proc.cmd_wr);
  close(proc.res_rd);
  proc.cmd_wr = -1;
  proc.res_rd = -1;
  waitpid(proc.pid, nullptr, 0);
  std::cerr << "process " << name << " terminated\n";
  for (auto it = st.calcs.begin(); it != st.calcs.end(); ++it) {
    calc_t& calc = it->second;
    if (calc.proc == name && calc.state == calc_state_t::progress) {
      calc.state = calc_state_t::failed;
      std::cerr << "calculation " << it->first << " failed\n";
    }
  }
}

void kuznetsov::detail::queryCalc(state_t& st, calc_t& calc, request_kind_t kind, size_t wait_ms)
{
  const proc_t& proc = st.procs.at(calc.proc);
  if (calc.state != calc_state_t::progress || !proc.alive) {
    return;
  }
  request_t req{};
  req.kind = kind;
  req.calc_id = calc.id;
  req.wait_ms = wait_ms;
  response_t res{};
  if (!exchange(proc.cmd_wr, proc.res_rd, req, res) || res.calc_id != calc.id) {
    markDead(st, calc.proc);
    return;
  }
  calc.state = res.state;
  calc.ms = res.ms;
  calc.area = res.area;
}

void kuznetsov::detail::closeOtherChildren(const state_t& st)
{
  for (auto it = st.procs.begin(); it != st.procs.end(); ++it) {
    if (it->second.alive) {
      close(it->second.cmd_wr);
      close(it->second.res_rd);
    }
  }
}

kuznetsov::proc_t kuznetsov::detail::startProc(std::ostream& out, const state_t& st, size_t seed)
{
  out.flush();
  int cmd[2]{};
  int res[2]{};

  if (pipe(cmd) < 0) {
    throw sysError("pipe");
  }
  if (pipe(res) < 0) {
    const std::system_error e = sysError("pipe");
    close(cmd[0]);
    close(cmd[1]);
    throw e;
  }
  const pid_t pid = fork();
  if (pid < 0) {
    const std::system_error e = sysError("fork");
    close(cmd[0]);
    close(cmd[1]);
    close(res[0]);
    close(res[1]);
    throw e;
  } else if (pid == 0) {
    close(cmd[1]);
    close(res[0]);
    closeOtherChildren(st);
    int code = 0;
    try {
      computeProcess(cmd[0], res[1], seed);
    } catch (const std::exception& e) {
      std::cerr << "compute process: " << e.what() << '\n';
      code = 1;
    } catch (...) {
      code = 1;
    }
    close(cmd[0]);
    close(res[1]);
    std::_Exit(code);
  }
  close(cmd[0]);
  close(res[1]);
  return proc_t{pid, cmd[1], res[0], true};
}

void kuznetsov::detail::shutdown(state_t& st)
{
  for (auto it = st.procs.begin(); it != st.procs.end(); ++it) {
    if (it->second.alive) {
      close(it->second.cmd_wr);
      close(it->second.res_rd);
    }
  }
  for (auto it = st.procs.begin(); it != st.procs.end(); ++it) {
    if (it->second.alive) {
      waitpid(it->second.pid, nullptr, 0);
      it->second.alive = false;
    }
  }
}

void kuznetsov::cliProcess(std::istream& in, std::ostream& out)
{
  state_t st{};
  try {
    std::unordered_map< std::string, cmd_t > cmds;
    cmds["circle"] = doCircle;
    cmds["ellipse"] = doEllipse;
    cmds["rotate"] = doRotate;
    cmds["set"] = doSet;
    cmds["show"] = doShow;
    cmds["showset"] = doShowSet;
    cmds["frame"] = doFrame;
    cmds["frameset"] = doFrameSet;
    cmds["spawn"] = doSpawn;
    cmds["area-on"] = doAreaOn;
    cmds["status"] = doStatus;
    cmds["wait"] = doWait;
    cmds["timeout"] = doTimeout;
    cmds["kill"] = doKill;

    std::string cm;
    while (in >> cm) {
      auto cmd = cmds.find(cm);
      if (cmd == cmds.end()) {
        out << "<UNKNOWN COMMAND>\n";
        detail::skipLine(in);
        continue;
      }
      try {
        cmd->second(in, out, st);
      } catch (const std::system_error& e) {
        std::cerr << e.what() << '\n';
        out << "<INVALID COMMAND>\n";
        detail::skipLine(in);
      } catch (const std::exception&) {
        out << "<INVALID COMMAND>\n";
        detail::skipLine(in);
      }
    }
  } catch (...) {
    detail::shutdown(st);
    throw;
  }
  detail::shutdown(st);
}

void kuznetsov::doCircle(std::istream& in, std::ostream&, state_t& st)
{
  using detail::arg;
  std::string name;
  int r = 0, x = 0, y = 0;
  in >> detail::nameArg(name) >> detail::positiveArg(r) >> arg(x) >> arg(y);
  detail::requireLineEnd(in);
  detail::addShape(st, name, ellipse_t{0.0, r, r, x, y, true});
}

void kuznetsov::doEllipse(std::istream& in, std::ostream&, state_t& st)
{
  using detail::arg;
  std::string name;
  int rx = 0, ry = 0, x = 0, y = 0;
  in >> detail::nameArg(name) >> detail::positiveArg(rx) >> detail::positiveArg(ry) >> arg(x) >> arg(y);
  detail::requireLineEnd(in);
  detail::addShape(st, name, ellipse_t{0.0, rx, ry, x, y, false});
}

void kuznetsov::doRotate(std::istream& in, std::ostream&, state_t& st)
{
  using detail::arg;
  std::string name;
  double ksi = 0.0;
  in >> arg(name) >> arg(ksi);
  detail::requireLineEnd(in);
  ellipse_t& el = st.shapes.at(name);
  constexpr double full_turn = 2.0 * M_PI;
  double angle = std::fmod(el.angle + ksi, full_turn);
  if (angle < 0.0) {
    angle += full_turn;
  }
  el.angle = angle;
}

void kuznetsov::doSet(std::istream& in, std::ostream&, state_t& st)
{
  using detail::arg;
  std::string name;
  long size = 0;
  in >> detail::nameArg(name) >> detail::positiveArg(size);
  if (!in) {
    throw std::invalid_argument("bad set header");
  }
  set_t set{};
  set.shapes.reserve(size);
  for (long i = 0; i < size; ++i) {
    std::string shape;
    if (!(in >> arg(shape))) {
      throw std::invalid_argument("not enough shapes");
    }
    set.shapes.push_back(&st.shapes.at(shape));
  }
  detail::requireLineEnd(in);
  if (st.sets.count(name)) {
    throw std::invalid_argument("set exists");
  }
  st.sets.emplace(name, std::move(set));
}

void kuznetsov::doShow(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  detail::printShape(out, st.shapes.at(name));
}

void kuznetsov::doShowSet(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  const set_t& set = st.sets.at(name);
  for (auto it = set.shapes.cbegin(); it != set.shapes.cend(); ++it) {
    detail::printShape(out, **it);
  }
}

void kuznetsov::doFrame(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  const ellipse_t& el = st.shapes.at(name);
  detail::printRect(out, bounds(&el, &el + 1));
}

void kuznetsov::doFrameSet(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  const set_t& set = st.sets.at(name);
  std::vector< ellipse_t > shapes;
  shapes.reserve(set.shapes.size());
  for (auto it = set.shapes.cbegin(); it != set.shapes.cend(); ++it) {
    shapes.push_back(**it);
  }
  detail::printRect(out, bounds(shapes.data(), shapes.data() + shapes.size()));
}

void kuznetsov::doSpawn(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  long seed = 0;
  in >> detail::nameArg(name) >> detail::arg(seed);
  detail::requireLineEnd(in);
  if (seed < 0 || st.procs.count(name)) {
    throw std::invalid_argument("bad spawn");
  }
  auto ins = st.procs.emplace(name, proc_t{-1, -1, -1, false});
  try {
    ins.first->second = detail::startProc(out, st, static_cast< size_t >(seed));
  } catch (...) {
    st.procs.erase(ins.first);
    throw;
  }
}

void kuznetsov::doAreaOn(std::istream& in, std::ostream&, state_t& st)
{
  using detail::arg;
  std::string proc_name, calc_name, set_name;
  long threads = 0, tries = 0;
  in >> arg(proc_name) >> detail::nameArg(calc_name) >> arg(set_name);
  in >> detail::positiveArg(threads) >> detail::positiveArg(tries);
  detail::requireLineEnd(in);
  if (st.calcs.count(calc_name)) {
    throw std::invalid_argument("calculation exists");
  }
  const set_t& set = st.sets.at(set_name);
  const proc_t& proc = st.procs.at(proc_name);
  if (!proc.alive) {
    throw std::invalid_argument("process is terminated");
  }
  const size_t id = st.next_id++;
  const std::string shm = "/kuznetsov-" + std::to_string(getpid()) + "-" + std::to_string(id);
  auto ins = st.calcs.emplace(calc_name, calc_t{proc_name, id, calc_state_t::progress, 0.0, 0.0});

  bool delivered = false;
  response_t ack{};
  try {
    writeSetShm(shm, set);
    const detail::ShmUnlinker unlinker(shm.c_str());
    request_t req{};
    req.kind = request_kind_t::calc;
    req.calc_id = id;
    req.threads = static_cast< size_t >(threads);
    req.tries = static_cast< size_t >(tries);
    req.count = set.shapes.size();
    std::strncpy(req.shm_name, shm.c_str(), shm_name_size - 1);
    delivered = exchange(proc.cmd_wr, proc.res_rd, req, ack) && ack.calc_id == id;
  } catch (...) {
    st.calcs.erase(ins.first);
    throw;
  }
  if (!delivered) {
    st.calcs.erase(ins.first);
    detail::markDead(st, proc_name);
    throw std::runtime_error("process does not respond");
  }
  if (ack.state != calc_state_t::progress) {
    st.calcs.erase(ins.first);
    throw std::runtime_error("calculation was not started");
  }
}

void kuznetsov::doStatus(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  calc_t& calc = st.calcs.at(name);
  detail::queryCalc(st, calc, request_kind_t::status, 0);
  detail::printStatus(out, name, calc);
}

void kuznetsov::doWait(std::istream& in, std::ostream& out, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  calc_t& calc = st.calcs.at(name);
  detail::queryCalc(st, calc, request_kind_t::wait, 0);
  detail::printStatus(out, name, calc);
}

void kuznetsov::doTimeout(std::istream& in, std::ostream& out, state_t& st)
{
  using detail::arg;
  std::string name;
  long ms = 0;
  in >> arg(name) >> arg(ms);
  detail::requireLineEnd(in);
  if (ms < 0) {
    throw std::invalid_argument("bad timeout");
  }
  calc_t& calc = st.calcs.at(name);
  detail::queryCalc(st, calc, request_kind_t::timeout, static_cast< size_t >(ms));
  detail::printStatus(out, name, calc);
}

void kuznetsov::doKill(std::istream& in, std::ostream&, state_t& st)
{
  std::string name;
  in >> detail::arg(name);
  detail::requireLineEnd(in);
  proc_t& proc = st.procs.at(name);
  if (!proc.alive) {
    throw std::invalid_argument("process is terminated");
  }

  calc_t* current = nullptr;
  for (auto it = st.calcs.begin(); it != st.calcs.end(); ++it) {
    calc_t& calc = it->second;
    if (calc.proc != name || calc.state != calc_state_t::progress) {
      continue;
    }
    detail::queryCalc(st, calc, request_kind_t::status, 0);
    if (calc.state == calc_state_t::progress && (!current || calc.id < current->id)) {
      current = &calc;
    }
  }
  if (current) {
    detail::queryCalc(st, *current, request_kind_t::wait, 0);
  }
  if (!proc.alive) {
    return;
  }

  for (auto it = st.calcs.begin(); it != st.calcs.end(); ++it) {
    if (it->second.proc == name && it->second.state == calc_state_t::progress) {
      it->second.state = calc_state_t::canceled;
    }
  }
  request_t req{};
  req.kind = request_kind_t::kill;
  response_t ack{};
  exchange(proc.cmd_wr, proc.res_rd, req, ack);
  proc.alive = false;
  close(proc.cmd_wr);
  close(proc.res_rd);
  proc.cmd_wr = -1;
  proc.res_rd = -1;
  waitpid(proc.pid, nullptr, 0);
}
