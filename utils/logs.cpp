#include "logs.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

namespace utils {

  Log::Log() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << '[' << std::this_thread::get_id() << "] <"
      << std::put_time(std::localtime(&time), "%F %T.")
      << std::setfill('0') << std::setw(3) << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000
      << "> ";
  }
  
  Log::Log(Log&& other) noexcept
    : out(std::move(other.out))
  {
    other.out.setstate(std::ios_base::badbit);
  }

  Log::~Log() {
    if (out) {
      std::cout << out.str() << std::endl;
    }
  }

}