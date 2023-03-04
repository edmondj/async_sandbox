#include "server.hpp"

namespace async_grpc {

  ServerExecutor::ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> notifCq) noexcept
    : CompletionQueueExecutor(std::move(notifCq))
  {}

  grpc::ServerCompletionQueue* ServerExecutor::GetNotifCq()
  {
    return static_cast<grpc::ServerCompletionQueue*>(GetCq());
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
      m_executors.emplace_back(builder.AddCompletionQueue());
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

  ServerExecutor& Server::SelectNextExecutor()
  {
    // Round Robin on all executors, more algorithms possible
    return m_executors[m_nextExecutor++ % m_executors.size()].GetExecutor();
  }
}