#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "async_grpc.hpp"

namespace async_grpc {
  class Server;

  struct IServiceImpl {
    virtual ~IServiceImpl() = default;

    virtual grpc::Service* GetGrpcService() = 0;
    virtual void StartListening(Server& server) = 0;
  };

  class ServerExecutor : public CompletionQueueExecutor {
  public:
    ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> notifCq) noexcept;

    ServerExecutor(ServerExecutor&&) = default;
    ServerExecutor& operator=(ServerExecutor&&) = default;

    grpc::ServerCompletionQueue* GetNotifCq();
  };

  struct ServerOptions {
    std::vector<std::string> addresses;
    std::vector<std::reference_wrapper<IServiceImpl>> services;
    size_t executorCount = 2;
    size_t threadsPerExecutor = 2;
    std::unique_ptr<grpc::ServerBuilderOption> options;
  };

  struct ServerContext {
    grpc::ServerContext context;
  };

  // Unary

  template<typename TService, typename TRequest, typename TResponse>
  using TUnaryListenFunc = void(TService::*)(grpc::ServerContext* context, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::CompletionQueue* cq, grpc::ServerCompletionQueue* notif_cq, void* tag);

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext : public ServerContext {
  public:
    ServerUnaryContext()
      : m_response(&context)
    {}

    TRequest request;

    auto FinishWithError(const grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_response.FinishWithError(status, data.tag);
      });
    }

    auto Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_response.Finish(response, status, data.tag);
      });
    }

    template<typename TService, typename TServiceBase>
    auto Listen(ServerExecutor& executor, TService& service, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc) {
      return CompletionQueueAwaitable([&, listenFunc](const AwaitData& data) {
        (service.*listenFunc)(&context, &request, &m_response, executor.GetCq(), executor.GetNotifCq(), data.tag);
      });
    }

  private:
    grpc::ServerAsyncResponseWriter<TResponse> m_response;
  };

  template<typename T, typename TRequest, typename TResponse>
  concept ServerUnaryHandlerConcept = std::invocable<T, std::unique_ptr<ServerUnaryContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerUnaryContext<TRequest, TResponse>>&&>, Task<>>
    ;

  // ~Unary

  // Client Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TClientStreamListenFunc = void (TService::*)(grpc::ServerContext*, grpc::ServerAsyncReader<TResponse, TRequest>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerClientStreamContext : public ServerContext {
  public:
    ServerClientStreamContext()
      : m_reader(&context)
    {}

    auto Read(TRequest& request) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader.Read(&request, data.tag);
      });
    }

    auto FinishWithError(const grpc::Status& status) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader.FinishWithError(status, data.tag);
      });
    }

    auto Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_reader.Finish(response, status, data.tag);
      });
    }

    template<typename TService, typename TServiceBase>
    auto Listen(ServerExecutor& executor, TService& service, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc) {
      return CompletionQueueAwaitable([&, listenFunc](const AwaitData& data) mutable {
        (service.*listenFunc)(&context, &m_reader, executor.GetCq(), executor.GetNotifCq(), data.tag);
      });
    }

  private:
    grpc::ServerAsyncReader<TResponse, TRequest> m_reader;
  };

  template<typename T, typename TRequest, typename TResponse>
  concept ServerClientStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerClientStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerClientStreamContext<TRequest, TResponse>>&&>, Task<>>
    ;

  // ~Client Stream

  // Server Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TServerStreamListenFunc = void (TService::*)(grpc::ServerContext*, TRequest*, grpc::ServerAsyncWriter<TResponse>*, ::grpc::CompletionQueue*, ::grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerServerStreamContext : public ServerContext {
  public:
    ServerServerStreamContext()
      : m_writer(&context)
    {}

    TRequest request;

    auto Write(const TResponse& response) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer.Write(response, data.tag);
      });
    }

    auto Write(const TResponse& response, grpc::WriteOptions options) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer.Write(response, options, data.tag);
      });
    }

    auto WriteAndFinish(const TResponse& response, grpc::WriteOptions options, const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer.WriteAndFinish(response, options, status, data.tag);
      });
    }

    auto Finish(const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_writer.Finish(status, data.tag);
      });
    }

    template<typename TService, typename TServiceBase>
    auto Listen(ServerExecutor& executor, TService& service, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc) {
      return CompletionQueueAwaitable([&, listenFunc](const AwaitData& data) mutable {
        (service.*listenFunc)(&context, &request, &m_writer, executor.GetCq(), executor.GetNotifCq(), data.tag);
      });
    }

  private:
    grpc::ServerAsyncWriter<TResponse> m_writer;
  };

  template<typename T, typename TRequest, typename TResponse>
  concept ServerServerStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerServerStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerServerStreamContext<TRequest, TResponse>>&&>, Task<>>
    ;

  // ~Server Stream

  // Bidirectional Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TBidirectionalStreamListenFunc = void (TService::*)(grpc::ServerContext*, grpc::ServerAsyncReaderWriter<TResponse, TRequest>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerBidirectionalStreamContext : public ServerContext {
  public:
    explicit ServerBidirectionalStreamContext()
      : m_stream(&context)
    {}

    auto Read(TRequest& request) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_stream.Read(&request, data.tag);
      });
    }

    auto Write(const TResponse& response) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_stream.Write(response, data.tag);
      });
    }

    auto Write(const TResponse& response, grpc::WriteOptions options) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_stream.Write(response, options, data.tag);
      });
    }

    auto WriteAndFinish(const TResponse& response, grpc::WriteOptions options, const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_stream.WriteAndFinish(response, options, status, data.tag);
      });
    }

    auto Finish(const grpc::Status& status = grpc::Status::OK) {
      return CompletionQueueAwaitable([&](const AwaitData& data) {
        m_stream.Finish(status, data.tag);
      });
    }

    template<typename TService, typename TServiceBase>
    auto Listen(ServerExecutor& executor, TService& service, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc) {
      return CompletionQueueAwaitable([&, listenFunc](const AwaitData& data) mutable {
        (service.*listenFunc)(&context, &m_stream, executor.GetCq(), executor.GetNotifCq(), data.tag);
      });
    }

  private:
    grpc::ServerAsyncReaderWriter<TResponse, TRequest> m_stream;
  };

  template<typename T, typename TRequest, typename TResponse>
  concept ServerBidirectionalStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerBidirectionalStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerBidirectionalStreamContext<TRequest, TResponse>>&&>, Task<>>
    ;

  // ~Bidirectional Stream

