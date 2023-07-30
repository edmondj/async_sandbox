#include "variable_service_impl.hpp"
#include <functional>

void VariableServiceImpl::StartListening(async_grpc::Server& server) {
  StartListeningUnary(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, Write), std::bind_front(&VariableServiceImpl::WriteImpl, this));
  StartListeningUnary(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, Read), std::bind_front(&VariableServiceImpl::ReadImpl, this));
  StartListeningUnary(server, ASYNC_GRPC_SERVER_LISTEN_FUNC(Service, Del), std::bind_front(&VariableServiceImpl::DelImpl, this));
}

async_grpc::Task<> VariableServiceImpl::WriteImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::WriteRequest, variable_service::WriteResponse>> context) {
  variable_service::WriteResponse response;
  bool was_inserted;
  {
    auto lock = std::unique_lock(m_mutex);
    // The forbidden upsert
    was_inserted = m_storage.insert_or_assign(context->request.key(), context->request.value()).second;
  }
  response.set_was_inserted(was_inserted);

  co_await context->Finish(response);
}

async_grpc::Task<> VariableServiceImpl::ReadImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::ReadRequest, variable_service::ReadResponse>> context)
{
  auto lock = std::shared_lock(m_mutex);
  auto found = m_storage.find(context->request.key());
  if (found == m_storage.end()) {
    lock.unlock();
    co_await context->FinishWithError(grpc::Status(grpc::StatusCode::NOT_FOUND, "Request key was not found in storage"));
  } else {
    variable_service::ReadResponse response;
    response.set_value(found->second);
    lock.unlock();
    co_await context->Finish(response);
  }
}

async_grpc::Task<> VariableServiceImpl::DelImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::DelRequest, variable_service::DelResponse>> context) {
  bool was_deleted;
  {
    auto lock = std::unique_lock(m_mutex);
    was_deleted = m_storage.erase(context->request.key());
  }
  variable_service::DelResponse response;
  response.set_was_deleted(was_deleted);
  co_await context->Finish(response);
}
