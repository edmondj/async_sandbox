#include <coroutine>
#include "async_grpc.hpp"

namespace async_grpc {

  Executor::Executor(std::unique_ptr<grpc::CompletionQueue> cq)
    : m_cq(std::move(cq))
  {}

  bool Executor::Poll()
  {
    return Tick(GetCq());
  }

  void Executor::Shutdown()
  {
    m_cq->Shutdown();
  }

  grpc::CompletionQueue* Executor::GetCq() const
  {
    return m_cq.get();
  }

  bool Executor::Tick(grpc::CompletionQueue* cq)
  {
    void* tag = nullptr;
    bool ok = false;
    if (!cq->Next(&tag, &ok)) {
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

}