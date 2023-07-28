#include <iostream>
#include <async_grpc/client.hpp>
#include <async_grpc/iostream.hpp>
#include <protos/echo_service.grpc.pb.h>
#include <protos/variable_service.grpc.pb.h>
#include <utils/logs.hpp>
#include <ranges>

using EchoClient = async_grpc::Client<echo_service::EchoService>;
using VariableClient = async_grpc::Client<variable_service::VariableService>;

class Program {
public:
  Program(const std::shared_ptr<grpc::Channel>& channel)
    : m_echo(channel)
  {
#define ADD_HANDLER(command) m_handlers.insert_or_assign(#command, &Program::command)
    ADD_HANDLER(Noop);
    ADD_HANDLER(NestedEcho);
    ADD_HANDLER(UnaryEcho);
    ADD_HANDLER(ClientStreamEcho);
    ADD_HANDLER(ServerStreamEcho);
    ADD_HANDLER(BidirectionalStreamEcho);
#undef ADD_HANDLER
  }

  auto AvailableCommands() const {
    return std::views::keys(m_handlers);
  }

  void Process(std::string_view line) {
    for (const auto& [name, handler] : m_handlers) {
      if (line.starts_with(name)) {
        async_lib::Spawn(m_executor.GetExecutor(), (this->*handler)());
        return;
      }
    }
    utils::Log() << "Unknown command";
  }

private:
  async_grpc::Task<> Noop() {
    utils::Log() << "Start";
    utils::Log() << "End " << co_await[]() -> async_grpc::Task<std::string> {
      utils::Log() << "Start sub1";
      utils::Log() << "End " << co_await[]() -> async_grpc::Task<std::string> {
        utils::Log() << "Sub2";
        co_return "sub2";
      }();
      co_return "sub1";
    }();
  }

