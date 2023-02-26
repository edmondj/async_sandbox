#include <cassert>
#include <async_grpc/server.hpp>
#include "echo_service_impl.hpp"

static async_grpc::ServerUnaryCoroutine UnaryEchoImpl(std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) {
  echo_service::UnaryEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  co_await context->Finish(response);
}

static async_grpc::ServerClientStreamCoroutine ClientStreamEchoImpl(std::unique_ptr<async_grpc::ServerClientStreamContext<echo_service::ClientStreamEchoRequest, echo_service::ClientStreamEchoResponse>> context) {
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
    co_await async_grpc::Alarm(std::chrono::system_clock::now() + std::chrono::milliseconds(context->request.delay_ms()));
    response.set_n(n);
    co_await context->Write(response);
  }
  co_await context->Finish();
}

static async_grpc::ServerBidirectionalStreamCoroutine BidirectionalStreamEchoImpl(std::unique_ptr<async_grpc::ServerBidirectionalStreamContext<echo_service::BidirectionalStreamEchoRequest, echo_service::BidirectionalStreamEchoResponse>> context) {
  echo_service::BidirectionalStreamEchoRequest request;
  echo_service::BidirectionalStreamEchoResponse response;
  async_grpc::Alarm<std::chrono::system_clock::time_point> alarm;
  async_grpc::Coroutine::Subroutine subroutine;
  uint32_t delay_ms = 0;

  while (co_await context->Read(request)) {
    switch (request.command_case()) {
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kMessage: {
      if (subroutine) {
        co_await context->Finish(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not change message while running"));
        co_return;
      }
      response.set_message(std::move(*request.mutable_message()->mutable_message()));
      delay_ms = request.message().delay_ms();
      break;
    }
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kStart: {
      if (subroutine) {
        co_await context->Finish(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not start when already started"));
        co_return;
      }
      if (response.message().empty() || delay_ms == 0) {
        co_await context->Finish(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not start with invalid message set"));
        co_return;
      }
      subroutine = co_await async_grpc::Coroutine::StartSubroutine([&]() -> async_grpc::Coroutine {
        while (true) {
          alarm = async_grpc::Alarm(std::chrono::system_clock::now() + std::chrono::milliseconds(delay_ms));
          if (!co_await alarm) {
            break;
          }
          if (!co_await context->Write(response)) {
            break;
          }
        }
      }());
      break;
    }
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kStop: {
      if (!subroutine) {
        co_await context->Finish(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not stop when not running"));
        co_return;
      }
      alarm.Cancel();
      co_await subroutine;
      break;
    }
    default:
      if (subroutine) {
        alarm.Cancel();
        co_await subroutine;
      }
      co_await context->Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Unhandled command case"));
      co_return;
    }
  }
  if (subroutine) {
    alarm.Cancel();
    co_await subroutine;
  }
  co_await context->Finish();
}

void EchoServiceImpl::StartListening(async_grpc::Server& server)
{
  server.Spawn(server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho), &UnaryEchoImpl));
  server.Spawn(server.StartListeningClientStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho), &ClientStreamEchoImpl));
  server.Spawn(server.StartListeningServerStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ServerStreamEcho), &ServerStreamEchoImpl));
  server.Spawn(server.StartListeningBidirectionalStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, BidirectionalStreamEcho), &BidirectionalStreamEchoImpl));
}
