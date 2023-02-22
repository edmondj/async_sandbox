#include "server.hpp"
#include "threads.hpp"

namespace async_grpc {
  
  ServerExecutor::ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> cq)
    : Executor(std::move(cq)) {
  }

  grpc::ServerCompletionQueue* ServerExecutor::GetNotifCq() const {
    return static_cast<grpc::ServerCompletionQueue*>(GetCq());
  }

  ServerContext::ServerContext(ServerExecutor& executor)
    : executor(executor)
  {}

  Server::Server(ServerOptions options)
    : m_options(std::move(options))
  {
    grpc::ServerBuilder builder;
    for (const std::string& addr : m_options.addresses) {
      builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    }
    for (IServiceImpl& service : m_options.services) {
      builder.RegisterService(service.GetGrpcService());
    }
    // reserve is *needed*, to avoid moving references to executors that are used in executor threads
    m_executors.reserve(m_options.executorCount);
    for (size_t i = 0; i < m_options.executorCount; ++i) {
      auto& executor = m_executors.emplace_back();
      executor.executor = ServerExecutor(builder.AddCompletionQueue());
      executor.thread = ExecutorThread(executor.executor);
    }
    m_server = builder.BuildAndStart();

    for (IServiceImpl& service : m_options.services) {
      service.StartListening(*this);
    }
  }

  Server::~Server() {
    m_server->Shutdown();
    for (Executor& executor : m_executors) {
      executor.executor.Shutdown();
    }
  }
}