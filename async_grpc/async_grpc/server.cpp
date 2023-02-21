#include "server.hpp"
#include "threads.hpp"

namespace async_grpc {

  Server::Server(const ServerOptions& options)
  {
    grpc::ServerBuilder builder;
    for (const std::string& addr : options.addresses) {
      builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    }
    for (const IServiceImpl& service : options.services) {
      builder.RegisterService(service.GetService());
    }
    for (size_t i = 0; i < options.executorCount; ++i) {
      auto& executor = m_executors.emplace_back();
      executor.cq = builder.AddCompletionQueue();
      executor.thread = CompletionQueueThread(executor.cq.get());
    }
    m_server = builder.BuildAndStart();
  }

}