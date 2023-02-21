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

  struct ServerListenCoroutine {

    struct promise_type : BaseGrpcPromise
    {
      ServerListenCoroutine get_return_object() {
        return {};
      }

      std::suspend_never initial_suspend() {
        return {};
      }

      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void return_void() {}
      void unhandled_exception() {}
    };

  };

  class Server;

  class ServerContext {
  public:
    explicit ServerContext(Server& server);
    const grpc::ServerContext& GetContext() const { return m_context; }
    grpc::ServerContext& GetContext() { return m_context; }

  protected:
    Server& m_server;
    grpc::ServerContext m_context;

    friend class Server;
  };

  // Unary

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext : public ServerContext {
  public:
    ServerUnaryContext(Server& server)
      : ServerContext(server)
      , m_response(&m_context)
    {}

    const TRequest& GetRequest() const { return m_request; }

    class FinishWithErrorAwaitable;
    FinishWithErrorAwaitable FinishWithError(const grpc::Status& status) {
      return FinishWithErrorAwaitable(m_response, status);
    }

    class FinishAwaitable;
    FinishAwaitable Finish(const TResponse& response, const grpc::Status& status = grpc::Status::OK) {
      return FinishAwaitable(m_response, response, status);
    }

  private:
    TRequest m_request;
    grpc::ServerAsyncResponseWriter<TResponse> m_response;
    
    friend class Server;
  };

  struct ServerUnaryCoroutine
  {
    struct promise_type : BaseGrpcPromise
    {
      ServerUnaryCoroutine get_return_object() {
        return {};
      }

      std::suspend_never initial_suspend() {
        return {};
      }

      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void return_void() {}
      void unhandled_exception() {}
    };
  };

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext<TRequest, TResponse>::FinishAwaitable {
  public:
    FinishAwaitable(grpc::ServerAsyncResponseWriter<TResponse>& response, const TResponse& message, const grpc::Status& status)
      : m_response(response)
      , m_message(message)
      , m_status(status)
    {}

    bool await_ready() {
      return false;
    }

    void await_suspend(std::coroutine_handle<ServerUnaryCoroutine::promise_type> h) {
      m_promise = &h.promise();
      m_response.Finish(m_message, m_status, h.address());
    }

    bool await_resume() {
      return m_promise->lastOk;
    }

  private:
    grpc::ServerAsyncResponseWriter<TResponse>& m_response;
    const TResponse& m_message;
    const grpc::Status& m_status;
    BaseGrpcPromise* m_promise = nullptr;
  };

  template<typename TRequest, typename TResponse>
  class ServerUnaryContext<TRequest, TResponse>::FinishWithErrorAwaitable {
  public:
    FinishWithErrorAwaitable(grpc::ServerAsyncResponseWriter<TResponse>& response, const grpc::Status& status)
      : m_response(response)
      , m_status(status)
    {}

    bool await_ready() {
      return false;
    }

    void await_suspend(std::coroutine_handle<ServerUnaryCoroutine::promise_type> h) {
      m_promise = &h.promise();
      m_response.FinishWithError(m_status, h.address());
    }

    bool await_resume() {
      return m_promise->lastOk;
    }

  private:
    grpc::ServerAsyncResponseWriter<TResponse>& m_response;
    const grpc::Status& m_status;
    BaseGrpcPromise* m_promise = nullptr;
  };

  template<typename T, typename TRequest, typename TResponse>
  concept ServerUnaryHandlerConcept = requires(T handler, std::unique_ptr<ServerUnaryContext<TRequest, TResponse>> arg) {
    { handler(std::move(arg)) } -> std::same_as<ServerUnaryCoroutine>;
  };

  template<typename TService, typename TRequest, typename TResponse>
  using TUnaryListenFunc = void(TService::*)(grpc::ServerContext* context, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::CompletionQueue* cq, grpc::ServerCompletionQueue* notif_cq, void* tag);

  // ~Unary

  class Server {
  public:
    explicit Server(ServerOptions options);
    ~Server();

    friend class ServerContext;
    template<typename TRequest, typename TResponse>
    friend class ServerUnaryContext;

    template<typename TRequest, typename TResponse, ServiceImplConcept TService, AsyncServiceBase<TService> TGrpcService, ServerUnaryHandlerConcept<TRequest, TResponse> THandler>
    ServerListenCoroutine StartListeningUnary(TService& service, TUnaryListenFunc<TGrpcService, TRequest, TResponse> listenFunc, THandler&& handler) {
      struct PollUnary {
        grpc::ServerCompletionQueue* cq;
        TService& service;
        TUnaryListenFunc<TGrpcService, TRequest, TResponse> listenFunc;
        grpc::ServerContext& context;
        TRequest& request;
        grpc::ServerAsyncResponseWriter<TResponse>& response;
        BaseGrpcPromise* m_promise = nullptr;
        
        bool await_ready() {
          return false;
        }

        void await_suspend(std::coroutine_handle<ServerListenCoroutine::promise_type> h) {
          m_promise = &h.promise();
          (service.GetConcreteGrpcService().*listenFunc)(&context, &request, &response, cq, cq, h.address());
        }

        bool await_resume() {
          return m_promise->lastOk;
        }

      };

      while (true)
      {
        auto context = std::make_unique<ServerUnaryContext<TRequest, TResponse>>(*this);
        grpc::ServerCompletionQueue* cq = GetNextCq();
        if (!co_await PollUnary{ GetNextCq(), service, listenFunc, context->m_context, context->m_request, context->m_response })
        {
          // listen failed, most likely shutting down
          co_return;
        }
        handler(std::move(context));
      }
    }

  private:
    grpc::ServerCompletionQueue* GetNextCq() {
      // Round Robin on all executors, more algorithms possible
      return m_executors[m_nextExecutor++ % m_executors.size()].cq.get();
    }

    ServerOptions m_options;
    std::unique_ptr<grpc::Server> m_server;
    
    struct Executor {
      CompletionQueueThread thread;
      std::unique_ptr<grpc::ServerCompletionQueue> cq;
    };
    std::vector<Executor> m_executors;
    std::atomic<size_t> m_nextExecutor = 0;
  };
}