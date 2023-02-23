#include "client.hpp"

namespace async_grpc {

  ClientExecutor::ClientExecutor() noexcept
    : m_cq(std::make_unique<grpc::CompletionQueue>())
  {}

  grpc::CompletionQueue* ClientExecutor::GetCq() const
  {
    return m_cq.get();
  }

  ClientContext::ClientContext(const ClientExecutor& executor) noexcept
    : executor(executor)
  {}

  ChannelProvider::ChannelProvider(std::shared_ptr<grpc::Channel> channel) noexcept
      : m_channels(1, std::move(channel))
  {}

  ChannelProvider::ChannelProvider(std::vector<std::shared_ptr<grpc::Channel>> channels) noexcept
      : m_channels(std::move(channels))
  {
      assert(!m_channels.empty());
  }

  ChannelProvider::ChannelProvider(ChannelProvider&& other) noexcept
      : m_channels(std::move(other.m_channels))
      , m_nextChannel(other.m_nextChannel.load())
  {}

  const std::shared_ptr<grpc::Channel>& ChannelProvider::SelectNextChannel() {
      return m_channels[++m_nextChannel % m_channels.size()];
  }

}