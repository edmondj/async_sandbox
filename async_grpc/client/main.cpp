#include <iostream>
#include <async_grpc/client.hpp>
#include <protos/echo_service.grpc.pb.h>

class EchoClient : public async_grpc::ClientBase<echo_service::EchoService> {
public:
  EchoClient(async_grpc::ChannelProvider channelProvider)
    : async_grpc::ClientBase<echo_service::EchoService>(std::move(channelProvider))
  {}

  ASYNC_GRPC_CLIENT_UNARY(UnaryEcho)
};

class Program {
public:
  Program(const std::shared_ptr<grpc::Channel>& channel)
    : m_echo(channel)
  {
#define ADD_HANDLER(client, rpc) m_handlers.insert_or_assign(std::string(client::Service::service_full_name()) + '.' + #rpc, &Program::Handle ## rpc)
    ADD_HANDLER(EchoClient, UnaryEcho);
#undef ADD_HANDLER
  }

  void Process(std::string_view line) {
    for (const auto& [name, handler] : m_handlers) {
      if (line.starts_with(name)) {
        (this->*handler)(name, line.substr(std::min(line.size(), name.size() + 1)));
        return;
      }
    }
    std::cout << "Unknown RPC" << std::endl;
  }

private:
  void LogTimestamp() {
    std::cout << '<' << std::chrono::system_clock::now() << "> ";
  }

  bool LogStatus(const grpc::Status& status) {
    std::cout << '[' << async_grpc::GrpcStatusCodeString(status.error_code()) << "] ";
    if (status.ok()) {
      return true;
    }
    std::cout << ' ' << status.error_message() << std::endl;
    return false;
  }

  async_grpc::Coroutine HandleUnaryEcho(std::string_view rpc, std::string_view command) {
    grpc::ClientContext context;
    echo_service::UnaryEchoRequest request;
    request.set_message(std::string(command));

    echo_service::UnaryEchoResponse response;
    grpc::Status status;
    if (co_await m_echo.UnaryEcho(m_executor.GetExecutor(), context, request, response, status)) {
      LogTimestamp();
      if (LogStatus(status)) {
        std::cout << response.message() << std::endl;
      }
    }
  }

  EchoClient m_echo;
  async_grpc::ClientExecutorThread m_executor;
  using THandler = async_grpc::Coroutine(Program::*)(std::string_view, std::string_view);
  std::map<std::string, THandler> m_handlers;
};

int main() {
  Program program(grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials()));

  for (std::string line; std::getline(std::cin, line);) {
    if (line == "quit") {
      break;
    }
    program.Process(line);
  }
}