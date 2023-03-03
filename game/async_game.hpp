#pragma once

#include <cassert>
#include <coroutine>
#include <utility>
#include <optional>
#include <mutex>
#include <vector>
#include <variant>

namespace async_game {
  class Executor;

  struct PromiseBase;

  struct Job {
    std::coroutine_handle<> handle;
    PromiseBase* promise = nullptr;
  };

  struct PromiseBase {
    void* resultStorage = nullptr;
    Executor* executor = nullptr;
    Job next;
  };

  template<typename T>
  class Task {
  public:
    struct promise_type;

    Task() = default;

    explicit Task(promise_type* promise)
      : promise(promise)
    {}

    Task(Task&& other)
      : promise(std::exchange(other.promise, nullptr))
    {}

    Task& operator=(Task&& other) {
      std::swap(promise, other.promise);
      return nullptr;
    }

    ~Task() {
      if (promise) {
        std::coroutine_handle<promise_type>::from_promise(*promise).destroy();
      }
    }

    promise_type* promise = nullptr;
  };

  template<typename T>
  struct Task<T>::promise_type : PromiseBase {
    Task<T> get_return_object() { return Task<T>(this); }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    template<typename U>
    void return_value(U&& value) {
      assert(resultStorage);
      reinterpret_cast<std::optional<T>*>(resultStorage)->emplace(std::forward<U>(value));
    }

    void unhandled_exception() {}

    void MarkReady() {
      this->executor->MarkReady({ std::coroutine_handle<promise_type>::from_promise(*this), this });
    }
  };

  template<>
  struct Task<void>::promise_type : PromiseBase {
    inline Task<void> get_return_object() { return Task<void>(this); }
    inline std::suspend_always initial_suspend() { return {}; }
    inline std::suspend_always final_suspend() noexcept { return {}; }
    inline void return_void() {}
    inline void unhandled_exception() {}
  };

  template<typename T>
  class TaskAwait {
  public:
    explicit TaskAwait(typename Task<T>::promise_type* promise)
      : m_promise(promise)
    {}

    TaskAwait(const TaskAwait&) = delete;
    TaskAwait(TaskAwait&&) = delete;
    TaskAwait& operator=(const TaskAwait&) = delete;
    TaskAwait& operator=(TaskAwait&&) = delete;

    ~TaskAwait() {

    }

    bool await_ready() { return false; }

    template<std::derived_from<PromiseBase> TPromise>
    bool await_suspend(std::coroutine_handle<TPromise> parent) {
      m_promise->executor = parent.promise().executor;
      m_promise->next = { parent, &parent.promise() };
      if constexpr (!std::is_void_v<T>) {
        m_promise->resultStorage = &m_result;
      }
      auto cur = std::coroutine_handle<typename Task<T>::promise_type>::from_promise(*m_promise);
      cur.resume();
      return !cur.done();
    }

    T await_resume() {
      if constexpr (!std::is_void_v<T>) {
        return std::move(m_result).value();
      }
    }

  private:
    typename Task<T>::promise_type* m_promise;
    [[no_unique_address]] std::conditional_t<std::is_void_v<T>, std::monostate, std::optional<T>> m_result;
  };

  template<typename T>
  TaskAwait<T> operator co_await(Task<T>&& task) {
    return TaskAwait<T>(std::exchange(task.promise, nullptr));
  }

  using Coroutine = Task<void>;

  class Executor {
  public:

    void MarkReady(const Job& job);

    template<std::invocable<> TFunc>
    void Spawn(TFunc&& func) { Spawn([](TFunc func) -> Coroutine { co_await func(); }(std::forward<TFunc>(func))); }

    void Spawn(Coroutine&& coroutine);

    void Update();

  private:
    void Resume(Job& j);

    std::mutex m_lock;
    std::vector<Job> m_ready;
  };

}