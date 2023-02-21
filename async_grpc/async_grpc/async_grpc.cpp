#include <coroutine>
#include "threads.hpp"
#include "async_grpc.hpp"

namespace async_grpc {

  CompletionQueueThread::CompletionQueueThread(grpc::CompletionQueue* cq)
    : m_thread([cq]() { auto threadId = ScopedGrpcThread(); while (Tick(cq)); })
  {}

  bool CompletionQueueThread::Tick(grpc::CompletionQueue* cq)
  {
    void* tag = nullptr;
    bool ok = false;
    if (!cq->Next(&tag, &ok)) {
      return false;
    }
    auto h = std::coroutine_handle<BaseGrpcPromise>::from_address(tag);
    h.promise().lastOk = ok;
    h.resume();
    if (h.done()) {
      h.destroy();
    }
    return true;
  }

}