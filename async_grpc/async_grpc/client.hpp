#pragma once

#include "async_grpc.hpp"
#include <optional>

namespace async_grpc {

  class ClientExecutor {
  public:
    ClientExecutor() noexcept;

    grpc::CompletionQueue* GetCq() const;

  private:
    std::unique_ptr<grpc::CompletionQueue> m_cq;
  };

  using ClientExecutorThread = ExecutorThread<ClientExecutor>;

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
    { [&t, &status]() -> Coroutine { co_await *t(status); } };
  };

  class DefaultRetryPolicy {
  public:
    std::optional<Alarm<std::chrono::system_clock::time_point>> operator()(const grpc::Status& status);

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
    RetryOptions() = default;

    RetryOptions(TRetry retryPolicy, TContext contextProvider)
      : retryPolicy(std::move(retryPolicy))
      , contextProvider(std::move(contextProvider))
    {}

    TRetry retryPolicy;
    TContext contextProvider;
  };
  template<RetryPolicyConcept TRetry, ClientContextProviderConcept TContext>
  RetryOptions(TRetry, TContext) -> RetryOptions<TRetry, TContext>;

  using DefaultRetryOptions = RetryOptions<DefaultRetryPolicy, DefaultClientContextProvider>;
  static_assert(RetryOptionsConcept<DefaultRetryOptions>);

  // Unary
  
  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareUnaryFunc = std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> (TStub::*)(grpc::ClientContext*, const TRequest&, grpc::CompletionQueue*);
 
  template<typename TResponse>
  class [[nodiscard]] ClientUnaryCall {
  public:

    ClientUnaryCall(std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> reader)
      : m_reader(std::move(reader))
    {}

    auto Finish(TResponse& response, grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader->Finish(&response, &status, data.tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> m_reader;
  };

  template<typename TStub, typename TRequest, typename TResponse>
  class [[nodiscard]] ClientUnaryAwaitable {
  public:
    ClientUnaryAwaitable(TStub stub, TPrepareUnaryFunc<TStub, TRequest, TResponse> func, grpc::ClientContext& context, const TRequest& request)
      : m_stub(std::move(stub))
      , m_func(func)
      , m_context(context)
      , m_request(request)
    {}

    bool await_ready() { return false; }
    bool await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_reader = (m_stub.*m_func)(&m_context, m_request, AwaitData::FromHandle(h).cq);
      return false;
    }
    ClientUnaryCall<TResponse> await_resume() {
      return ClientUnaryCall<TResponse>(std::move(m_reader));
    }

  private:
    TStub m_stub;
    TPrepareUnaryFunc<TStub, TRequest, TResponse> m_func;
    grpc::ClientContext& m_context;
    const TRequest& m_request;
    std::unique_ptr<grpc::ClientAsyncResponseReader<TResponse>> m_reader;
  };

   //~Unary

  // Client Stream

  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareClientStreamFunc = std::unique_ptr<grpc::ClientAsyncWriter<TRequest>>(TStub::*)(grpc::ClientContext*, TResponse*, grpc::CompletionQueue*, void*);

  template<typename TRequest>
  class [[nodiscard]] ClientClientStreamCall  {
  public:
    ClientClientStreamCall(std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> writer)
      : m_writer(std::move(writer))
    {}

    auto Write(const TRequest& msg) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer->Write(msg, data.tag);
      });
    }

    auto Write(const TRequest& msg, grpc::WriteOptions options) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer->Write(msg, options, data.tag);
      });
    }

    auto WritesDone() {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer->WritesDone(data.tag);
      });
    }

    auto Finish(grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer->Finish(&status, data.tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncWriter<TRequest>> m_writer;
  };

  // ~Client Stream

  // Server Stream

  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareServerStreamFunc = std::unique_ptr<grpc::ClientAsyncReader<TResponse>>(TStub::*)(grpc::ClientContext*, const TRequest&, grpc::CompletionQueue*, void*);

  template<typename TResponse>
  class [[nodiscard]] ClientServerStreamCall {
  public:

    ClientServerStreamCall(std::unique_ptr<grpc::ClientAsyncReader<TResponse>> reader)
      : m_reader(std::move(reader))
    {}

    auto Read(TResponse& response) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader->Read(&response, data.tag);
      });
    }

    auto Finish(grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader->Finish(&status, data.tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncReader<TResponse>> m_reader;
  };

  // ~Server Stream

  // Bidirectional Stream

  template<typename TStub, typename TRequest, typename TResponse>
  using TPrepareBidirectionalStreamFunc = std::unique_ptr<grpc::ClientAsyncReaderWriter<TRequest, TResponse>>(TStub::*)(grpc::ClientContext*, grpc::CompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class [[nodiscard]] ClientBidirectionalStreamCall {
  public:

    ClientBidirectionalStreamCall(std::unique_ptr<grpc::ClientAsyncReaderWriter<TRequest, TResponse>> readerWriter)
      : m_readerWriter(std::move(readerWriter))
    {}

    auto Read(TResponse& response) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_readerWriter->Read(&response, data.tag);
      });
    }

    auto Write(const TRequest& msg) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_readerWriter->Write(msg, data.tag);
      });
    }

    auto Write(const TRequest& msg, grpc::WriteOptions options) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_readerWriter->Write(msg, options, data.tag);
      });
    }

    auto WritesDone() {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_readerWriter->WritesDone(data.tag);
      });
    }

    auto Finish(grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_readerWriter->Finish(&status, data.tag);
      });
    }

  private:
    std::unique_ptr<grpc::ClientAsyncReaderWriter<TRequest, TResponse>> m_readerWriter;
  };


  // ~Bidirectional Stream

