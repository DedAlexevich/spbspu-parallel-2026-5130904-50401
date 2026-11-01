#include <csignal>
#include <exception>
#include <iostream>

#include <signal.h>

#include "cli.hpp"

int main()
{
  struct sigaction act{};
  act.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &act, nullptr) < 0) {
    std::cerr << "sigaction failed\n";
    return 1;
  }
  try {
    kuznetsov::cliProcess(std::cin, std::cout);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