#define ASYNC_GRPC_SERVER_LISTEN_FUNC(service, rpc) &service::AsyncService::Request ## rpc

  class Server {
  public:
    explicit Server(ServerOptions&& options);

    // Will call Shutdown, see Shutdown
    ~Server();

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    void StartListeningUnary(TService& service, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      async_lib::Spawn(SelectNextExecutor(), ListenUnary(service, listenFunc, std::move(handler)));
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerClientStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    void StartListeningClientStream(TService& service, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      async_lib::Spawn(SelectNextExecutor(), ListenClientStream(service, listenFunc, std::move(handler)));
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerServerStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    void StartListeningServerStream(TService& service, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      async_lib::Spawn(SelectNextExecutor(), ListenServerStream(service, listenFunc, std::move(handler)));
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerBidirectionalStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    void StartListeningBidirectionalStream(TService& service, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      async_lib::Spawn(SelectNextExecutor(), ListenBidirectionalStream(service, listenFunc, std::move(handler)));
    }

    // Will stop listening and will wait for all pending calls to complete. Use Shutdown(deadline) to forcibly cancel pending calls after some time
    void Shutdown();

    template<typename TDeadline>
    void Shutdown(const TDeadline& deadline) {
      m_server->Shutdown(deadline);
    }

  private:
    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    Task<> ListenUnary(TService& service, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (true) {
        auto& executor = SelectNextExecutor();
        auto context = std::make_unique<ServerUnaryContext<TRequest, TResponse>>();
        if (!co_await context->Listen(executor, service, listenFunc)) {
          break;
        }
        async_lib::Spawn(executor, handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerClientStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    Task<> ListenClientStream(TService& service, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (true) {
        auto& executor = SelectNextExecutor();
        auto context = std::make_unique<ServerClientStreamContext<TRequest, TResponse>>();
        if (!co_await context->Listen(executor, service, listenFunc)) {
          break;
        }
        async_lib::Spawn(executor, handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerServerStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    Task<> ListenServerStream(TService& service, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (true) {
        auto& executor = SelectNextExecutor();
        auto context = std::make_unique<ServerServerStreamContext<TRequest, TResponse>>();
        if (!co_await context->Listen(executor, service, listenFunc)) {
          break;
        }
        async_lib::Spawn(executor, handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, typename TService, typename TServiceBase, ServerBidirectionalStreamHandlerConcept<TRequest, TResponse> THandler>
      requires std::derived_from<TService, TServiceBase>
    Task<> ListenBidirectionalStream(TService& service, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (true) {
        auto& executor = SelectNextExecutor();
        auto context = std::make_unique<ServerBidirectionalStreamContext<TRequest, TResponse>>();
        if (!co_await context->Listen(executor, service, listenFunc)) {
          break;
        }
        async_lib::Spawn(executor, handler(std::move(context)));
      }
    }

    ServerExecutor& SelectNextExecutor();

    std::unique_ptr<grpc::Server> m_server;

    std::vector<ExecutorThreads<ServerExecutor>> m_executors;
    std::atomic<size_t> m_nextExecutor = 0;
  };

  template<ServiceConcept TService>
  class BaseServiceImpl : public IServiceImpl {
  public:
    BaseServiceImpl() = default;

    using Service = TService;

    virtual grpc::Service* GetGrpcService() final {
      return &m_service;
    }

    template<typename TRequest, typename TResponse, typename TServiceBase, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
    void StartListeningUnary(Server& server, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      server.StartListeningUnary(m_service, listenFunc, std::move(handler));
    }

    template<typename TRequest, typename TResponse, typename TServiceBase, ServerClientStreamHandlerConcept<TRequest, TResponse> THandler>
    void StartListeningClientStream(Server& server, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      server.StartListeningClientStream(m_service, listenFunc, std::move(handler));
    }

    template<typename TRequest, typename TResponse, typename TServiceBase, ServerServerStreamHandlerConcept<TRequest, TResponse> THandler>
    void StartListeningServerStream(Server& server, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      server.StartListeningServerStream(m_service, listenFunc, std::move(handler));
    }

    template<typename TRequest, typename TResponse, typename TServiceBase, ServerBidirectionalStreamHandlerConcept<TRequest, TResponse> THandler>
    void StartListeningBidirectionalStream(Server& server, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      server.StartListeningBidirectionalStream(m_service, listenFunc, std::move(handler));
    }

  private:
    BaseServiceImpl(const BaseServiceImpl&) = delete;
    BaseServiceImpl(BaseServiceImpl&&) = delete;
    BaseServiceImpl& operator=(const BaseServiceImpl&) = delete;
    BaseServiceImpl& operator=(BaseServiceImpl&&) = delete;

    typename TService::AsyncService m_service;
  };

}