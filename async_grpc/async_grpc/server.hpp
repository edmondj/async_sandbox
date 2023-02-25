#pragma once

#include <atomic>
#include <thread>
#include <coroutine>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "async_grpc.hpp"

namespace async_grpc {
  struct Server;

  struct IServiceImpl {
    virtual ~IServiceImpl() = default;

    virtual grpc::Service* GetGrpcService() = 0;
    virtual void StartListening(Server& server) = 0;
  };

  template<ServiceConcept TService>
  class BaseServiceImpl : public IServiceImpl {
  public:
    using Service = TService;

    virtual grpc::Service* GetGrpcService() final {
      return &m_service;
    }

    typename Service::AsyncService& GetConcreteGrpcService() {
      return m_service;
    }

  private:
    typename TService::AsyncService m_service;
  };

  template<typename T>
  concept ServiceImplConcept = std::derived_from<T, IServiceImpl>&& ServiceConcept<typename T::Service>&& requires(T t) {
    { t.GetConcreteGrpcService() } -> std::same_as<typename T::Service::AsyncService&>;
  };

  template<typename T, typename TService>
  concept AsyncServiceBase = ServiceImplConcept<TService> && std::derived_from<typename TService::Service::AsyncService, T>;

  struct ServerOptions {
    std::vector<std::string> addresses;
    std::vector<std::reference_wrapper<IServiceImpl>> services;
    size_t executorCount = 2;
  };

  using ServerListenCoroutine = Coroutine;

  class ServerExecutor {
  public:
    ServerExecutor() = default;
    explicit ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> cq);

    grpc::CompletionQueue* GetCq() const;
    grpc::ServerCompletionQueue* GetNotifCq() const;

