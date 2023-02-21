#pragma once

#include <grpcpp/grpcpp.h>

namespace async_grpc {

  struct IServiceImpl {
    virtual ~IServiceImpl() = default;

    virtual grpc::Service* GetService() const = 0;
  };

}