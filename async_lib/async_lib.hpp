#pragma once

#include <coroutine>
#include <optional>
#include <variant>
#include <cassert>

namespace async_lib {
  template<typename TExecutor>
  struct PromiseBase;

  template<typename TExecutor>
  struct Job {
    Job() = default;

    template<std::derived_from<PromiseBase<TExecutor>> TPromise>
    Job(TPromise* promise)
      : handle(std::coroutine_handle<TPromise>::from_promise(*promise))
      , promise(promise)
    {}

    template<std::derived_from<PromiseBase<TExecutor>> TPromise>
    Job(std::coroutine_handle<TPromise> handle)
      : handle(handle)
      , promise(&handle.promise())
    {}

    explicit operator bool() const { return static_cast<bool>(handle); }

    std::coroutine_handle<> handle;
    PromiseBase<TExecutor>* promise = nullptr;
  };

  template<typename TExecutor>
  struct PromiseBase {
    bool destroyOnDone = false;
    TExecutor* executor = nullptr;
    Job<TExecutor> parent;
  };

  template<typename TExecutor>
  void Resume(const Job<TExecutor>& job) {
    job.handle.resume();
    if (job.handle.done()) {
      Job<TExecutor> parent = job.promise->parent;
      if (job.promise->destroyOnDone) {
        job.handle.destroy();
      }
      if (parent) {
        Resume(parent);
      }
    }
  }

  template<typename T>
  concept ExecutorConcept = requires(T executor, const Job<T>& job) {
    { executor.Spawn(job) };
  };

  template<ExecutorConcept TExecutor, typename T = void>
  class Task;

  template<typename TExecutor, ExecutorConcept TaskExecutor>
    requires std::derived_from<TExecutor, TaskExecutor>
  void Spawn(TExecutor& executor, Task<TaskExecutor, void>&& task) {
    assert(task);
    auto* promise = std::exchange(task.promise, nullptr);
    promise->executor = &executor;
    promise->destroyOnDone = true;
    executor.Spawn(Job(promise));
  }

  template<ExecutorConcept TExecutor, typename T>
  struct Promise : PromiseBase<TExecutor> {
    auto get_return_object() { return Task<TExecutor, T>(this); }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    template<typename U>
    void return_value(U&& value) {
      result.emplace(std::forward<U>(value));
    }

    void unhandled_exception() {}

    std::optional<T> result;
  };

  template<ExecutorConcept TExecutor>
  struct Promise<TExecutor, void> : PromiseBase<TExecutor> {
    inline auto get_return_object() { return Task<TExecutor, void>(this); }
    inline std::suspend_always initial_suspend() { return {}; }
    inline std::suspend_always final_suspend() noexcept { return {}; }
    inline void return_void() {}
    inline void unhandled_exception() {}
  };

  template<ExecutorConcept TExecutor, typename T>
  Job(Promise<TExecutor, T>*) -> Job<TExecutor>;

  template<ExecutorConcept TExecutor, typename T>
  Job(std::coroutine_handle<Promise<TExecutor, T>>) -> Job<TExecutor>;

  template<ExecutorConcept TExecutor, typename T>
  class Task {
  public:
    using promise_type = Promise<TExecutor, T>;

    Task() = default;

    explicit Task(promise_type* promise)
      : promise(promise)
    {}

    Task(Task&& other)
      : promise(std::exchange(other.promise, nullptr))
    {}

    Task& operator=(Task&& other) {
      std::swap(promise, other.promise);
      return *this;
    }

    ~Task() {
      assert(!promise);
    }

    explicit operator bool() const { return promise; }

    promise_type* promise = nullptr;
  };

  template<ExecutorConcept TExecutor, typename T>
  class TaskAwait {
  public:
    using promise_type = typename Task<TExecutor, T>::promise_type;

    explicit TaskAwait(promise_type* promise)
      : m_promise(promise)
    {}

    TaskAwait(const TaskAwait&) = delete;
    TaskAwait(TaskAwait&&) = delete;
    TaskAwait& operator=(const TaskAwait&) = delete;
    TaskAwait& operator=(TaskAwait&&) = delete;

    ~TaskAwait() {
      std::coroutine_handle<promise_type>::from_promise(*m_promise).destroy();
    }

    bool await_ready() { return std::coroutine_handle<promise_type>::from_promise(*m_promise).done(); }

    template<std::derived_from<PromiseBase<TExecutor>> TPromise>
    bool await_suspend(std::coroutine_handle<TPromise> parent) {
      m_promise->parent = Job(parent);
      // The task hasn't been started yet
      if (!m_promise->executor) {
        m_promise->executor = parent.promise().executor;
        auto cur = std::coroutine_handle<promise_type>::from_promise(*m_promise);
        cur.resume();
        return !cur.done();
      }
      // The task was already started
      assert(m_promise->executor == parent.promise().executor);
      return true;
    }

    T await_resume() {
      if constexpr (!std::is_void_v<T>) {
        assert(m_promise->result);
        return std::move(m_promise->result).value();
      }
    }

  private:
    promise_type* m_promise;
  };

  template<ExecutorConcept TExecutor, typename T>
  auto operator co_await(Task<TExecutor, T>&& task) {
    return TaskAwait<TExecutor, T>(std::exchange(task.promise, nullptr));
  }


  template<ExecutorConcept TaskExecutor, typename T>
  class StartSubroutine {
  public:
    using promise_type = typename Task<TaskExecutor, T>::promise_type;

    explicit StartSubroutine(Task<TaskExecutor, T>&& task)
      : m_task(std::move(task))
    {}

    StartSubroutine(const StartSubroutine&) = delete;
    StartSubroutine(StartSubroutine&&) = delete;
    StartSubroutine& operator=(const StartSubroutine&) = delete;
    StartSubroutine& operator=(StartSubroutine&&) = delete;

    bool await_ready() { return false; }

    template<std::derived_from<PromiseBase<TaskExecutor>> TPromise>
    bool await_suspend(std::coroutine_handle<TPromise> parent) {
      m_task.promise->executor = parent.promise().executor;
      auto cur = std::coroutine_handle<promise_type>::from_promise(*m_task.promise);
      cur.resume();
      return false;
    }

    Task<TaskExecutor, T> await_resume() {
      return std::move(m_task);
    }

  private:
    Task<TaskExecutor, T> m_task;
  };
}