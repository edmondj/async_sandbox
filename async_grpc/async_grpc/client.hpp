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

  template<typename T>
  concept RetryPolicyConcept = requires(T t, const grpc::Status& status) {
    { static_cast<bool>(t(status)) };
    { *t(status) } -> TimePointConcept;
  };

  class DefaultRetryPolicy {
  public:
    std::optional<std::chrono::system_clock::time_point> operator()(const grpc::Status& status);

  private:
    size_t m_retryCount = 0;
  };
  static_assert(RetryPolicyConcept<DefaultRetryPolicy>);

  template<typename T>
  concept ClientContextProviderConcept = requires(T t) {
    { t() } -> std::same_as<std::unique_ptr<grpc::ClientContext>>;
  };

  struct DefaultClientContextProvider {
    std::unique_ptr<grpc::ClientContext> operator()();
  };

  template<typename T>
  concept RetryOptionsConcept = requires(T t) {
    { t.retryPolicy } -> RetryPolicyConcept;
    { t.contextProvider } -> ClientContextProviderConcept;
  };

  template<RetryPolicyConcept TRetry, ClientContextProviderConcept TContext>
  struct RetryOptions {
    TRetry retryPolicy;
    TContext contextProvider;
  };
  using DefaultRetryOptions = RetryOptions<DefaultRetryPolicy, DefaultClientContextProvider>;
  static_assert(RetryOptionsConcept<DefaultRetryOptions>);

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

  template<typename TStub, typename TRequest, typename TResponse>
  Coroutine ClientStartUnary(TStub&& stub, TStartUnaryFunc<TStub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request, TResponse& response, grpc::Status& status) {
    ClientUnaryContext<TResponse> call(executor, (stub.*func)(&context, request, executor.GetCq()));
    co_await call.Finish(response, status);
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

#define ASYNC_GRPC_CLIENT_START_FUNC(client, rpc) &client::Service::Stub::Async ## rpc

  template<ServiceConcept TService>
  class Client {
  public:
    using Service = TService;

    Client(ChannelProvider channelProvider)
      : m_channelProvider(std::move(channelProvider))
    {}

    template<typename TRequest, typename TResponse>
    ClientUnaryContext<TResponse> StartUnary(TStartUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request) {
      return ClientUnaryContext(executor, (MakeStub().*func)(&context, request, executor.GetCq()));
    }

    template<typename TRequest, typename TResponse>
    Coroutine CallUnary(TStartUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request, TResponse& response, grpc::Status& status) {
      co_await StartUnary(func, executor, context, request).Finish(response, status);
    }

    template<typename TRequest, typename TResponse, RetryOptionsConcept TRetryOptions = DefaultRetryOptions>
    Coroutine AutoRetryUnary(TStartUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, std::unique_ptr<grpc::ClientContext>& context, const TRequest& request, TResponse& response, grpc::Status& status, TRetryOptions retryOptions = {}) {
      while (true) {
        context = retryOptions.contextProvider();
        if (!co_await CallUnary(func, executor, *context, request, response, status)) {
          break;
        }
        if (status.ok()) {
          break;
        }
        if (auto shouldRetry = retryOptions.retryPolicy(status); shouldRetry) {
          co_await Alarm(executor, *shouldRetry);
        } else {
          break;
        }
      }
    }

  protected:
    ChannelProvider m_channelProvider;

    typename Service::Stub MakeStub() {
      return Service::Stub(m_channelProvider.SelectNextChannel());
    }
  };
}
