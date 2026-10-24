#ifndef CLI_H
#define CLI_H
#include <cstddef>
#include <ios>
#include <iosfwd>
#include <istream>
#include <map>
#include <string>

#include <sys/types.h>

#include "interprocess_connect.h"

namespace kuznetsov {
  struct proc_t {
    pid_t pid;
    int cmd_wr;
    int res_rd;
    bool alive;
  };

  struct calc_t {
    std::string proc;
    size_t id;
    calc_state_t state;
    double ms;
    double area;
  };

  struct State {
    std::map< std::string, circle_t > circles;
    std::map< std::string, set_t > sets;
    std::map< std::string, proc_t > procs;
    std::map< std::string, calc_t > calcs;
    size_t next_id;
  };

  using cmd_t = void(*)(std::istream&, std::ostream&, State&);

  void cliProcess(std::istream& in, std::ostream& out);

  void doCircle(std::istream& in, std::ostream& out, State& st);
  void doSet(std::istream& in, std::ostream& out, State& st);
  void doShow(std::istream& in, std::ostream& out, State& st);
  void doShowSet(std::istream& in, std::ostream& out, State& st);
  void doFrame(std::istream& in, std::ostream& out, State& st);
  void doFrameSet(std::istream& in, std::ostream& out, State& st);
  void doSpawn(std::istream& in, std::ostream& out, State& st);
  void doAreaOn(std::istream& in, std::ostream& out, State& st);
  void doStatus(std::istream& in, std::ostream& out, State& st);
  void doWait(std::istream& in, std::ostream& out, State& st);

  namespace detail {
    class IOGuard {
    public:
      explicit IOGuard(std::basic_ios< char >& s);
      ~IOGuard();
    private:
      std::basic_ios< char >& s_;
      std::streamsize width_;
      std::streamsize precision_;
      std::basic_ios< char >::fmtflags fmt_;
      char fill_;
    };

    template< class T >
    struct LineArg {
      T& ref;
    };

    struct NameArg {
      std::string& ref;
    };

    template< class T >
    struct PositiveArg {
      T& ref;
    };

    bool atLineEnd(std::istream& in);
    void requireLineEnd(std::istream& in);
    void skipLine(std::istream& in);
    bool isValidName(const std::string& name);

    void printCircle(std::ostream& out, const circle_t& c);
    void printRect(std::ostream& out, const rect_t& r);
    void printStatus(std::ostream& out, const std::string& name, const calc_t& calc);

    void markDead(State& st, const std::string& name);
    void queryCalc(State& st, calc_t& calc, request_kind_t kind);
    void closeOtherChildren(const State& st);
    proc_t startProc(std::ostream& out, const State& st, size_t seed);
    void shutdown(State& st);

    template< class T >
    std::istream& operator>>(std::istream& in, LineArg< T >&& arg)
    {
      std::istream::sentry s(in, true);
      if (!s) {
        return in;
      }
      if (atLineEnd(in)) {
        in.setstate(std::ios_base::failbit);
        return in;
      }
      return in >> arg.ref;
    }

    template< class T >
    LineArg< T > arg(T& ref)
    {
      return LineArg< T >{ref};
    }

    std::istream& operator>>(std::istream& in, NameArg&& arg);
    NameArg nameArg(std::string& ref);

    template< class T >
    std::istream& operator>>(std::istream& in, PositiveArg< T >&& arg)
    {
      T value{};
      if (!(in >> LineArg< T >{value})) {
        return in;
      }
      if (value > 0) {
        arg.ref = value;
      } else {
        in.setstate(std::ios_base::failbit);
      }
      return in;
    }

    template< class T >
    PositiveArg< T > positiveArg(T& ref)
    {
      return PositiveArg< T >{ref};
    }
  }

}

#endif
