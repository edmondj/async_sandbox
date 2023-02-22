#include <iostream>
#include <async_grpc/server.hpp>
#include <protos/echo_service.grpc.pb.h>
#include <async_grpc/threads.hpp>

class EchoServiceImpl : public async_grpc::BaseServiceImpl<echo_service::EchoService> {
public:
  virtual void StartListening(async_grpc::Server& server) override {
    server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho),
      [](std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) -> async_grpc::ServerUnaryCoroutine {
        assert(async_grpc::ThisThreadIsGrpc());

        echo_service::UnaryEchoResponse response;
        response.set_message(context->GetRequest().message());
        co_await context->Finish(response);
      }
    );

    server.StartListeningClientStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho),
      [](std::unique_ptr<async_grpc::ServerClientStreamContext<echo_service::ClientStreamEchoRequest, echo_service::ClientStreamEchoResponse>> context) -> async_grpc::ServerClientStreamCoroutine {
        echo_service::ClientStreamEchoResponse response;
        echo_service::ClientStreamEchoRequest request;
        while (co_await context->Read(request)) {
          response.add_messages(std::move(*request.mutable_message()));
        }
        co_await context->Finish(response);
      }
    );
  }
};

int main() {

  EchoServiceImpl echo;

  auto server = [&]() {
    async_grpc::ServerOptions options;
    options.addresses.push_back("[::1]:4213");
    options.services.push_back(std::ref(echo));
    return async_grpc::Server(std::move(options));
  }();

  std::cin.get();
  std::cout << "Shutting down" << std::endl;
}