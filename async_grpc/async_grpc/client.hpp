#pragma once

#include "async_grpc.hpp"

namespace async_grpc {

  class ClientExecutor {
  public:
    ClientExecutor() noexcept;

    grpc::CompletionQueue* GetCq() const;

  private:
    std::unique_ptr<grpc::CompletionQueue> m_cq;
  };

  using ClientExecutorThread = ExecutorThread<ClientExecutor>;

  class ClientContext {
  public:
    ClientContext(const ClientExecutor& executor) noexcept;

    const ClientExecutor& executor;
  };

  class ChannelProvider {
  public:
    ChannelProvider(std::shared_ptr<grpc::Channel> channel) noexcept;
    ChannelProvider(std::vector<std::shared_ptr<grpc::Channel>> channels) noexcept;
    ChannelProvider(ChannelProvider&& other) noexcept;

    const std::shared_ptr<grpc::Channel>& SelectNextChannel();

  private:
    const std::vector<std::shared_ptr<grpc::Channel>> m_channels;
    std::atomic<size_t> m_nextChannel{ 0 };
  };

#define ASYNC_GRPC_CLIENT_START_FUNC(service, rpc) &service::Stub::Async ## rpc

  // Unary
  
  template<typename TStub, typename TRequest, typename TResponse>
  using TStartUnaryFunc = std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> (TStub::*)(grpc::ClientContext*, const TRequest&, grpc::CompletionQueue*);
 
  template<typename TResponse>
  class ClientUnaryContext : public ClientContext {
  public:

    ClientUnaryContext(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> reader)
      : ClientContext(executor)
      , m_reader(std::move(reader))
    {}

    auto Finish(TResponse& response, grpc::Status& status) {
      return Awaitable([&](void* tag) {
        m_reader->Finish(&response, &status, tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> m_reader;
  };

  template<typename TStub, typename TRequest, typename TResponse>
  auto ClientStartUnary(TStub&& stub, TStartUnaryFunc<TStub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request) {
    return ClientUnaryContext<TResponse>(executor, (stub.*func)(&context, request, executor.GetCq()));
  }

  template<typename TStub>
  struct TStartUnaryFuncTrait;

  template<typename TStub, typename TRequest, typename TResponse>
  struct TStartUnaryFuncTrait<TStartUnaryFunc<TStub, TRequest, TResponse>> {
    using Stub = TStub;
    using Request = TRequest;
    using Response = TResponse;
  };

  // ~Unary

  template<ServiceConcept TService>
  class ClientBase {
  public:
    using Service = TService;

    ClientBase(ChannelProvider channelProvider)
      : m_channelProvider(std::move(channelProvider))
    {}

    // These must be put invoked inside the definition of a class that inherits ClientBase<service>
#define ASYNC_GRPC_CLIENT_UNARY(rpc) \
    inline auto rpc(const ::async_grpc::ClientExecutor& executor, ::grpc::ClientContext& context, const typename ::async_grpc::TStartUnaryFuncTrait<decltype(ASYNC_GRPC_CLIENT_START_FUNC(Service, rpc))>::Request& request) {\
      return ClientStartUnary(typename Service::Stub(m_channelProvider.SelectNextChannel()), ASYNC_GRPC_CLIENT_START_FUNC(Service, rpc), executor, context, request); \
    }

  protected:
    ChannelProvider m_channelProvider;
  };
}
