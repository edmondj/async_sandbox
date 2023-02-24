#include <iostream>
#include <async_grpc/client.hpp>
#include <protos/echo_service.grpc.pb.h>
#include <protos/variable_service.grpc.pb.h>

using EchoClient = async_grpc::Client<echo_service::EchoService>;
using VariableClient = async_grpc::Client<variable_service::VariableService>;

std::ostream& operator<<(std::ostream& out, const grpc::Status& status) {
  out << '[' << async_grpc::StatusCodeString(status.error_code());
  if (!status.ok()) {
    out << ':' << status.error_message();
  }
  return out << ']';
}

class Program {
public:
  Program(const std::shared_ptr<grpc::Channel>& channel)
    : m_echo(channel)
    , m_variable(channel)
  {
#define ADD_HANDLER(command) m_handlers.insert_or_assign(#command, &Program::command)
    ADD_HANDLER(UnaryEcho);
    ADD_HANDLER(ClientStreamEcho);
    ADD_HANDLER(ServerStreamEcho);
    ADD_HANDLER(BidirectionalStreamEcho);
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
  std::ostream& Log() {
    return std::cout << '<' << std::chrono::system_clock::now() << "> ";
  }

  async_grpc::Coroutine UnaryEcho() {
    Log() << "start" << std::endl;
    echo_service::UnaryEchoRequest request;
    request.set_message("hello");

    echo_service::UnaryEchoResponse response;
    grpc::Status status;
    std::unique_ptr<grpc::ClientContext> context;
    if (co_await m_echo.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(EchoClient, UnaryEcho), m_executor.GetExecutor(), context, request, response, status)) {
      std::ostream& out = Log() << status;
      if (status.ok()) {
        out << ' ' << response.message();
      }
      out << std::endl;
    }
    else {
      Log() << "cancelled" << std::endl;
    }
    Log() << "end" << std::endl;
  }

  async_grpc::Coroutine ClientStreamEcho() {
    Log() << "start" << std::endl;
    echo_service::ClientStreamEchoResponse response;
    grpc::ClientContext context;
    auto call = co_await m_echo.CallClientStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(EchoClient, ClientStreamEcho), m_executor.GetExecutor(), context, response);
    if (!call) {
      Log() << "start cancelled" << std::endl;
      co_return;
    }
    echo_service::ClientStreamEchoRequest request;
    for (auto&& msg : { "Hello!", "World!", "Bye!" }) {
      Log() << "writing " << msg << std::endl;
      request.set_message(msg);
      if (!co_await call->Write(request)) {
        Log() << "write cancelled" << std::endl;
        co_return;
      }
    }
    Log() << "done" << std::endl;
    if (!co_await call->WritesDone()) {
      Log() << "done cancelled" << std::endl;
      co_return;
    }
    grpc::Status status;
    if (!co_await call->Finish(status)) {
      Log() << "finish cancelled" << std::endl;
    } else {
      auto& out = Log() << status;
      if (status.ok()) {
        for (const std::string& msg : response.messages()) {
          out << ' ' << msg;
        }
        out << std::endl;
      }
    }
    Log() << "end" << std::endl;
  }

  async_grpc::Coroutine ServerStreamEcho() {
    Log() << "start" << std::endl;
    echo_service::ServerStreamEchoRequest request;
    request.set_message("hola");
    request.set_count(3);
    request.set_delay_ms(1000);
    grpc::ClientContext context;
    auto call = co_await m_echo.CallServerStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(EchoClient, ServerStreamEcho), m_executor.GetExecutor(), context, request);
    if (!call) {
      Log() << "start cancelled" << std::endl;
      co_return;
    }
    echo_service::ServerStreamEchoResponse response;
    Log() << "start read" << std::endl;
    while (co_await call->Read(response)) {
      Log() << response.message() << ' ' << response.n() << std::endl;
    }
    Log() << "done read" << std::endl;
    grpc::Status status;
    if (!co_await call->Finish(status)) {
      Log() << "finish cancelled" << std::endl;
    }
    else {
      Log() << status << std::endl;
    }
    Log() << "end" << std::endl;
  }

  async_grpc::Coroutine BidirectionalStreamEcho() {
    Log() << "start" << std::endl;
    grpc::ClientContext context;
    auto call = co_await m_echo.CallBidirectionalStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(EchoClient, BidirectionalStreamEcho), m_executor.GetExecutor(), context);
    if (!call) {
      Log() << "start cancelled" << std::endl;
      co_return;
    }
    auto readCoroutine = [&]() -> async_grpc::Coroutine {
      echo_service::BidirectionalStreamEchoResponse response;
      while (co_await call->Read(response)) {
        Log() << response.message() << std::endl;
      }
      Log() << "read done" << std::endl;
    }();
    echo_service::BidirectionalStreamEchoRequest request;
    auto* message = request.mutable_message();
    message->set_message("guten tag");
    message->set_delay_ms(1000);
    Log() << "write message" << std::endl;
    if (!co_await call->Write(request)) {
      Log() << "write cancelled" << std::endl;
      co_return;
    }
    request.mutable_start();
    Log() << "write start" << std::endl;
    if (!co_await call->Write(request)) {
      Log() << "write cancelled" << std::endl;
      co_return;
    }
    Log() << "wait" << std::endl;
    if (!co_await async_grpc::Alarm(m_executor.GetExecutor(), std::chrono::system_clock::now() + std::chrono::milliseconds(3500))) {
      Log() << "wait cancelled" << std::endl;
      co_return;
    }
    request.mutable_stop();
    Log() << "write stop" << std::endl;
    if (!co_await call->Write(request)) {
      Log() << "write cancelled" << std::endl;
      co_return;
    }
    if (!co_await call->WritesDone()) {
      Log() << "writes done cancelled" << std::endl;
      co_return;
    }
    Log() << "wait read" << std::endl;
    co_await readCoroutine;
    Log() << "end" << std::endl;
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