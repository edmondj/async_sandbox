#pragma once

#include <atomic>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "service_impl.hpp"
#include "async_grpc.hpp"

namespace async_grpc {

  struct ServerOptions {
    std::vector<std::string> addresses;
    std::vector<std::reference_wrapper<IServiceImpl>> services;
    size_t executorCount = 2;
  };

  class Server {
  public:
    explicit Server(const ServerOptions& options);

  private:
    std::unique_ptr<grpc::Server> m_server;
    
    struct Executor {
      CompletionQueueThread thread;
      std::unique_ptr<grpc::ServerCompletionQueue> cq;
    };
    std::vector<Executor> m_executors;
    std::atomic<size_t> m_nextExecutor = 0;
  };
}