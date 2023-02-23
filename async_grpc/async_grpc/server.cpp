#include "server.hpp"

namespace async_grpc {
  
  ServerExecutor::ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> cq)
    : m_cq(std::move(cq))
  {}

  grpc::CompletionQueue* ServerExecutor::GetCq() const {
    return m_cq.get();
  }

  grpc::ServerCompletionQueue* ServerExecutor::GetNotifCq() const {
    return m_cq.get();
  }

  ServerContext::ServerContext(const ServerExecutor& executor)
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
    m_executors.reserve(m_options.executorCount);
    for (size_t i = 0; i < m_options.executorCount; ++i) {
      m_executors.emplace_back(ServerExecutor(builder.AddCompletionQueue()));
    }
    m_server = builder.BuildAndStart();

    for (IServiceImpl& service : m_options.services) {
      service.StartListening(*this);
    }
  }

  Server::~Server() {
    Shutdown();
  }

  void Server::Shutdown()
  {
    m_server->Shutdown();
  }

  const ServerExecutor& Server::SelectNextExecutor()
  {
    // Round Robin on all executors, more algorithms possible
    return m_executors[m_nextExecutor++ % m_executors.size()].GetExecutor();
  }
}