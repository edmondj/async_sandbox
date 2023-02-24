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

  class ClientCall {
  public:
    ClientCall(const ClientExecutor& executor) noexcept;

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
  using TPrepareUnaryFunc = std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> (TStub::*)(grpc::ClientContext*, const TRequest&, grpc::CompletionQueue*);
 
  template<typename TResponse>
  class [[nodiscard]] ClientUnaryCall : public ClientCall {
  public:

    ClientUnaryCall(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> reader)
      : ClientCall(executor)
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

  template<typename TResponse>
  class [[nodiscard]] ClientUnaryAwaitable {
  public:
    ClientUnaryAwaitable(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> reader)
      : m_executor(executor)
      , m_reader(std::move(reader))
    {}

    bool await_ready() { return true; }
    void await_suspend(std::coroutine_handle<Coroutine::promise_type>) {}
    ClientUnaryCall<TResponse> await_resume() {
      m_reader->StartCall();
      return ClientUnaryCall<TResponse>(m_executor, std::move(m_reader));
    }

  private:
    const ClientExecutor& m_executor;
    std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> m_reader;
  };

  // ~Unary

  // Client Stream

  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareClientStreamFunc = std::unique_ptr<grpc::ClientAsyncWriter<TRequest>>(TStub::*)(grpc::ClientContext*, TResponse*, grpc::CompletionQueue*);

  template<typename TRequest>
  class [[nodiscard]] ClientClientStreamCall : public ClientCall {
  public:
    ClientClientStreamCall(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> writer)
      : ClientCall(executor)
      , m_writer(std::move(writer))
    {}

    auto Write(const TRequest& msg) {
      return Awaitable([&](void* tag) {
        m_writer->Write(msg, tag);
      });
    }

    auto Write(const TRequest& msg, grpc::WriteOptions options) {
      return Awaitable([&](void* tag) {
        m_writer->Write(msg, options, tag);
      });
    }

    auto WritesDone() {
      return Awaitable([&](void* tag) {
        m_writer->WritesDone(tag);
      });
    }

    auto Finish(grpc::Status& status) {
      return Awaitable([&](void* tag) {
        m_writer->Finish(&status, tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> m_writer;
  };

  template<typename TRequest>
  class [[nodiscard]] ClientClientStreamAwaitable {
  public:
    ClientClientStreamAwaitable(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> writer)
      : m_executor(executor)
      , m_writer(std::move(writer))
    {}

    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_promise = &h.promise();
      m_writer->StartCall(h.address());
    }
    std::optional<ClientClientStreamCall<TRequest>> await_resume() {
      if (!m_promise->cancelled) {
        return std::nullopt;
      }
      return ClientClientStreamCall<TRequest>(m_executor, std::move(m_writer));
    }

  private:
    Coroutine::promise_type* m_promise = nullptr;
    const ClientExecutor& m_executor;
    std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> m_writer;
  };

  // ~Client Stream

  // Server Stream

  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareServerStreamFunc = std::unique_ptr<grpc::ClientAsyncReader<TResponse>>(TStub::*)(grpc::ClientContext*, const TRequest&, grpc::CompletionQueue*);

  template<typename TResponse>
  class [[nodiscard]] ClientServerStreamCall : public ClientCall {
  public:

    ClientServerStreamCall(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncReader<TResponse>> reader)
      : ClientCall(executor)
      , m_reader(std::move(reader))
    {}

    auto Read(TResponse& response) {
      return Awaitable([&](void* tag) {
        m_reader->Read(&response, tag);
      });
    }

    auto Finish(grpc::Status& status) {
      return Awaitable([&](void* tag) {
        m_reader->Finish(&status, tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncReader<TResponse>> m_reader;
  };

  template<typename TResponse>
  class [[nodiscard]] ClientServerStreamAwaitable {
  public:
    ClientServerStreamAwaitable(const ClientExecutor& executor, std::unique_ptr<grpc::ClientAsyncReader<TResponse>> reader)
      : m_executor(executor)
      , m_reader(std::move(reader))
    {}

    bool await_ready() { return false; }
    
    void await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_promise = &h.promise();
      m_reader->StartCall(h.address());
    }
    
    std::optional<ClientServerStreamCall<TResponse>> await_resume() {
      if (!m_promise->cancelled) {
        return std::nullopt;
      }
      return ClientServerStreamCall<TResponse>(m_executor, std::move(m_reader));
    }

  private:
    const ClientExecutor& m_executor;
    Coroutine::promise_type* m_promise = nullptr;
    std::unique_ptr<grpc::ClientAsyncReader<TResponse>> m_reader;
  };

  // ~Server Stream

#define ASYNC_GRPC_CLIENT_PREPARE_FUNC(client, rpc) &client::Service::Stub::PrepareAsync ## rpc

  template<ServiceConcept TService>
  class Client {
  public:
    using Service = TService;

    Client(ChannelProvider channelProvider)
      : m_channelProvider(std::move(channelProvider))
    {}

    template<typename TRequest, typename TResponse>
    auto CallUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request) {
      return ClientUnaryAwaitable(executor, (MakeStub().*func)(&context, request, executor.GetCq()));
    }

    template<typename TRequest, typename TResponse>
    Coroutine CallUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request, TResponse& response, grpc::Status& status) {
      auto call = co_await CallUnary(func, executor, context, request);
      co_await call.Finish(response, status);
    }

    template<typename TRequest, typename TResponse, RetryOptionsConcept TRetryOptions = DefaultRetryOptions>
    Coroutine AutoRetryUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, std::unique_ptr<grpc::ClientContext>& context, const TRequest& request, TResponse& response, grpc::Status& status, TRetryOptions retryOptions = {}) {
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

    template<typename TRequest, typename TResponse>
    auto CallClientStream(TPrepareClientStreamFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, TResponse& response) {
      return ClientClientStreamAwaitable(executor, (MakeStub().*func)(&context, &response, executor.GetCq()));
    }

    template<typename TRequest, typename TResponse>
    auto CallServerStream(TPrepareServerStreamFunc<typename Service::Stub, TRequest, TResponse> func, const ClientExecutor& executor, grpc::ClientContext& context, const TRequest& request) {
      return ClientServerStreamAwaitable(executor, (MakeStub().*func)(&context, request, executor.GetCq()));
    }

  protected:
    ChannelProvider m_channelProvider;

    typename Service::Stub MakeStub() {
      return Service::Stub(m_channelProvider.SelectNextChannel());
    }
  };
}
