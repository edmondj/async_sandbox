#pragma once

#include <thread>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <async_lib/async_lib.hpp>

namespace async_grpc {
  
  template<typename T>
  concept ServiceConcept = std::derived_from<typename T::AsyncService, grpc::Service>&& requires {
    typename T::Stub;
  };

  class CompletionQueueExecutor;

  template<typename T = void>
  using Task = async_lib::Task<CompletionQueueExecutor, T>;

  class CompletionQueueExecutor;
  using Job = async_lib::Job<CompletionQueueExecutor>;
  using PromiseBase = async_lib::PromiseBase<CompletionQueueExecutor>;

  class CompletionQueueExecutor {
  public:

    // Creates a new completion queue
    CompletionQueueExecutor();
    CompletionQueueExecutor(std::unique_ptr<grpc::CompletionQueue> cq) noexcept;

    CompletionQueueExecutor(CompletionQueueExecutor&&) = default;
    CompletionQueueExecutor& operator=(CompletionQueueExecutor&&) = default;

    ~CompletionQueueExecutor();

    grpc::CompletionQueue* GetCq();

    void Shutdown();

    void Spawn(const Job& job);

  private:
    std::unique_ptr<grpc::CompletionQueue> m_cq;
  };

  bool Tick(grpc::CompletionQueue* cq);

  std::jthread SpawnExecutorThread(grpc::CompletionQueue* cq);

  template<std::derived_from<CompletionQueueExecutor> TExecutor>
  class ExecutorThreads {
  public:
    explicit ExecutorThreads(size_t threadCounts)
      : ExecutorThreads(TExecutor{}, threadCounts)
    {}

    ExecutorThreads(TExecutor executor, size_t threadCounts)
      : m_executor(std::move(executor))
    {
      m_threads.reserve(threadCounts);
      for (size_t i = 0; i < threadCounts; ++i) {
        m_threads.push_back(SpawnExecutorThread(m_executor.GetCq()));
      }
    }

    ExecutorThreads(ExecutorThreads&&) = default;
    ExecutorThreads& operator=(ExecutorThreads&&) = default;

    ~ExecutorThreads() {
      Shutdown();
    }

    TExecutor& GetExecutor() { return m_executor; }
    void Shutdown() { m_executor.Shutdown(); }

  private:
    TExecutor m_executor;
    std::vector<std::jthread> m_threads;
  };


  struct SuspendedJob {
    Job job;
    bool ok = false;
  };

  struct AwaitData {
    grpc::CompletionQueue* cq;
    void* tag;
  };

  template<std::invocable<const AwaitData&> TFunc>
  class CompletionQueueAwaitable {
  public:
    using ReturnType = std::invoke_result_t<TFunc, const AwaitData&>;

    explicit CompletionQueueAwaitable(TFunc func)
      : m_func(std::move(func))
    {}

    bool await_ready() { return false; }

    template<std::derived_from<PromiseBase> TPromise>
    void await_suspend(std::coroutine_handle<TPromise> handle) {
      m_suspended = true;
      m_job.job = Job(handle);
      auto data = AwaitData{ m_job.job.promise->executor->GetCq(), &m_job };
      if constexpr (std::is_void_v<ReturnType>) {
        m_func(data);
      } else {
        m_result = m_func(data);
      }
    }

    auto await_resume() {
      m_suspended = false;
      if constexpr (std::is_void_v<ReturnType>) {
        return m_job.ok;
      } else {
        if (m_job.ok) {
          return std::move(m_result);
        }
        return std::optional<ReturnType>();
      }
    }

    ~CompletionQueueAwaitable() {
      assert(!m_suspended);
    }

  private:
    TFunc m_func;
    SuspendedJob m_job;
    bool m_suspended = false;
    [[no_unique_address]] std::conditional_t<std::is_void_v<ReturnType>, std::monostate, std::optional<ReturnType>> m_result;
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

    void SetDeadline(TDeadline deadline) {
      m_deadline = deadline;
    }

    auto Start() {
      return CompletionQueueAwaitable([this](const AwaitData& data) {
        m_alarm.Set(data.cq, m_deadline, data.tag);
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

}
