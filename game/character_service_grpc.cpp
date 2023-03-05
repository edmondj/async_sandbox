#include "character_service_grpc.hpp"
#include <async_grpc/iostream.hpp>
#include <utils/logs.hpp>

static auto Log() {
  return utils::Log() << "[CharacterServiceGrpc] ";
}

CharacterServiceGrpc::CharacterServiceGrpc()
  : m_client(grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials()))
{}

async_game::Task<Player> CharacterServiceGrpc::GetPlayer()
{
  co_return co_await async_lib::SpawnCrossTask(m_executor.GetExecutor(), [this]() -> async_grpc::Task<Player> {
    Player sent;

    if (auto res = co_await Read("level")) {
      sent.level = res->value_or(1);
    }
    if (auto res = co_await Read("xp")) {
      sent.xp = res->value_or(0);
    }

    co_return sent;
  }());
}

async_game::Task<int64_t> CharacterServiceGrpc::GiveXp(int64_t ammount)
{
  co_return co_await async_lib::SpawnCrossTask(m_executor.GetExecutor(), [this, ammount]() -> async_grpc::Task<int64_t> {
    int64_t cur_xp = (co_await Read("xp")).value_or(std::nullopt).value_or(0);
    co_await Write("xp", cur_xp + ammount);
    co_return cur_xp;
  }());
}

async_game::Task<int64_t> CharacterServiceGrpc::LevelUp()
{
  co_return co_await async_lib::SpawnCrossTask(m_executor.GetExecutor(), [this]() -> async_grpc::Task<int64_t> {
    int64_t cur_level = (co_await Read("level")).value_or(std::nullopt).value_or(1);
    co_await Write("level", cur_level + 1);
    co_await Write("xp", 0);
    co_return cur_level;
  }());
}

async_game::Task<> CharacterServiceGrpc::Reset()
{
  co_await async_lib::SpawnCrossTask(m_executor.GetExecutor(), [this]() -> async_grpc::Task<> {

    co_await Del("level");
    co_await Del("xp");

  }());
}

async_grpc::Task<utils::expected<std::optional<int64_t>, grpc::Status>> CharacterServiceGrpc::Read(std::string_view name)
{
  Log() << "Requesting " << name;
  std::unique_ptr<grpc::ClientContext> context;
  variable_service::ReadRequest request;
  request.set_key(std::string(name));
  variable_service::ReadResponse response;
  auto status = grpc::Status::CANCELLED;
  if (co_await m_client.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(variable_service::VariableService, Read), context, request, response, status))
  {
    if (status.ok()) {
      Log() << "Got " << name << ", value: " << response.value();
      co_return response.value();
    }
    if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
      Log() << "Value " << name << " not found";
      co_return std::nullopt;
    }
    Log() << "Failed to retrieve " << name << ": " << status;
  } else {
    Log() << "Retrieving " << name << " cancelled";
  }
  co_return utils::unexpected(status);
}

async_grpc::Task<utils::expected<void, grpc::Status>> CharacterServiceGrpc::Write(std::string_view name, int64_t value)
{
  Log() << "Writing " << name << " to " << value;
  std::unique_ptr<grpc::ClientContext> context;
  variable_service::WriteRequest request;
  request.set_key(std::string(name));
  request.set_value(value);
  variable_service::WriteResponse response;
  auto status = grpc::Status::CANCELLED;
  if (co_await m_client.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(variable_service::VariableService, Write), context, request, response, status))
  {
    if (status.ok()) {
      Log() << "Successfuly wrote " << name;
      co_return utils::expected<void, grpc::Status>{};
    }
    Log() << "Failed to write " << name << ": " << status;
  }
  else {
    Log() << "Writing " << name << " cancelled";
  }
  co_return utils::unexpected(status);
}

async_grpc::Task<utils::expected<void, grpc::Status>> CharacterServiceGrpc::Del(std::string_view name)
{
  Log() << "Deleting " << name;
  std::unique_ptr<grpc::ClientContext> context;
  variable_service::DelRequest request;
  request.set_key(std::string(name));
  variable_service::DelResponse response;
  auto status = grpc::Status::CANCELLED;
  if (co_await m_client.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(variable_service::VariableService, Del), context, request, response, status))
  {
    if (status.ok()) {
      Log() << "Successfully deleted " << name;
      co_return utils::expected<void, grpc::Status>{};
    }
    Log() << "Failed to delete " << name << ": " << status;
  }
  else {
    Log() << "Deleting " << name << " cancelled";
  }
  co_return utils::unexpected(status);
}
