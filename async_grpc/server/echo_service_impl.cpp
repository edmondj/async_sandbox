#include <cassert>
#include <async_grpc/threads.hpp>
#include "echo_service_impl.hpp"

static async_grpc::ServerUnaryCoroutine UnaryEchoImpl(std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) {
  assert(async_grpc::ThisThreadIsGrpc());

  echo_service::UnaryEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  co_await context->Finish(response);
}

static async_grpc::ServerClientStreamCoroutine ClientStreamEchoImpl(std::unique_ptr<async_grpc::ServerClientStreamContext<echo_service::ClientStreamEchoRequest, echo_service::ClientStreamEchoResponse>> context) {
  assert(async_grpc::ThisThreadIsGrpc());

  echo_service::ClientStreamEchoResponse response;
  echo_service::ClientStreamEchoRequest request;
  while (co_await context->Read(request)) {
    response.add_messages(std::move(*request.mutable_message()));
  }
  co_await context->Finish(response);
}

void EchoServiceImpl::StartListening(async_grpc::Server& server)
{
  server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho), &UnaryEchoImpl);
  server.StartListeningClientStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho), &ClientStreamEchoImpl);
}
