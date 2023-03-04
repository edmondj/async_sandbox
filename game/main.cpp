#include "async_game.hpp"
#include <async_grpc/client.hpp>
#include <async_grpc/iostream.hpp>
#include <utils/logs.hpp>
#include <utils/expected.hpp>
#include <protos/echo_service.grpc.pb.h>
#include "character_service_grpc.hpp"

std::string GrpcStatusToString(const grpc::Status& status) {
  std::ostringstream oss;
  oss << status;
  return std::move(oss).str();
}

int main() {
  async_game::Executor executor;
  async_grpc::ClientExecutorThread grpcExecutor;
  async_grpc::Client<echo_service::EchoService> echo(grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials()));
  size_t done = 0;

  auto echoTest = [&]() -> async_game::Task<> {
    utils::Log() << "Beginning";
    //auto sendEcho = [&grpcExecutor, &echo](std::string_view message) -> async_game::Task<utils::expected<std::string, std::string>> {
    //  utils::Log() << "Inside task";
    //  echo_service::UnaryEchoRequest request;
    //  request.set_message(std::string(message));
    //  grpc::ClientContext context;
    //  echo_service::UnaryEchoResponse response;
    //  grpc::Status status;
    //  utils::Log() << "Starting Echo";
    //  co_await async_game::GrpcCoroutine(grpcExecutor.GetExecutor(), echo.CallUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(echo_service::EchoService, UnaryEcho), context, request, response, status));
    //  utils::Log() << "Echo finished [" << status << "] [" << response.ShortDebugString() << ']';
    //  if (status.ok()) {
    //    co_return std::move(*response.mutable_message());
    //  }
    //  co_return utils::unexpected(GrpcStatusToString(status));
    //};

    //utils::Log() << "Calling task";
    //utils::expected<std::string, std::string> res = co_await sendEcho("Hello!");
    //utils::Log() << "Task done";
    //if (res) {
    //  utils::Log() << "Success! Got: [" << *res << ']';
    //}
    //else {
    //  utils::Log() << "Failed with error [" << res.error() << ']';
    //}


    //utils::Log() << "Sending Bye!";
    //res = co_await sendEcho("Bye!");
    //utils::Log() << "Bye done";
    //if (res) {
    //  utils::Log() << "Success! Got: [" << *res << ']';
    //}
    //else {
    //  utils::Log() << "Failed with error [" << res.error() << ']';
    //}

    utils::Log() << "Task done";
    ++done;
    co_return;
  };

  utils::Log() << "Queuing work";
  async_lib::Spawn(executor, echoTest());

  utils::Log() << "Update loop";
  while (done < 1) {
    executor.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  utils::Log() << "Game over";
}