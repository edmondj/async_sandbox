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

  Server::Server(ServerOptions&& options)
  {
    grpc::ServerBuilder builder;
    for (const std::string& addr : options.addresses) {
      builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    }
    for (IServiceImpl& service : options.services) {
      builder.RegisterService(service.GetGrpcService());
    }
    m_executors.reserve(options.executorCount);
    for (size_t i = 0; i < options.executorCount; ++i) {
      m_executors.emplace_back(ServerExecutor(builder.AddCompletionQueue()));
    }
    if (options.options) {
      builder.SetOption(std::move(options.options));
    }
    m_server = builder.BuildAndStart();

    for (IServiceImpl& service : options.services) {
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

  void Server::Spawn(ServerListenCoroutine&& coroutine)
  {
    ServerListenCoroutine::Spawn(SelectNextExecutor(), std::move(coroutine));
  }

  ServerExecutor& Server::SelectNextExecutor()
  {
    // Round Robin on all executors, more algorithms possible
    return m_executors[m_nextExecutor++ % m_executors.size()].GetExecutor();
  }
}