#include <coroutine>
#include "async_grpc.hpp"

namespace async_grpc {
  
  const char* StatusCodeString(grpc::StatusCode code) {
    switch (code) {
    case grpc::StatusCode::OK: return "OK";
    case grpc::StatusCode::CANCELLED: return "CANCELLED";
    case grpc::StatusCode::UNKNOWN: return "UNKNOWN";
    case grpc::StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case grpc::StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
    case grpc::StatusCode::NOT_FOUND: return "NOT_FOUND";
    case grpc::StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
    case grpc::StatusCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
    case grpc::StatusCode::UNAUTHENTICATED: return "UNAUTHENTICATED";
    case grpc::StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
    case grpc::StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
    case grpc::StatusCode::ABORTED: return "ABORTED";
    case grpc::StatusCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
    case grpc::StatusCode::UNIMPLEMENTED: return "UNIMPLEMENTED";
    case grpc::StatusCode::INTERNAL: return "INTERNAL";
    case grpc::StatusCode::UNAVAILABLE: return "UNAVAILABLE";
    case grpc::StatusCode::DATA_LOSS: return "DATA_LOSS";
    default: return "UNKNOWN";
    }
  }
  
  bool CompletionQueueTick(grpc::CompletionQueue* cq)
  {
    void* tag = nullptr;
    bool ok = false;
    if (!cq->Next(&tag, &ok)) {
      return false;
    }
    auto h = std::coroutine_handle<Coroutine::promise_type>::from_address(tag);
    h.promise().cancelled = !ok;
    Coroutine::Resume(h);
    return true;
  }

  void CompletionQueueShutdown(grpc::CompletionQueue* cq)
  {
    cq->Shutdown();
  }

}