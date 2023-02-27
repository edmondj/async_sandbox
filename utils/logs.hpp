#pragma once

#include <sstream>

namespace utils {

  class Log {
  public:
    Log();
    ~Log();

    template<typename T>
    Log& operator<<(T&& t) {
      out << std::forward<T>(t);
      return *this;
    }

  private:
    std::ostringstream out;

    Log(const Log&) = delete;
    Log(Log&&) = delete;
    Log& operator=(const Log&) = delete;
    Log& operator=(Log&&) = delete;
  };

}