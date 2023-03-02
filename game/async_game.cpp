#include "async_game.hpp"

namespace async_game {
  
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
      for (Job& j : ready) {
        Resume(j);
      }
    }
  }

  void Executor::Resume(Job& j) {
    j.handle.resume();
    if (j.handle.done()) {
      Job next = j.promise->next;
      j.handle.destroy();
      if (next.handle) {
        Resume(next);
      }
    }
  }

}