#include "async_grpc.hpp"

namespace async_grpc {

  CompletionQueueExecutor::CompletionQueueExecutor()
    : m_cq(std::make_unique<grpc::CompletionQueue>())
  {}

  CompletionQueueExecutor::CompletionQueueExecutor(std::unique_ptr<grpc::CompletionQueue> cq) noexcept
    : m_cq(std::move(cq))
  {}

  CompletionQueueExecutor::~CompletionQueueExecutor() {
    if (m_cq) {
      Shutdown();
    }
  }

  grpc::CompletionQueue* CompletionQueueExecutor::GetCq()
  {
    return m_cq.get();
  }

  bool Tick(grpc::CompletionQueue* cq)
  {
    bool ok = false;
    void* tag = nullptr;
    if (!cq->Next(&tag, &ok)) {
      return false;
    }
    auto* job = reinterpret_cast<SuspendedJob*>(tag);
    job->ok = ok;
    assert(job->job);
    async_lib::Resume(job->job);
    return true;
  }
  
  std::jthread SpawnExecutorThread(grpc::CompletionQueue* cq) {
    return std::jthread([cq]() { while (Tick(cq)) {} });
  }

  void CompletionQueueExecutor::Shutdown()
  {
    m_cq->Shutdown();
  }

  void CompletionQueueExecutor::Spawn(const Job& job) {
    async_lib::Resume(job);
  }
}
