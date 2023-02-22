#include <coroutine>
#include "threads.hpp"
#include "async_grpc.hpp"

namespace async_grpc {

  ExecutorThread::ExecutorThread(Executor& executor)
    : m_thread([&executor]() {
      auto threadId = ScopedGrpcThread();
      while (executor.Poll())
        ;
    })
  {}

  Executor::Executor(std::unique_ptr<grpc::CompletionQueue> cq)
    : m_cq(std::move(cq))
  {}

  bool Executor::Poll()
  {
    void* tag = nullptr;
    bool ok = false;
    if (!m_cq->Next(&tag, &ok)) {
      return false;
    }
    auto h = std::coroutine_handle<Coroutine::promise_type>::from_address(tag);
    h.promise().lastOk = ok;
    h.resume();
    if (h.done()) {
      h.destroy();
    }
    return true;
  }

  void Executor::Shutdown()
  {
    m_cq->Shutdown();
  }

  grpc::CompletionQueue* Executor::GetCq() const
  {
    return m_cq.get();
  }

}