#define ASYNC_GRPC_CLIENT_PREPARE_FUNC(client, rpc) &client::Service::Stub::Async ## rpc

  template<ServiceConcept TService>
  class Client {
  public:
    using Service = TService;

    Client(ChannelProvider channelProvider)
      : m_channelProvider(std::move(channelProvider))
    {}

    template<typename TRequest, typename TResponse>
    auto CallUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, grpc::ClientContext& context, const TRequest& request) {
      return ClientUnaryAwaitable<typename Service::Stub, TRequest, TResponse>(MakeStub(), func, context, request);
    }

    template<typename TRequest, typename TResponse>
    Coroutine CallUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, grpc::ClientContext& context, const TRequest& request, TResponse& response, grpc::Status& status) {
      auto call = co_await CallUnary(func, context, request);
      co_await call.Finish(response, status);
    }

    template<typename TRequest, typename TResponse, RetryOptionsConcept TRetryOptions = DefaultRetryOptions>
    Coroutine AutoRetryUnary(TPrepareUnaryFunc<typename Service::Stub, TRequest, TResponse> func, std::unique_ptr<grpc::ClientContext>& context, const TRequest& request, TResponse& response, grpc::Status& status, TRetryOptions retryOptions = {}) {
      while (true) {
        context = retryOptions.contextProvider();
        if (!co_await CallUnary(func, *context, request, response, status)) {
          break;
        }
        if (status.ok()) {
          break;
        }
        if (auto shouldRetry = retryOptions.retryPolicy(status); shouldRetry) {
          co_await *shouldRetry;
        } else {
          break;
        }
      }
    }

    template<typename TRequest, typename TResponse>
    auto CallClientStream(TPrepareClientStreamFunc<typename Service::Stub, TRequest, TResponse> func, grpc::ClientContext& context, TResponse& response) {
      return CompletionQueueAwaitable([stub = MakeStub(), func, &context, &response](const AwaitData& data) mutable {
        return ClientClientStreamCall((stub.*func)(&context, &response, data.cq, data.tag));
      });
    }

    template<typename TRequest, typename TResponse>
    auto CallServerStream(TPrepareServerStreamFunc<typename Service::Stub, TRequest, TResponse> func, grpc::ClientContext& context, const TRequest& request) {
      return CompletionQueueAwaitable([stub = MakeStub(), func, &context, &request](const AwaitData& data) mutable {
        return ClientServerStreamCall((stub.*func)(&context, request, data.cq, data.tag));
      });
    }

    template<typename TRequest, typename TResponse>
    auto CallBidirectionalStream(TPrepareBidirectionalStreamFunc<typename Service::Stub, TRequest, TResponse> func, grpc::ClientContext& context) {
      return CompletionQueueAwaitable([stub = MakeStub(), func, &context](const AwaitData& data) mutable {
        return ClientBidirectionalStreamCall((stub.*func)(&context, data.cq, data.tag));
      });
    }

  protected:
    ChannelProvider m_channelProvider;

    typename Service::Stub MakeStub() {
      return typename Service::Stub(m_channelProvider.SelectNextChannel());
    }
  };
}
