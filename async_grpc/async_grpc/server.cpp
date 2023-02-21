#include "server.hpp"
#include "threads.hpp"

namespace async_grpc {

  ServerContext::ServerContext(Server& server)
    : m_server(server)
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
    for (size_t i = 0; i < m_options.executorCount; ++i) {
      auto& executor = m_executors.emplace_back();
      executor.cq = builder.AddCompletionQueue();
      executor.thread = CompletionQueueThread(executor.cq.get());
    }
    m_server = builder.BuildAndStart();

    for (IServiceImpl& service : m_options.services) {
      service.StartListening(*this);
    }
  }

  Server::~Server() {
    m_server->Shutdown();
    for (Executor& executor : m_executors) {
      executor.cq->Shutdown();
    }
  }
}