#include <cassert>
#include <async_grpc/server.hpp>
#include <utils/Logs.hpp>
#include "echo_service_impl.hpp"

static async_grpc::ServerUnaryCoroutine UnaryEchoImpl(std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) {
  utils::Log() << "Received UnaryEcho [" << context->request.ShortDebugString() << ']';
  echo_service::UnaryEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  utils::Log() << "Sending Reply [" << response.ShortDebugString() << ']';
  co_await context->Finish(response);
  utils::Log() << "End of UnaryEcho";
}

static async_grpc::ServerClientStreamCoroutine ClientStreamEchoImpl(std::unique_ptr<async_grpc::ServerClientStreamContext<echo_service::ClientStreamEchoRequest, echo_service::ClientStreamEchoResponse>> context) {
  utils::Log() << "Received ClientStreamEcho";
  echo_service::ClientStreamEchoResponse response;
  echo_service::ClientStreamEchoRequest request;
  utils::Log() << "Start reading";
  while (co_await context->Read(request)) {
    utils::Log() << "Got [" << request.ShortDebugString() << ']';
    response.add_messages(std::move(*request.mutable_message()));
  }
  utils::Log() << "Replying [" << response.ShortDebugString() << ']';
  co_await context->Finish(response);
  utils::Log() << "End of ClientStreamEcho";
}

static async_grpc::ServerServerStreamCoroutine ServerStreamEchoImpl(std::unique_ptr<async_grpc::ServerServerStreamContext<echo_service::ServerStreamEchoRequest, echo_service::ServerStreamEchoResponse>> context) {
  utils::Log() << "Received ServerstreamEcho [" << context->request.ShortDebugString() << ']';
  echo_service::ServerStreamEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  for (uint32_t n = 1; n <= context->request.count(); ++n) {
    utils::Log() << "Started waiting";
    co_await async_grpc::Alarm(std::chrono::system_clock::now() + std::chrono::milliseconds(context->request.delay_ms()));
    response.set_n(n);
    utils::Log() << "Sending response [" << response.ShortDebugString() << ']';
    co_await context->Write(response);
  }
  utils::Log() << "End of ServerStreamEcho";
  co_await context->Finish();
}

static async_grpc::ServerBidirectionalStreamCoroutine BidirectionalStreamEchoImpl(std::unique_ptr<async_grpc::ServerBidirectionalStreamContext<echo_service::BidirectionalStreamEchoRequest, echo_service::BidirectionalStreamEchoResponse>> context) {
  echo_service::BidirectionalStreamEchoRequest request;
  echo_service::BidirectionalStreamEchoResponse response;
  grpc::Status status;
  async_grpc::Alarm<std::chrono::system_clock::time_point> alarm;
  async_grpc::Coroutine subroutine;
  uint32_t delay_ms = 0;

  utils::Log() << "Received BidirectionalStreamEcho";
  while (status.ok() && co_await context->Read(request)) {
    utils::Log() << "Received message [" << request.ShortDebugString() << ']';
    switch (request.command_case()) {
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kMessage: {
      if (subroutine) {
        status = grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not change message while running");
        break;
      }
      utils::Log() << "Setting message";
      response.set_message(std::move(*request.mutable_message()->mutable_message()));
      delay_ms = request.message().delay_ms();
      break;
    }
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kStart: {
      if (subroutine) {
        status = grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not start when already started");
        break;
      }
      if (response.message().empty() || delay_ms == 0) {
        status = grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not start with invalid message set");
        break;
      }
      utils::Log() << "Starting write subroutine";
      subroutine = co_await async_grpc::Coroutine::StartSubroutine([&]() -> async_grpc::Coroutine {
        while (true) {
          utils::Log() << "Write subroutine waiting";
          alarm = async_grpc::Alarm(std::chrono::system_clock::now() + std::chrono::milliseconds(delay_ms));
          if (!co_await alarm) {
            break;
          }
          utils::Log() << "Write subroutine writing";
          if (!co_await context->Write(response)) {
            break;
          }
        }
        utils::Log() << "Write subroutine stopping";
      }());
      break;
    }
    case echo_service::BidirectionalStreamEchoRequest::CommandCase::kStop: {
      if (!subroutine) {
        status = grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Can not stop when not running");
        break;
      }
      utils::Log() << "Stopping subroutine";
      alarm.Cancel();
      co_await std::move(subroutine);
      utils::Log() << "Subroutine stopped";
      break;
    }
    default:
      assert(false);
    }
  }
  if (subroutine) {
    utils::Log() << "Stopping subroutine";
    alarm.Cancel();
    co_await std::move(subroutine);
    utils::Log() << "Subroutine stopped";
  }
  utils::Log() << "Sending status [" << status << ']';
  co_await context->Finish();
  utils::Log() << "End of BidirectionalStreamEcho";
}

void EchoServiceImpl::StartListening(async_grpc::Server& server)
{
  server.Spawn(server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho), &UnaryEchoImpl));
  server.Spawn(server.StartListeningClientStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho), &ClientStreamEchoImpl));
  server.Spawn(server.StartListeningServerStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ServerStreamEcho), &ServerStreamEchoImpl));
  server.Spawn(server.StartListeningBidirectionalStream(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, BidirectionalStreamEcho), &BidirectionalStreamEchoImpl));
}
