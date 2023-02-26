#pragma once

#include <thread>
#include <concepts>
#include <coroutine>
#include <variant>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>

namespace async_grpc {

  const char* StatusCodeString(grpc::StatusCode code);

  class Coroutine;

  template<typename T>
  concept ExecutorConcept = std::move_constructible<T> && requires(const T executor, Coroutine&& coro) {
    { executor.GetCq() } -> std::same_as<grpc::CompletionQueue*>;
  };

  template<typename T>
  concept ServiceConcept = std::derived_from<typename T::AsyncService, grpc::Service>&& requires {
    typename T::Stub;
  };

  // Must be either co_await'ed or given to Spawn
  class [[nodiscard]] Coroutine {
  public:
    enum class ResumeResult {
      Suspended,
      Cancelled,
      Done
    };

    struct promise_type {
      inline Coroutine get_return_object() { return {*this}; }
      inline std::suspend_always initial_suspend() noexcept { return {}; };
      inline std::suspend_always final_suspend() noexcept {
        if (parent) {
          parent.promise().cancelled = cancelled;
        }
        return {};
      };
      void return_void() {}
      void unhandled_exception() {}

      grpc::CompletionQueue* completionQueue = nullptr;
      bool cancelled = false;
      std::coroutine_handle<promise_type> parent;
      ResumeResult* resumeStorage = nullptr;
    };

    inline bool await_ready() { return false; }
    inline bool await_suspend(std::coroutine_handle<promise_type> parent) {
      m_promise.completionQueue = parent.promise().completionQueue;
      auto cur = std::coroutine_handle<promise_type>::from_promise(m_promise);
      ResumeResult res = Coroutine::Resume(cur);
      if (res != ResumeResult::Suspended) {
        parent.promise().cancelled = res == ResumeResult::Cancelled;
        return false;
      }
      m_promise.parent = parent;
      return true;
    }
    inline bool await_resume() {
      return !m_promise.cancelled;
    }

    static ResumeResult GetResult(std::coroutine_handle<promise_type> h) {
      if (h.done()) {
        if (h.promise().cancelled) {
          return ResumeResult::Cancelled;
        }
        return ResumeResult::Done;
      }
      return ResumeResult::Suspended;
    }

    static ResumeResult Resume(std::coroutine_handle<promise_type> h) {
      h.resume();
      ResumeResult res = GetResult(h);
      if (h.promise().resumeStorage) {
        *h.promise().resumeStorage = res;
      }
      if (res != ResumeResult::Suspended) {
        std::coroutine_handle<promise_type> parent = h.promise().parent;
        bool cancelled = h.promise().cancelled;
        if (parent) {
          parent.promise().cancelled = cancelled;
          Resume(parent);
        }
        h.destroy();
      }
      return res;
    }

    template<ExecutorConcept TExecutor>
    static void Spawn(TExecutor& executor, Coroutine&& coroutine) {
      coroutine.m_promise.completionQueue = executor.GetCq();
      auto h = std::coroutine_handle<promise_type>::from_promise(coroutine.m_promise);
      Resume(h);
    }

    struct [[nodiscard]] Subroutine {
    public:
      Subroutine() = default;
      Subroutine(promise_type& promise)
        : m_promise(&promise)
        , m_resume(GetResult(std::coroutine_handle<promise_type>::from_promise(promise)))
      {
        m_promise->resumeStorage = &m_resume;
      }
      Subroutine(Subroutine&& other) noexcept
        : m_promise(std::exchange(other.m_promise, nullptr))
        , m_resume(other.m_resume)
      {
        if (m_promise) {
          m_promise->resumeStorage = &m_resume;
        }
      }
      Subroutine& operator=(Subroutine&& other) noexcept
      {
        std::swap(m_promise, other.m_promise);
        if (m_promise) {
          m_promise->resumeStorage = &m_resume;
        }
        if (other.m_promise) {
          other.m_promise->resumeStorage = &other.m_resume;
        }
        return *this;
      }
      ~Subroutine() {
        assert(!m_promise);
      }

      explicit operator bool() const { return m_promise; }

      bool await_ready() { return m_resume != ResumeResult::Suspended; }
      void await_suspend(std::coroutine_handle<promise_type> parent) {
        m_promise->parent = parent;
      }
      inline bool await_resume() {
        m_promise = nullptr;
        return m_resume == ResumeResult::Done;
      }

    private:
      promise_type* m_promise = nullptr;
      ResumeResult m_resume = ResumeResult::Done;
    };

    // Start a subroutine on the same executor as the current coroutine
    // Needs to be immediately co_await'ed
    // The subroutine will run start running immediatly
    struct [[nodiscard]] StartSubroutine {
      StartSubroutine(Coroutine&& coro)
        : m_promise(coro.m_promise)
      {}

      bool await_ready() { return false; }
      bool await_suspend(std::coroutine_handle<promise_type> cur) {
        m_promise.completionQueue = cur.promise().completionQueue;
        Coroutine::Resume(std::coroutine_handle<promise_type>::from_promise(m_promise));
        return false;
      }

      Subroutine await_resume() {
        return Subroutine(m_promise);
      }

    private:
      promise_type& m_promise;
    };

  private:

    inline Coroutine(promise_type& promise)
      : m_promise(promise)
    {}

    Coroutine() = delete;
    Coroutine(const Coroutine&) = delete;
    Coroutine(Coroutine&&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;
    Coroutine& operator=(Coroutine&&) = delete;

    promise_type& m_promise;
  };

  // TFunc must be calling an action queuing the provided tag to a completion queue managed by CompletionQueueThread
  template<std::invocable<grpc::CompletionQueue*, void*> TFunc>
  class [[nodiscard]] Awaitable {
  public:
    Awaitable(TFunc func)
      : m_func(std::move(func))
    {}

    bool await_ready() {
      return false;
    }

    void await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_promise = &h.promise();
      m_func(m_promise->completionQueue, h.address());
    }

    bool await_resume() {
      return !m_promise->cancelled;
    }

  private:
    TFunc m_func;
    Coroutine::promise_type* m_promise = nullptr;
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

  template<typename T>
  concept TimePointConcept = requires(grpc::Alarm a, grpc::CompletionQueue * cq, T t, void* tag) {
    { a.Set(cq, t, tag) };
  };

  template<TimePointConcept TDeadline>
  class Alarm {
  public:
    Alarm() = default;

    Alarm(TDeadline deadline)
      : m_deadline(std::move(deadline))
    {}

    auto Start() {
      return Awaitable([&](grpc::CompletionQueue* cq, void* tag) {
        m_alarm.Set(cq, m_deadline, tag);
      });
    }

    void Cancel() {
      m_alarm.Cancel();
    }

    auto operator co_await() { return Start(); }

  private:
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

    TExecutor& GetExecutor() {
      return m_executor;
    }

    void Spawn(Coroutine&& coroutine) {
      Coroutine::Spawn(m_executor, std::move(coroutine));
    }

  private:
    TExecutor m_executor;
    std::jthread m_thread;
  };
}