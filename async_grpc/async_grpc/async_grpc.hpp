#pragma once

#include <thread>
#include <concepts>
#include <coroutine>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>

namespace async_grpc {

  const char* GrpcStatusCodeString(grpc::StatusCode code);

  template<typename T>
  concept ServiceConcept = std::derived_from<typename T::AsyncService, grpc::Service>&& requires {
    typename T::Stub;
  };

  class Coroutine {
  public:
    struct promise_type {
      inline Coroutine get_return_object() { return {this}; }
      inline std::suspend_never initial_suspend() noexcept { return {}; };
      inline std::suspend_always final_suspend() noexcept {
        if (next) {
          next.promise().lastOk = lastOk;
          next.resume();
        }
        return {};
      };
      void return_void() {}
      void unhandled_exception() {}

      bool lastOk = false;
      std::coroutine_handle<Coroutine::promise_type> next;
    };

    inline bool await_ready() { return false; }
    inline void await_suspend(std::coroutine_handle<Coroutine::promise_type> next) {
      promise->next = next;
    }
    inline bool await_resume() { return promise->lastOk; }

    promise_type* promise;
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

  template<typename T>
  concept ExecutorConcept = std::move_constructible<T> && requires(const T executor) {
    { executor.GetCq() } -> std::same_as<grpc::CompletionQueue*>;
  };

  bool CompletionQueueTick(grpc::CompletionQueue* cq);
  void CompletionQueueShutdown(grpc::CompletionQueue* cq);

  template<ExecutorConcept TExecutor>
  class Executor : public TExecutor {
  public:
    Executor(TExecutor executor)
      : TExecutor(std::move(executor))
    {}

    bool Poll() {
      return CompletionQueueTick(this->GetCq());
    }

    void Shutdown() {
      this->GetCq()->Shutdown();
    }
  };

  template<typename TDeadline>
  class Alarm {
  public:
    Alarm() = default;

    template<ExecutorConcept TExecutor>
    Alarm(const TExecutor& executor, TDeadline deadline)
      : m_cq(executor.GetCq())
      , m_deadline(std::move(deadline))
    {}

    auto Start() {
      return Awaitable([&](void* tag) {
        m_alarm.Set(m_cq, m_deadline, tag);
      });
    }

    void Cancel() {
      m_alarm.Cancel();
    }

    auto operator co_await() { return Start(); }

  private:
    grpc::CompletionQueue* m_cq = nullptr;
    TDeadline m_deadline;
    grpc::Alarm m_alarm;
  };

  // A thread running an executor Poll loop
  template<ExecutorConcept TExecutor>
  class ExecutorThread {
  public:
    explicit ExecutorThread(TExecutor executor = {})
      : m_executor(std::move(executor))
      , m_thread([cq = m_executor.GetCq()]() { while (CompletionQueueTick(cq)); })
    {}

    ExecutorThread(ExecutorThread&& other) = default;
    ExecutorThread& operator=(ExecutorThread&&) = default;
    
    ~ExecutorThread() {
      Shutdown();
    }

    void Shutdown() {
      CompletionQueueShutdown(m_executor.GetCq());
    }

    const TExecutor& GetExecutor() {
      return m_executor;
    }

  private:
    TExecutor m_executor;
    std::jthread m_thread;
  };
}