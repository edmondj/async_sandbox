#include <cassert>
#include <async_grpc/server.hpp>
#include <async_grpc/iostream.hpp>
#include <utils/Logs.hpp>
#include "echo_service_impl.hpp"

static async_grpc::Task<> UnaryEchoImpl(std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) {
  utils::Log() << "Received UnaryEcho [" << context->request.ShortDebugString() << ']';
  echo_service::UnaryEchoResponse response;
  response.set_message(std::move(*context->request.mutable_message()));
  utils::Log() << "Sending Reply [" << response.ShortDebugString() << ']';
  co_await context->Finish(response);
  utils::Log() << "End of UnaryEcho";
}

static async_grpc::Task<> ClientStreamEchoImpl(std::unique_ptr<async_grpc::ServerClientStreamContext<echo_service::ClientStreamEchoRequest, echo_service::ClientStreamEchoResponse>> context) {
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

static async_grpc::Task<> ServerStreamEchoImpl(std::unique_ptr<async_grpc::ServerServerStreamContext<echo_service::ServerStreamEchoRequest, echo_service::ServerStreamEchoResponse>> context) {
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

static async_grpc::Task<> BidirectionalStreamEchoImpl(std::unique_ptr<async_grpc::ServerBidirectionalStreamContext<echo_service::BidirectionalStreamEchoRequest, echo_service::BidirectionalStreamEchoResponse>> context) {
  echo_service::BidirectionalStreamEchoRequest request;
  echo_service::BidirectionalStreamEchoResponse response;
  grpc::Status status;
  async_grpc::Alarm<std::chrono::system_clock::time_point> alarm;
  async_grpc::Task<> subroutine;
  uint32_t delay_ms = 0;

  auto writeRoutine = [&]() -> async_grpc::Task<> {
    while (true) {
      utils::Log() << "Write subroutine waiting";
      alarm.SetDeadline(std::chrono::system_clock::now() + std::chrono::milliseconds(delay_ms));
      if (!co_await alarm) {
        utils::Log() << "Alarm cancelled";
        break;
      }
      utils::Log() << "Write subroutine writing";
      if (!co_await context->Write(response)) {
        utils::Log() << "Write cancelled";
        break;
      }
    }
    utils::Log() << "Write subroutine stopping";
  };

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
      subroutine = co_await async_lib::StartSubroutine(writeRoutine());
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
  StartListeningUnary(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, UnaryEcho), &UnaryEchoImpl);
  StartListeningClientStream(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ClientStreamEcho), &ClientStreamEchoImpl);
  StartListeningServerStream(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, ServerStreamEcho), &ServerStreamEchoImpl);
  StartListeningBidirectionalStream(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, BidirectionalStreamEcho), &BidirectionalStreamEchoImpl);
}