  private:
    std::unique_ptr<grpc::ServerCompletionQueue> m_cq;
  };
  using ServerExecutorThread = ExecutorThread<ServerExecutor>;

  class ServerContext {
  public:
    explicit ServerContext(const ServerExecutor& executor);

    const ServerExecutor& executor;
    grpc::ServerContext context;
  };

  template<std::derived_from<ServerContext> TContext, std::invocable<TContext&, void*> TFunc>
  class ListenAwaitable {
  public:
    ListenAwaitable(std::unique_ptr<TContext> context, TFunc func)
      : m_context(std::move(context))
      , m_func(std::move(func))
    {}

    bool await_ready() {
      return false;
    }

    void await_suspend(std::coroutine_handle<Coroutine::promise_type> h) {
      m_promise = &h.promise();
      m_func(*m_context, h.address());
    }

    std::unique_ptr<TContext> await_resume() {
      if (!m_promise->cancelled) {
        return std::move(m_context);
      }
      return nullptr;
    }

  private:
    std::unique_ptr<TContext> m_context;
    TFunc m_func;
    Coroutine::promise_type* m_promise = nullptr;
  };

  // Unary

  template<typename TService, typename TRequest, typename TResponse>
  using TUnaryListenFunc = void(TService::*)(grpc::ServerContext* context, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::CompletionQueue* cq, grpc::ServerCompletionQueue* notif_cq, void* tag);

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext : public ServerContext {
  public:
    explicit ServerUnaryContext(const ServerExecutor& executor)
      : ServerContext(executor)
      , m_response(&context)
    {}

    TRequest request;

    auto FinishWithError(const grpc::Status& status) {
      return Awaitable([&](void* tag) {
        m_response.FinishWithError(status, tag);
      });
    }

    auto Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_response.Finish(response, status, tag);
      });
    }

    template<typename TService, AsyncServiceBase<TService> TServiceBase>
    static auto Listen(TService& service, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc, const ServerExecutor& executor) {
      return ListenAwaitable(std::make_unique<ServerUnaryContext>(executor), [&](ServerUnaryContext& context, void* tag) mutable {
        (service.GetConcreteGrpcService().*listenFunc)(&context.context, &context.request, &context.m_response, executor.GetCq(), executor.GetNotifCq(), tag);
      });
    }

  private:
    grpc::ServerAsyncResponseWriter<TResponse> m_response;
  };

  using ServerUnaryCoroutine = Coroutine;

  template<typename T, typename TRequest, typename TResponse>
  concept ServerUnaryHandlerConcept = std::invocable<T, std::unique_ptr<ServerUnaryContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerUnaryContext<TRequest, TResponse>>&&>, ServerUnaryCoroutine>
    ;

  // ~Unary

  // Client Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TClientStreamListenFunc = void (TService::*)(grpc::ServerContext*, grpc::ServerAsyncReader<TResponse, TRequest>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerClientStreamContext : public ServerContext {
  public:
    explicit ServerClientStreamContext(const ServerExecutor& executor)
      : ServerContext(executor)
      , m_reader(&context)
    {}

    auto Read(TRequest& request) {
      return Awaitable([&](void* tag) {
        m_reader.Read(&request, tag);
      });
    }

    auto FinishWithError(const grpc::Status& status) {
      return Awaitable([&](void* tag) {
        m_reader.FinishWithError(status, tag);
      });
    }

    auto Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_reader.Finish(response, status, tag);
      });
    }

    template<typename TService, AsyncServiceBase<TService> TServiceBase>
    static auto Listen(TService& service, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, const ServerExecutor& executor) {
      return ListenAwaitable(std::make_unique<ServerClientStreamContext>(executor), [&](ServerClientStreamContext& context, void* tag) mutable {
        (service.GetConcreteGrpcService().*listenFunc)(&context.context, &context.m_reader, executor.GetCq(), executor.GetNotifCq(), tag);
      });
    }

  private:
    grpc::ServerAsyncReader<TResponse, TRequest> m_reader;
  };

  using ServerClientStreamCoroutine = Coroutine;

  template<typename T, typename TRequest, typename TResponse>
  concept ServerClientStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerClientStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerClientStreamContext<TRequest, TResponse>>&&>, ServerClientStreamCoroutine>
    ;

  // ~Client Stream

  // Server Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TServerStreamListenFunc = void (TService::*)(grpc::ServerContext*, TRequest*, grpc::ServerAsyncWriter<TResponse>*, ::grpc::CompletionQueue*, ::grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerServerStreamContext : public ServerContext {
  public:
    explicit ServerServerStreamContext(const ServerExecutor& executor)
      : ServerContext(executor)
      , m_writer(&context)
    {}

    TRequest request;

    auto Write(const TResponse& response) {
      return Awaitable([&](void* tag) {
        m_writer.Write(response, tag);
      });
    }

    auto Write(const TResponse& response, grpc::WriteOptions options) {
      return Awaitable([&](void* tag) {
        m_writer.Write(response, options, tag);
      });
    }

    auto WriteAndFinish(const TResponse& response, grpc::WriteOptions options, const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_writer.WriteAndFinish(response, options, status, tag);
      });
    }

    auto Finish(const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_writer.Finish(status, tag);
      });
    }

    template<typename TService, AsyncServiceBase<TService> TServiceBase>
    static auto Listen(TService& service, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, const ServerExecutor& executor) {
      return ListenAwaitable(std::make_unique<ServerServerStreamContext>(executor), [&](ServerServerStreamContext& context, void* tag) mutable {
        (service.GetConcreteGrpcService().*listenFunc)(&context.context, &context.request, &context.m_writer, executor.GetCq(), executor.GetNotifCq(), tag);
      });
    }

  private:
    grpc::ServerAsyncWriter<TResponse> m_writer;
  };

  using ServerServerStreamCoroutine = Coroutine;

  template<typename T, typename TRequest, typename TResponse>
  concept ServerServerStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerServerStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerServerStreamContext<TRequest, TResponse>>&&>, ServerServerStreamCoroutine>
    ;

  // ~Server Stream

  // Bidirectional Stream

  template<typename TService, typename TRequest, typename TResponse>
  using TBidirectionalStreamListenFunc = void (TService::*)(grpc::ServerContext*, grpc::ServerAsyncReaderWriter<TResponse, TRequest>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

  template<typename TRequest, typename TResponse>
  class ServerBidirectionalStreamContext : public ServerContext {
  public:
    explicit ServerBidirectionalStreamContext(const ServerExecutor& executor)
      : ServerContext(executor)
      , m_stream(&context)
    {}

    auto Read(TRequest& request) {
      return Awaitable([&](void* tag) {
        m_stream.Read(&request, tag);
      });
    }

    auto Write(const TResponse& response) {
      return Awaitable([&](void* tag) {
        m_stream.Write(response, tag);
      });
    }

    auto Write(const TResponse& response, grpc::WriteOptions options) {
      return Awaitable([&](void* tag) {
        m_stream.Write(response, options, tag);
      });
    }

    auto WriteAndFinish(const TResponse& response, grpc::WriteOptions options, const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_stream.WriteAndFinish(response, options, status, tag);
      });
    }

    auto Finish(const grpc::Status& status = grpc::Status::OK) {
      return Awaitable([&](void* tag) {
        m_stream.Finish(status, tag);
      });
    }

    template<typename TService, AsyncServiceBase<TService> TServiceBase>
    static auto Listen(TService& service, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, const ServerExecutor& executor) {
      return ListenAwaitable(std::make_unique<ServerBidirectionalStreamContext>(executor), [&](ServerBidirectionalStreamContext& context, void* tag) mutable {
        (service.GetConcreteGrpcService().*listenFunc)(&context.context, &context.m_stream, executor.GetCq(), executor.GetNotifCq(), tag);
      });
    }

  private:
    grpc::ServerAsyncReaderWriter<TResponse, TRequest> m_stream;
  };

  using ServerBidirectionalStreamCoroutine = Coroutine;

  template<typename T, typename TRequest, typename TResponse>
  concept ServerBidirectionalStreamHandlerConcept = std::invocable<T, std::unique_ptr<ServerBidirectionalStreamContext<TRequest, TResponse>>&&>
    && std::same_as<std::invoke_result_t<T, std::unique_ptr<ServerBidirectionalStreamContext<TRequest, TResponse>>&&>, ServerBidirectionalStreamCoroutine>
    ;

  // ~Server Stream

