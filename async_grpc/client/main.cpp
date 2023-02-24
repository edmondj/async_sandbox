#include <iostream>
#include <async_grpc/client.hpp>
#include <protos/echo_service.grpc.pb.h>
#include <protos/variable_service.grpc.pb.h>

using EchoClient = async_grpc::Client<echo_service::EchoService>;
using VariableClient = async_grpc::Client<variable_service::VariableService>;

class Program {
public:
  Program(const std::shared_ptr<grpc::Channel>& channel)
    : m_echo(channel)
    , m_variable(channel)
  {
#define ADD_HANDLER(command) m_handlers.insert_or_assign(#command, &Program::command)
    ADD_HANDLER(UnaryEcho);
#undef ADD_HANDLER
  }

  void Process(std::string_view line) {
    for (const auto& [name, handler] : m_handlers) {
      if (line.starts_with(name)) {
        (this->*handler)();
        return;
      }
    }
    std::cout << "Unknown command" << std::endl;
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

  async_grpc::Coroutine UnaryEcho() {
    echo_service::UnaryEchoRequest request;
    request.set_message("hello");

    echo_service::UnaryEchoResponse response;
    grpc::Status status;
    std::unique_ptr<grpc::ClientContext> context;
    if (co_await m_echo.AutoRetryUnary(ASYNC_GRPC_CLIENT_START_FUNC(EchoClient, UnaryEcho), m_executor.GetExecutor(), context, request, response, status)) {
      LogTimestamp();
      if (LogStatus(status)) {
        std::cout << response.message() << std::endl;
      }
    }
  }

  EchoClient m_echo;
  VariableClient m_variable;
  async_grpc::ClientExecutorThread m_executor;
  using THandler = async_grpc::Coroutine(Program::*)();
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