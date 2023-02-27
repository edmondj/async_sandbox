#include <iostream>
#include <async_grpc/server.hpp>
#include <utils/Logs.hpp>
#include "echo_service_impl.hpp"
#include "variable_service_impl.hpp"

int main() {
  utils::Log() << "Setting up services...";
  EchoServiceImpl echo;
  VariableServiceImpl variable;

  utils::Log() << "Setting up server...";
  auto server = [&]() {
    async_grpc::ServerOptions options;
    options.addresses.push_back("[::1]:4213");
    options.services.push_back(echo);
    options.services.push_back(variable);
    return async_grpc::Server(std::move(options));
  }();

  utils::Log() << "Server started";

  std::cin.get();
  utils::Log() << "Shutting down...";
  server.Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(30));
  utils::Log() << "Bye!";
}