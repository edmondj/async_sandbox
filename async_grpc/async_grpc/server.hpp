#pragma once

#include <atomic>
#include <thread>
#include <coroutine>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "service_impl.hpp"
#include "async_grpc.hpp"

namespace async_grpc {

  struct ServerOptions {
    std::vector<std::string> addresses;
    std::vector<std::reference_wrapper<IServiceImpl>> services;
    size_t executorCount = 2;
  };

  using ServerListenCoroutine = GrpcCoroutine;

  class Server;

  class ServerExecutor {
  public:
    ServerExecutor() = default;
    explicit ServerExecutor(std::unique_ptr<grpc::ServerCompletionQueue> cq);

    void Shutdown();

    grpc::CompletionQueue* GetCq() const;
    grpc::ServerCompletionQueue* GetNotifCq() const;

  private:
    std::unique_ptr<grpc::ServerCompletionQueue> m_cq;
  };

  class ServerContext {
  public:
    const grpc::ServerContext& GetContext() const { return m_context; }
    grpc::ServerContext& GetContext() { return m_context; }

  protected:
    grpc::ServerContext m_context;
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

    void await_suspend(std::coroutine_handle<GrpcCoroutine::promise_type> h) {
      m_promise = &h.promise();
      m_func(*m_context, h.address());
    }

    std::unique_ptr<TContext> await_resume() {
      if (m_promise->lastOk) {
        return std::move(m_context);
      }
      return nullptr;
    }

  private:
    std::unique_ptr<TContext> m_context;
    TFunc m_func;
    GrpcCoroutine::promise_type* m_promise = nullptr;
  };

  // Unary

  template<typename TService, typename TRequest, typename TResponse>
  using TUnaryListenFunc = void(TService::*)(grpc::ServerContext* context, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::CompletionQueue* cq, grpc::ServerCompletionQueue* notif_cq, void* tag);

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext : public ServerContext {
  public:
    ServerUnaryContext()
      : m_response(&m_context)
    {}

    const TRequest& GetRequest() const { return m_request; }

    auto FinishWithError(const grpc::Status& status) {
      return GrpcAwaitable([&](void* tag) {
        m_response.FinishWithError(status, tag);
      });
    }

    auto Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return GrpcAwaitable([&](void* tag) {
        m_response.Finish(response, status, tag);
      });
    }

    template<typename TService, AsyncServiceBase<TService> TGrpcService>
    static auto Listen(TService& service, TUnaryListenFunc<TGrpcService, TRequest, TResponse> listenFunc, ServerExecutor& executor) {
      return ListenAwaitable(std::make_unique<ServerUnaryContext>(), [&](ServerUnaryContext& context, void* tag) mutable {
          (service.GetConcreteGrpcService().*listenFunc)(&context.m_context, &context.m_request, &context.m_response, executor.GetCq(), executor.GetNotifCq(), tag);
        }
      );
    }

  private:
    TRequest m_request;
    grpc::ServerAsyncResponseWriter<TResponse> m_response;
  };

  using ServerUnaryCoroutine = GrpcCoroutine;

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
    ServerClientStreamContext()
      : m_reader(&m_context)
    {}
    
    grpc::ServerAsyncReader<TResponse, TRequest>& GetReader() { return m_reader; }

  private:
    grpc::ServerAsyncReader<TResponse, TRequest> m_reader;
  };

  // ~Client Stream

#define ASYNC_GRPC_SERVER_LISTEN_FUNC(service, method) &service::AsyncService::Request ## method

  class Server {
  public:
    explicit Server(ServerOptions options);
    ~Server();

    friend class ServerContext;
    template<typename TRequest, typename TResponse>
    friend class ServerUnaryContext;

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TGrpcService, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningUnary(TService& service, TUnaryListenFunc<TGrpcService, TRequest, TResponse> listenFunc, THandler&& handler) {
      while (auto context = co_await ServerUnaryContext<TRequest, TResponse>::Listen(service, listenFunc, GetNextExecutor())) {
        std::forward<THandler>(handler)(std::move(context));
      }
    }

  private:
    ServerExecutor& GetNextExecutor() {
      // Round Robin on all executors, more algorithms possible
      return m_executors[m_nextExecutor++ % m_executors.size()].executor;
    }

    ServerOptions m_options;
    std::unique_ptr<grpc::Server> m_server;
    
    struct Executor {
      CompletionQueueThread thread;
      ServerExecutor executor;
    };
    std::vector<Executor> m_executors;
    std::atomic<size_t> m_nextExecutor = 0;
  };
}