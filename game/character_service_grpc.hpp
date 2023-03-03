#include "character_service.hpp"
#include <async_grpc/client.hpp>
#include <protos/variable_service.grpc.pb.h>

class CharacterServiceGrpc final : public CharacterService {
public:
  CharacterServiceGrpc();

  virtual Result<std::vector<Player>> GetAllPlayers() override;
  virtual Result<void> LevelupPlayer(int64_t id) override;
  virtual Result<Player> CreatePlayer() override;
  virtual Result<void> DeletePlayer() override;

private:
  async_grpc::Client<variable_service::VariableService> m_client;
  async_grpc::ClientExecutorThread m_executor;

  async_game::Task<utils::expected<std::optional<int64_t>, grpc::Status>> GetVariable(std::string_view name);
};