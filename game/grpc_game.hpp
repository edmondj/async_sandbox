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
      async_grpc::Coroutine::Spawn(m_executor, [h, routine = std::move(m_routine)]() mutable -> async_grpc::Coroutine {
        co_await std::move(routine);
        h.promise().MarkReady();
      }());
    }

    inline void await_resume() {}

  private:
    async_grpc::ClientExecutor& m_executor;
    async_grpc::Coroutine m_routine;
  };

}