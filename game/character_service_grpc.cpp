#include "character_service_grpc.hpp"
#include "grpc_game.hpp"
#include <sstream>

Error GrpcStatusToError(const grpc::Status& status) {
  std::ostringstream oss;
  oss << status;
  return Error{ std::move(oss).str() };
}

CharacterServiceGrpc::CharacterServiceGrpc()
  : m_client(grpc::CreateChannel("[::1]:4213", grpc::InsecureChannelCredentials()))
{}

Result<std::vector<Player>> CharacterServiceGrpc::GetAllPlayers()
{
  auto var = co_await GetVariable("all_players");
  if (!var) {
    co_return utils::unexpected(GrpcStatusToError(var.error()));
  }
  std::vector<Player> players;
  if (*var) {
    for (int64_t i = 0; i < 64; ++i) {
      if (**var & (1ll << i)) {
        players.push_back(Player{ i, 1 });
      }
    }
  }
  co_return std::move(players);
}

Result<void> CharacterServiceGrpc::LevelupPlayer([[maybe_unused]] int64_t id)
{
  return Result<void>();
}

Result<Player> CharacterServiceGrpc::CreatePlayer()
{
  return Result<Player>();
}

Result<void> CharacterServiceGrpc::DeletePlayer()
{
  return Result<void>();
}

async_game::Task<utils::expected<std::optional<int64_t>, grpc::Status>> CharacterServiceGrpc::GetVariable(std::string_view name)
{
  std::unique_ptr<grpc::ClientContext> context;
  grpc::Status status;
  variable_service::ReadRequest request;
  request.set_key(std::string(name));
  variable_service::ReadResponse response;
  if (!co_await async_game::GrpcCoroutine(m_executor.GetExecutor(), m_client.AutoRetryUnary(ASYNC_GRPC_CLIENT_PREPARE_FUNC(variable_service::VariableService, Read), context, request, response, status))) {
    co_return utils::unexpected(grpc::Status::CANCELLED);
  }
  if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
    co_return std::nullopt;
  }
  if (!status.ok()) {
    co_return utils::unexpected(status);
  }
  co_return response.value();
}
