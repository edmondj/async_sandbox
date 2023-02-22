/*
#include <iostream>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>
#include <variable_service.grpc.pb.h>
#include <thread>
#include <coroutine>
#include <map>
#include <string>
#include <sstream>

struct grpc_promise
{
  bool last_ok = false;
};

struct listen_coroutine
{
  struct promise_type : grpc_promise
  {
    listen_coroutine get_return_object() {
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

template<typename TService, typename TServiceBase, typename TRequest, typename TResponse>
class poll_unary
{
  using listen_func_ptr = void (TServiceBase::*)(grpc::ServerContext*, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void*);

public:
  poll_unary(TService& service, listen_func_ptr listen_func, grpc::ServerContext* context, TRequest* request, grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::CompletionQueue* call_cq, grpc::ServerCompletionQueue* notification_cq)
    : m_service(service)
    , m_listen_func(listen_func)
    , m_context(context)
    , m_request(request)
    , m_response(response)
    , m_call_cq(call_cq)
    , m_notification_cq(notification_cq)
  {}

   
  bool await_ready() {
    return false;
  }

  template<std::derived_from<grpc_promise> TPromise>
  void await_suspend(std::coroutine_handle<TPromise> h) {
    m_promise = &h.promise();
    void* tag = h.address();
    std::printf("Listening tag %p\n", tag);
    (m_service.*m_listen_func)(m_context, m_request, m_response, m_call_cq, m_notification_cq, tag);
  }

  [[nodiscard]] bool await_resume() {
    return m_promise->last_ok;
  }

private:
  TService& m_service;
  listen_func_ptr m_listen_func;
  grpc::ServerContext* m_context;
  TRequest* m_request;
  grpc::ServerAsyncResponseWriter<TResponse>* m_response;
  grpc::CompletionQueue* m_call_cq;
  grpc::ServerCompletionQueue* m_notification_cq;
  grpc_promise* m_promise = nullptr;
};

template<typename TResponse>
struct unary_finish_with_error {
  unary_finish_with_error(grpc::ServerAsyncResponseWriter<TResponse>* response, grpc::Status status)
    : m_response(response)
    , m_status(std::move(status))
  {}

  bool await_ready() {
    return false;
  }

  template<std::derived_from<grpc_promise> TPromise>
  void await_suspend(std::coroutine_handle<TPromise> h) {
    m_promise = &h.promise();
    void* tag = h.address();
    std::printf("Finishing tag %p\n", tag);
    m_response->FinishWithError(m_status, tag);
  }

  [[nodiscard]] bool await_resume() {
    return m_promise->last_ok;
  }

  grpc::ServerAsyncResponseWriter<TResponse>* m_response;
  grpc::Status m_status;
  grpc_promise* m_promise = nullptr;
};

struct unary_coroutine
{
  struct promise_type : grpc_promise
  {
    unary_coroutine get_return_object() {
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
struct unary_context
{
  grpc::ServerContext context;
  TRequest request;
  grpc::ServerAsyncResponseWriter<TResponse> response{ &context };
};

template<typename T>
struct grpc_alarm
{
  grpc_alarm(grpc::CompletionQueue* cq, T&& deadline)
    : m_cq(cq)
    , m_deadline(std::forward<T>(deadline))
  {}

  bool await_ready() {
    return false;
  }

  template<std::derived_from<grpc_promise> TPromise>
  void await_suspend(std::coroutine_handle<TPromise> h) {
    m_promise = &h.promise();
    void* tag = h.address();
    std::printf("Alarm tag %p\n", tag);
    m_alarm.Set(m_cq, m_deadline, tag);
  }

  [[nodiscard]] bool await_resume() {
    return m_promise->last_ok;
  }

  grpc::Alarm m_alarm;
  grpc::CompletionQueue* m_cq;
  T m_deadline;
  grpc_promise* m_promise = nullptr;
};

template<typename... TArgs>
std::string dumb_format(TArgs&&... args)
{
  std::ostringstream out;
  (out << ... << std::forward<TArgs>(args));
  return out.str();
}

int main()
{
  grpc::ServerBuilder builder;
  builder.AddListeningPort("[::1]:4213", grpc::InsecureServerCredentials());
  auto cq = builder.AddCompletionQueue();

  variable_service::VariableService::AsyncService service;
  builder.RegisterService(&service);

  auto server = builder.BuildAndStart();

  auto read_handler = [cq = cq.get()](std::unique_ptr<unary_context<variable_service::ReadRequest, variable_service::ReadResponse>> context) -> unary_coroutine {
    auto deadline = context->context.client_metadata().find("debug_delay_ms");
    if (deadline != context->context.client_metadata().end())
    {
      if (!co_await grpc_alarm(cq, std::chrono::system_clock::now() + std::chrono::milliseconds(std::atoi(deadline->second.data()))))
      {
        // alarm canceled, most likely shutting down
        co_return;
      }
    }
    if (!co_await unary_finish_with_error(&context->response, grpc::Status(grpc::StatusCode::NOT_FOUND, dumb_format("Key ", context->request.key(), " not found"))))
    {
      // finish with error failed, most likely client is dead
    }
  };

  auto read_listener = [&service, cq = cq.get(), read_handler]() -> listen_coroutine {
    while (true)
    {
      auto context = std::make_unique<unary_context<variable_service::ReadRequest, variable_service::ReadResponse>>();
      if (!co_await poll_unary(service, &variable_service::VariableService::AsyncService::RequestRead, &context->context, &context->request, &context->response, cq, cq))
      {
        // listen failed, most likely shutting down
        co_return;
      }
      read_handler(std::move(context));
    }
  };

  read_listener();

  auto grpc_thread = std::jthread([cq = cq.get()]()
  {
    void* tag;
    bool ok;
    while (cq->Next(&tag, &ok))
    {
      std::printf("Got %p %s\n", tag, (ok ? "true" : "false"));
      auto h = std::coroutine_handle<grpc_promise>::from_address(tag);
      h.promise().last_ok = ok;
      h.resume();
      if (h.done()) {
        h.destroy();
      }
    }
  });

  std::cin.get();
  server->Shutdown();
  cq->Shutdown();
  grpc_thread.join();
}
*/

#include <iostream>
#include <async_grpc/server.hpp>
#include <protos/echo_service.grpc.pb.h>
#include <async_grpc/threads.hpp>

class EchoServiceImpl : public async_grpc::BaseServiceImpl<echo_service::EchoService> {
public:
  virtual void StartListening(async_grpc::Server& server) override {
    server.StartListeningUnary(*this, ASYNC_GRPC_SERVER_LISTEN_FUNC(echo_service::EchoService, UnaryEcho),
      [](std::unique_ptr<async_grpc::ServerUnaryContext<echo_service::UnaryEchoRequest, echo_service::UnaryEchoResponse>> context) -> async_grpc::ServerUnaryCoroutine {
        assert(async_grpc::ThisThreadIsGrpc());

        echo_service::UnaryEchoResponse response;
        response.set_message(context->GetRequest().message());
        co_await context->Finish(response);
      }
    );
  }
};

int main() {

  EchoServiceImpl echo;

  auto server = [&]() {
    async_grpc::ServerOptions options;
    options.addresses.push_back("[::1]:4213");
    options.services.push_back(std::ref(echo));
    return async_grpc::Server(std::move(options));
  }();

  std::cin.get();
  std::cout << "Shutting down" << std::endl;
}