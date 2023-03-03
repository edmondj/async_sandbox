#pragma once

#include "async_game.hpp"
#include <async_grpc/client.hpp>

namespace async_game {

  class GrpcCoroutine {
  public:
    inline explicit GrpcCoroutine(async_grpc::ClientExecutor& executor, async_grpc::Coroutine&& routine)
      : m_executor(executor)
      , m_routine(std::move(routine))
    {}

    inline bool await_ready() { return false; }

    template<std::derived_from<async_game::PromiseBase> TPromise>
    void await_suspend(std::coroutine_handle<TPromise> h) {
      async_grpc::Coroutine::Spawn(m_executor, Launcher(h));
    }

    inline bool await_resume() { return m_ok; }

  private:

    template<std::derived_from<async_game::PromiseBase> TPromise>
    async_grpc::Coroutine Launcher(std::coroutine_handle<TPromise> h) {
      m_ok = co_await std::move(m_routine);
      h.promise().MarkReady();
    }

    async_grpc::ClientExecutor& m_executor;
    async_grpc::Coroutine m_routine;
    bool m_ok = false;
  };

}