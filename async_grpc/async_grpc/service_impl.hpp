#pragma once

#include <grpcpp/grpcpp.h>
#include "async_grpc.hpp"

namespace async_grpc {

  struct Server;

  struct IServiceImpl {
    virtual ~IServiceImpl() = default;

    virtual grpc::Service* GetGrpcService() = 0;
    virtual void StartListening(Server& server) = 0;
  };

  template<GrpcServiceConcept TService>
  class BaseServiceImpl : public IServiceImpl {
  public:
    using Service = TService;

    virtual grpc::Service* GetGrpcService() final {
      return &m_service;
    }

    Service::AsyncService& GetConcreteGrpcService() {
      return m_service;
    }

  private:
    TService::AsyncService m_service;
  };

  template<typename T>
  concept ServiceImplConcept = std::derived_from<T, IServiceImpl> && GrpcServiceConcept<typename T::Service> && requires(T t) {
    { t.GetConcreteGrpcService() } -> std::same_as<typename T::Service::AsyncService&>;
  };

  template<typename T, typename TService>
  concept AsyncServiceBase = ServiceImplConcept<TService> && std::derived_from<typename TService::Service::AsyncService, T>;
}