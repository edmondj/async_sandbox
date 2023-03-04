#include "async_game.hpp"

namespace async_game {
  void Executor::Spawn(const Job& job) {
    MarkReady(job);
  }

  void Executor::MarkReady(const Job& job) {
    auto lock = std::unique_lock(m_lock);
    m_ready.push_back(job);
  }

  void Executor::Update() {
    while (true) {
      std::vector<Job> ready;
      {
        auto lock = std::unique_lock(m_lock);
        ready.swap(m_ready);
      }
      if (ready.empty()) {
        break;
      }
      for (const auto& job : ready) {
        async_lib::Resume(job);
      }
    }
  }

}