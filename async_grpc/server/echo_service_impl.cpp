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

static async_grpc::ServerServerStreamCoroutine ServerStreamEchoImpl(std::unique_ptr<async_grpc::ServerServerStreamContext<echo_service::ServerStreamEchoRequest, echo_service::ServerStreamEchoResponse>> context) {
  echo_service::ServerStreamEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  for (uint32_t n = 1; n <= context->request.count(); ++n) {
    co_await async_grpc::Alarm(context->executor, std::chrono::system_clock::now() + std::chrono::milliseconds(context->request.delay_ms()));
    response.set_n(n);
    co_await context->Write(response);
  }
  co_await context->Finish();
}

void EchoServiceImpl::StartListening(async_grpc::Server& server)
{
  server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho), &UnaryEchoImpl);
  server.StartListeningClientStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho), &ClientStreamEchoImpl);
  server.StartListeningServerStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ServerStreamEcho), &ServerStreamEchoImpl);
}