#define ASYNC_GRPC_SERVER_LISTEN_FUNC(service, rpc) &service::AsyncService::Request ## rpc

  class Server {
  public:
    explicit Server(ServerOptions options);

    // Will call Shutdown, see Shutdown
    ~Server();

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TServiceBase, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningUnary(TService& service, TUnaryListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (auto context = co_await ServerUnaryContext<TRequest, TResponse>::Listen(service, listenFunc, SelectNextExecutor())) {
        Coroutine::Spawn(handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TServiceBase, ServerClientStreamHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningClientStream(TService& service, TClientStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (auto context = co_await ServerClientStreamContext<TRequest, TResponse>::Listen(service, listenFunc, SelectNextExecutor())) {
        Coroutine::Spawn(handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TServiceBase, ServerServerStreamHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningServerStream(TService& service, TServerStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (auto context = co_await ServerServerStreamContext<TRequest, TResponse>::Listen(service, listenFunc, SelectNextExecutor())) {
        Coroutine::Spawn(handler(std::move(context)));
      }
    }

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TServiceBase, ServerBidirectionalStreamHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningBidirectionalStream(TService& service, TBidirectionalStreamListenFunc<TServiceBase, TRequest, TResponse> listenFunc, THandler handler) {
      while (auto context = co_await ServerBidirectionalStreamContext<TRequest, TResponse>::Listen(service, listenFunc, SelectNextExecutor())) {
        Coroutine::Spawn(handler(std::move(context)));
      }
    }

    // Will stop listening and will wait for all pending calls to complete. Use Shutdown(deadline) to forcibly cancel pending calls after some time
    void Shutdown();

    template<typename TDeadline>
    void Shutdown(const TDeadline& deadline) {
      m_server->Shutdown(deadline);
    }

  private:
    const ServerExecutor& SelectNextExecutor();

    ServerOptions m_options;
    std::unique_ptr<grpc::Server> m_server;

    std::vector<ServerExecutorThread> m_executors;
    std::atomic<size_t> m_nextExecutor = 0;
  };
}