  async_grpc::Task<> NestedEcho() {
    utils::Log() << "Start";
    co_await[this]() -> async_grpc::Task<> {
      utils::Log() << "Start sub1";
      co_await[this]() -> async_grpc::Task<> {
        utils::Log() << "Start sub2";
        echo_service::UnaryEchoRequest request;
        request.set_message("hello");
        echo_service::UnaryEchoResponse response;
        grpc::Status status;
        grpc::ClientContext context;
        utils::Log() << "Sending [" << request.ShortDebugString() << ']';
        if (co_await m_echo.CallUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, UnaryEcho), context, request, response, status)) {
          auto out = utils::Log() << "Received [" << status << ']';
          if (status.ok()) {
            out << " [" << response.ShortDebugString() << ']';
          }
        }
        else {
          utils::Log() << "Cancelled";
        }
        utils::Log() << "End sub2";
      }();
      utils::Log() << "End sub1";
    }();
    utils::Log() << "End";
  }

  async_grpc::Task<> UnaryEcho() {
    utils::Log() << "Start";
    echo_service::UnaryEchoRequest request;
    request.set_message("hello");

    echo_service::UnaryEchoResponse response;
    grpc::Status status;
    std::unique_ptr<grpc::ClientContext> context;

    auto retryOptions = async_grpc::RetryOptions(
      [policy = async_grpc::DefaultRetryPolicy()](const grpc::Status& status) mutable {
        auto sent = policy(status);
        utils::Log() << "Got [" << status << (sent ? "] will " : "] will not ") << "retry";
        return sent;
      },
      async_grpc::DefaultClientContextProvider{}
    );

    utils::Log() << "Sending [" << request.ShortDebugString() << ']';
    if (co_await m_echo.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, UnaryEcho), context, request, response, status, retryOptions)) {
      auto out = utils::Log() << "Received [" << status << ']';
      if (status.ok()) {
        out << " [" << response.ShortDebugString() << ']';
      }
    }
    else {
      utils::Log() << "Cancelled";
    }
    utils::Log() << "End";
  }

  async_grpc::Task<> ClientStreamEcho() {
    utils::Log() << "Start";
    echo_service::ClientStreamEchoResponse response;
    grpc::ClientContext context;
    auto call = co_await m_echo.CallClientStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, ClientStreamEcho), context, response);
    if (!call) {
      utils::Log() << "Cancelled";
      co_return;
    }
    echo_service::ClientStreamEchoRequest request;
    for (auto&& msg : { "Hello!", "World!", "Bye!" }) {
      request.set_message(msg);
      utils::Log() << "Sending [" << request.ShortDebugString() << "]";
      grpc::WriteOptions opt;
      opt.set_buffer_hint();
      if (!co_await call->Write(request, opt)) {
        utils::Log() << "Cancelled";
        co_return;
      }
    }
    utils::Log() << "Closing write stream";
    if (!co_await call->WritesDone()) {
      utils::Log() << "Cancelled";
      co_return;
    }
    grpc::Status status;
    utils::Log() << "Awaiting response";
    if (!co_await call->Finish(status)) {
      utils::Log() << "Cancelled";
    } else {
      auto out = utils::Log() << "Received [" << status << ']';
      if (status.ok()) {
        out << '[' << response.ShortDebugString() << ']';
      }
    }
    utils::Log() << "End";
  }

  async_grpc::Task<> ServerStreamEcho() {
    utils::Log() << "Start";
    echo_service::ServerStreamEchoRequest request;
    request.set_message("hola");
    request.set_count(3);
    request.set_delay_ms(1000);
    grpc::ClientContext context;
    utils::Log() << "Sending [" << request.ShortDebugString() << ']';
    auto call = co_await m_echo.CallServerStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, ServerStreamEcho), context, request);
    if (!call) {
      utils::Log() << "Cancelled";
      co_return;
    }
    echo_service::ServerStreamEchoResponse response;
    utils::Log() << "Start Read";
    while (co_await call->Read(response)) {
      utils::Log() << "Received [" << response.ShortDebugString() << ']';
    }
    utils::Log() << "Read done, getting status...";
    grpc::Status status;
    if (!co_await call->Finish(status)) {
      utils::Log() << "Cancelled";
    }
    else {
      utils::Log() << "Received [" << status << ']';
    }
    utils::Log() << "End";
  }

  async_grpc::Task<> BidirectionalStreamEcho() {
    utils::Log() << "Start";
    grpc::ClientContext context;
    auto call = co_await m_echo.CallBidirectionalStream(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, BidirectionalStreamEcho), context);
    if (!call) {
      utils::Log() << "Cancelled";
      co_return;
    }
    utils::Log() << "Starting read subroutine...";
    auto startRead = [&]() -> async_grpc::Task<> {
      utils::Log() << "Read subroutine started";
      echo_service::BidirectionalStreamEchoResponse response;
      while (co_await call->Read(response)) {
        utils::Log() << "Received [" << response.ShortDebugString() << ']';
      }
      utils::Log() << "Read subroutine done";
    };
    async_grpc::Task<> readTask = co_await async_lib::StartSubroutine(startRead());
    echo_service::BidirectionalStreamEchoRequest request;
    auto* message = request.mutable_message();
    message->set_message("guten tag");
    message->set_delay_ms(1000);
    utils::Log() << "Sending [" << request.ShortDebugString() << ']';
    if (!co_await call->Write(request)) {
      utils::Log() << "Cancelled";
      co_return;
    }
    request.mutable_start();
    utils::Log() << "Sending [" << request.ShortDebugString() << ']';
    if (!co_await call->Write(request)) {
      utils::Log() << "Cancelled";
      co_return;
    }
    utils::Log() << "Waiting";
    if (!co_await async_grpc::Alarm(std::chrono::system_clock::now() + std::chrono::milliseconds(3500))) {
      utils::Log() << "Cancelled";
      co_return;
    }
    request.mutable_stop();
    utils::Log() << "Sending [" << request.ShortDebugString() << ']';
    if (!co_await call->Write(request)) {
      utils::Log() << "Cancelled";
      co_return;
    }
    utils::Log() << "Closing write stream";
    if (!co_await call->WritesDone()) {
      utils::Log() << "Cancelled";
      co_return;
    }
    utils::Log() << "Stopping subroutine";
    context.TryCancel(); // Cancel pending read
    co_await std::move(readTask);
    utils::Log() << "End";
  }

  EchoClient m_echo;
  async_grpc::ClientExecutorThreads m_executor{ 2 };
  using THandler = async_grpc::Task<>(Program::*)();
  std::map<std::string, THandler> m_handlers;
};

int main() {
  Program program(grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials()));

  utils::Log() << "Client ready, type `help` for a list of commands or `quit` to quit";
  for (std::string line; std::getline(std::cin, line);) {
    if (line == "quit") {
      break;
    }
    else if (line == "help") {
      const char* delim = "";
      auto log = utils::Log() << "Available commands: ";
      for (std::string_view cmd : program.AvailableCommands()) {
        log << delim << cmd;
        delim = ", ";
      }
    }
    else {
      program.Process(line);
    }
  }
}