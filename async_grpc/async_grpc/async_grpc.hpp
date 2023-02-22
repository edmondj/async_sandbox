#pragma once

#include <cassert>
#include <thread>
#include <concepts>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>

namespace async_grpc {

  template<typename T>
  concept ServiceConcept = std::derived_from<typename T::AsyncService, grpc::Service>;


  struct Coroutine {
    struct promise_type {
      bool lastOk = false;

      inline Coroutine get_return_object() { return {}; }
      inline std::suspend_never initial_suspend() noexcept { return {}; };
      inline std::suspend_always final_suspend() noexcept { return {}; };
      void return_void() {}
      void unhandled_exception() {}
    };
  };

  // TFunc must be calling an action queuing the provided tag to a completion queue managed by CompletionQueueThread
  template<std::invocable<void*> TFunc>
  class Awaitable {
  public:
    Awaitable(TFunc func)
      : m_func(std::move(func))
    {}

    bool await_ready() {
      return false;
    }

    void await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_promise = &h.promise();
      m_func(h.address());
    }

    bool await_resume() {
      return m_promise->lastOk;
    }

  private:
    TFunc m_func;
    Coroutine::promise_type* m_promise = nullptr;
  };

  class Executor {
  public:
    Executor() = default;
    explicit Executor(std::unique_ptr<grpc::CompletionQueue> cq);

    bool Poll();
    void Shutdown();

    grpc::CompletionQueue* GetCq() const;

  private:
    std::unique_ptr<grpc::CompletionQueue> m_cq;
  };

  template<typename TDeadline>
  class Alarm {
  public:
    Alarm() = default;

    Alarm(Executor& executor, TDeadline deadline)
      : m_executor(&executor)
      , m_deadline(std::move(deadline))
    {}

    auto Start() {
      return Awaitable([&](void* tag) {
        m_alarm.Set(m_executor->GetCq(), m_deadline, tag);
      });
    }

    void Cancel() {
      m_alarm.Cancel();
    }

    auto operator co_await() { return Start(); }

  private:
    Executor* m_executor = nullptr;
    TDeadline m_deadline;
    grpc::Alarm m_alarm;
  };

  // A thread running an executor Poll loop
  // The thread will stop when the associated executor is shut down
  // The associated executor must live longer than the thread
  class ExecutorThread {
  public:
    ExecutorThread() = default;
    explicit ExecutorThread(Executor& executor);

    ExecutorThread(ExecutorThread&&) = default;
    ExecutorThread& operator=(ExecutorThread&&) = default;
    ~ExecutorThread() = default;

  private:
    std::jthread m_thread;
  };
}