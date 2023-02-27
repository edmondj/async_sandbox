#include <iostream>
#include <async_grpc/server.hpp>
#include "echo_service_impl.hpp"
#include "variable_service_impl.hpp"

int main() {
  std::cout << "Setting up services..." << std::endl;
  EchoServiceImpl echo;
  VariableServiceImpl variable;

  std::cout << "Setting up server..." << std::endl;
  auto server = [&]() {
    async_grpc::ServerOptions options;
    options.addresses.push_back("[::1]:4213");
    options.services.push_back(echo);
    options.services.push_back(variable);
    return async_grpc::Server(std::move(options));
  }();

  std::cout << "Server started" << std::endl;

  std::cin.get();
  std::cout << "Shutting down" << std::endl;
  server.Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(30));
}