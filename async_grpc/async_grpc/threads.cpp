#include "threads.hpp"
#include <unordered_set>
#include <shared_mutex>
#include <mutex>

namespace async_grpc {

  static std::unordered_set<std::thread::id> grpcThreads;
  static std::shared_mutex grpcThreadsMutex;

  ScopedGrpcThread::ScopedGrpcThread()
  {
    auto lock = std::unique_lock(grpcThreadsMutex);
    grpcThreads.insert(std::this_thread::get_id());
  }

  ScopedGrpcThread::~ScopedGrpcThread()
  {
    auto lock = std::unique_lock(grpcThreadsMutex);
    grpcThreads.erase(std::this_thread::get_id());
  }

  bool ThisThreadIsGrpc()
  {
    auto lock = std::shared_lock(grpcThreadsMutex);
    return grpcThreads.contains(std::this_thread::get_id());
  }

}