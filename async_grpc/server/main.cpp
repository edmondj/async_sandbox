#include <iostream>
#include <async_grpc/server.hpp>
#include "echo_service_impl.hpp"

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