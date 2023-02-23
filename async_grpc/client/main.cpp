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

int main() {
  auto channel = grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials());
  EchoClient echo(channel);

  async_grpc::ClientExecutorThread executor;

  for (std::string line; std::getline(std::cin, line);) {
    [&](std::string line) -> async_grpc::Coroutine {
      grpc::ClientContext context;
      echo_service::UnaryEchoRequest request;
      request.set_message(line);
      auto call = echo.UnaryEcho(executor.GetExecutor(), context, request);

      echo_service::UnaryEchoResponse response;
      grpc::Status status;
      if (co_await call.Finish(response, status)) {
        if (status.ok()) {
          std::cout << "[OK] " << response.message() << std::endl;
        } else {
          std::cout << '[' << async_grpc::GrpcStatusCodeString(status.error_code()) << "] " << status.error_message() << std::endl;
        }
      } else {
        std::puts("Cancelled");
      }
    }(line);
  }
}