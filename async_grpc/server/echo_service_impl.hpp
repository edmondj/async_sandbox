#pragma once

#include <async_grpc/server.hpp>
#include <protos/echo_service.grpc.pb.h>

class EchoServiceImpl : public async_grpc::BaseServiceImpl<echo_service::EchoService> {
public:
  virtual void StartListening(async_grpc::Server& server) override;
};
