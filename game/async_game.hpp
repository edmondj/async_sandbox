#pragma once

#include <async_lib/async_lib.hpp>
#include <vector>
#include <mutex>

namespace async_game {
  
  class Executor;
  using Job = async_lib::Job<Executor>;

  class Executor {
  public:

    void Spawn(const Job& job);
    void MarkReady(const Job& job);

    void Update();

  private:
    std::mutex m_lock;
    std::vector<Job> m_ready;
  };

  template<typename T = void>
  using Task = async_lib::Task<Executor, T>;
}