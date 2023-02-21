#pragma once

#include <thread>
#include <concepts>
#include <grpcpp/grpcpp.h>

namespace async_grpc {

  template<typename T>
  concept GrpcServiceConcept = std::derived_from<typename T::AsyncService, grpc::Service>;

  // All coroutine's promise ran inside a completion queue loop must inherit from this
  struct BaseGrpcPromise {
    bool lastOk = false;
  };

  // TFunc must be calling an action queuing the provided tag to a completion queue managed by CompletionQueueThread
  template<std::invocable<void*> TFunc>
  class GrpcAwaitable {
  public:
    GrpcAwaitable(TFunc func)
      : m_func(std::move(func))
    {}

    bool await_ready() {
      return false;
    }

    template<std::derived_from<BaseGrpcPromise> TPromise>
    void await_suspend(std::coroutine_handle<TPromise> h) {
      m_promise = &h.promise();
      m_func(h.address());
    }

    bool await_resume() {
      return m_promise->lastOk;
    }

  private:
    TFunc m_func;
    BaseGrpcPromise* m_promise = nullptr;
  };


  // A thread running a completion queue loop
  // The thread will stop when the associated completion queue is shut down
  // The associated completion queue must live longer than the thread
  class CompletionQueueThread {
  public:
    CompletionQueueThread() = default;
    explicit CompletionQueueThread(grpc::CompletionQueue* cq);

    CompletionQueueThread(CompletionQueueThread&&) = default;
    CompletionQueueThread& operator=(CompletionQueueThread&&) = default;
    ~CompletionQueueThread() = default;

    // Helper tick logic, mostly for debug
    static bool Tick(grpc::CompletionQueue* cq); 

  private:
    std::jthread m_thread;
  };
}