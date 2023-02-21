#pragma once

#include <thread>

namespace async_grpc {

  struct ScopedGrpcThread {
    ScopedGrpcThread();
    ~ScopedGrpcThread();

  private:
    ScopedGrpcThread(const ScopedGrpcThread&) = delete;
    ScopedGrpcThread(ScopedGrpcThread&&) = delete;
    ScopedGrpcThread& operator=(const ScopedGrpcThread&) = delete;
    ScopedGrpcThread& operator=(ScopedGrpcThread&&) = delete;
  };

  bool ThisThreadIsGrpc();
}