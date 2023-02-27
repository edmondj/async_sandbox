#include "logs.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace utils {

  Log::Log() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << '[' << std::this_thread::get_id() << "] <" << std::put_time(std::localtime(&time), "%F %T.") << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000 << "> ";
  }

  Log::~Log() {
    std::cout << out.str() << std::endl;
  }

}