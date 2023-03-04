#pragma once

#include <async_grpc/server.hpp>
#include <protos/variable_service.grpc.pb.h>
#include <map>

class VariableServiceImpl : public async_grpc::BaseServiceImpl<variable_service::VariableService> {
public:
  virtual void StartListening(async_grpc::Server& server) override;

private:
  async_grpc::Task<> WriteImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::WriteRequest, variable_service::WriteResponse>> context);
  async_grpc::Task<> ReadImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::ReadRequest, variable_service::ReadResponse>> context);
  async_grpc::Task<> DelImpl(std::unique_ptr<async_grpc::ServerUnaryContext<variable_service::DelRequest, variable_service::DelResponse>> context);

  std::map<std::string, int64_t> m_storage;
};
