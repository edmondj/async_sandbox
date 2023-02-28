#pragma once

#include <sstream>

namespace utils {

  class Log {
  public:
    Log();
    ~Log();
    
    Log(Log&& other) noexcept;

    template<typename T>
    Log& operator<<(T&& t)& {
      out << std::forward<T>(t);
      return *this;
    }

    template<typename T>
    Log&& operator<<(T&& t) && {
      out << std::forward<T>(t);
      return std::move(*this);
    }

  private:
    std::ostringstream out;

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log& operator=(Log&&) = delete;
  };

}