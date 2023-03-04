#include "character_service.hpp"
#include <async_grpc/client.hpp>
#include <protos/variable_service.grpc.pb.h>

class CharacterServiceGrpc final : public CharacterService {
public:
  CharacterServiceGrpc();

  virtual async_game::Task<Player> GetPlayer() override;
  virtual async_game::Task<> GiveXp(int64_t ammount) override;
  virtual async_game::Task<> LevelUp() override;
  virtual async_game::Task<> Reset() override;

private:
  async_grpc::Client<variable_service::VariableService> m_client;
  async_grpc::ClientExecutorThread m_executor;

  async_grpc::Task<utils::expected<std::optional<int64_t>, grpc::Status>> Read(std::string_view name);
  async_grpc::Task<utils::expected<void, grpc::Status>> Write(std::string_view name, int64_t value);
  async_grpc::Task<utils::expected<void, grpc::Status>> Del(std::string_view name);
};