#pragma once

#include <thread>
#include <concepts>
#include <coroutine>
#include <variant>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>

std::ostream& operator<<(std::ostream& out, const grpc::Status& status);

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
    enum class Status {
      Unstarted,
      Suspended,
      Cancelled,
      Done
    };

    struct promise_type {
      inline Coroutine get_return_object() { return Coroutine(this); }
      inline std::suspend_always initial_suspend() noexcept { return {}; };
      inline std::suspend_always final_suspend() noexcept { return {}; };
      void return_void() {}
      void unhandled_exception() {}

      grpc::CompletionQueue* completionQueue = nullptr;
      std::coroutine_handle<promise_type> parent;
      bool cancelled = false;
      Status* statusStorage = nullptr;

      Coroutine await_transform(Coroutine&& child) {
        child.m_promise->completionQueue = completionQueue;
        if (child.m_status == Status::Unstarted) {
          Resume(std::coroutine_handle<promise_type>::from_promise(*child.m_promise));
        }
        if (child.m_status == Status::Suspended) {
          child.m_promise->parent = std::coroutine_handle<promise_type>::from_promise(*this);
        }
        else {
          child.m_promise = nullptr;
        }
        return std::move(child);
      }

      void await_transform(Coroutine&) = delete;
      void await_transform(const Coroutine&) = delete;
      void await_transform(const Coroutine&&) = delete;

      template<typename TAwaitable>
      decltype(auto) await_transform(TAwaitable&& t) {
        return std::forward<TAwaitable>(t);
      }
    };

    // Ctor only used from promise_type::get_return_object
    inline explicit Coroutine(promise_type* promise)
      : m_promise(promise)
    {
      m_promise->statusStorage = &m_status;
    }

    Coroutine() = default;
    Coroutine(Coroutine&& other) noexcept
      : m_promise(std::exchange(other.m_promise, nullptr))
      , m_status(std::exchange(other.m_status, Status::Unstarted))
    {
      if (m_promise) {
        m_promise->statusStorage = &m_status;
      }
    }

    Coroutine& operator=(Coroutine&& other) noexcept {
      assert(!m_promise);
      assert(m_status != Status::Suspended);

      m_promise = std::exchange(other.m_promise, nullptr);
      m_status = std::exchange(other.m_status, Status::Unstarted);
      if (m_promise) {
        m_promise->statusStorage = &m_status;
      }
      return *this;
    }

    ~Coroutine() {
      assert(!m_promise);
    }

    explicit operator bool() const { return m_promise; }

    inline bool await_ready() { return m_status != Status::Suspended; }
    inline void await_suspend(std::coroutine_handle<promise_type> parent) {
      assert(m_promise);
      m_promise->parent = parent;
      m_promise = nullptr;
    }
    inline bool await_resume() { return m_status == Status::Done; }

    static Status GetStatus(std::coroutine_handle<promise_type> h) {
      assert(h);
      if (h.done()) {
        if (h.promise().cancelled) {
          return Status::Cancelled;
        }
        return Status::Done;
      }
      return Status::Suspended;
    }

    static void Resume(std::coroutine_handle<promise_type> h) {
      h.resume();
      Status res = GetStatus(h);
      if (h.promise().statusStorage) {
        *h.promise().statusStorage = res;
      }
      if (res != Status::Suspended) {
        std::coroutine_handle<promise_type> parent = h.promise().parent;
        bool cancelled = h.promise().cancelled;
        h.destroy();
        if (parent) {
          parent.promise().cancelled = cancelled;
          Resume(parent);
        }
      }
    }

    template<ExecutorConcept TExecutor>
    static void Spawn(TExecutor& executor, Coroutine&& coroutine) {
      promise_type* promise = std::exchange(coroutine.m_promise, nullptr);
      coroutine.m_status = Status::Done;
      promise->statusStorage = nullptr;
      promise->completionQueue = executor.GetCq();
      auto h = std::coroutine_handle<promise_type>::from_promise(*promise);
      Resume(h);
    }

    // Start a subroutine on the same executor as the current coroutine
    // Needs to be immediately co_await'ed
    // The subroutine will run start running immediatly
    struct [[nodiscard]] StartSubroutine {
      StartSubroutine(Coroutine&& coro)
        : m_promise(std::exchange(coro.m_promise, nullptr))
      {
        assert(m_promise);
        coro.m_status = Status::Done;
        m_promise->statusStorage = &m_status;
      }

      bool await_ready() { return false; }
      bool await_suspend(std::coroutine_handle<promise_type> cur) {
        m_promise->completionQueue = cur.promise().completionQueue;
        Coroutine::Resume(std::coroutine_handle<promise_type>::from_promise(*m_promise));
        return false; // always return to caller
      }

      Coroutine await_resume() {
        if (m_status == Status::Suspended) {
          return Coroutine(m_promise);
        }
        return Coroutine();
      }

    private:
      promise_type* m_promise;
      Status m_status = Status::Unstarted;
    };

  private:
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;

    promise_type* m_promise = nullptr;
    Status m_status = Status::Unstarted;
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

  template<typename T>
  constexpr bool IsAwaitable = false;
  template<typename T>
  constexpr bool IsAwaitable<Awaitable<T>> = true;

  template<typename T>
  concept IsAwaitableConcept = IsAwaitable<T> || IsAwaitable<decltype(std::declval<T>().operator co_await())>;

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

    Alarm(Alarm&&) = default;
    Alarm& operator=(Alarm&&) = default;

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