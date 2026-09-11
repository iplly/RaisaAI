#include "core/Process.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

std::string exec(std::string args) {
  std::cout << "exec: " << args << "\n\n";
  char buf[128];
  std::string result;
  struct FileCloser {
    void operator()(FILE *f) const { pclose(f); }
  };
  std::unique_ptr<FILE, FileCloser> pipe(popen(args.c_str(), "r"));
  if (!pipe)
    throw std::runtime_error("Ошибка popen");
  while (fgets(buf, sizeof(buf), pipe.get()) != nullptr) {
    result += buf;
  }
  return result;
}
