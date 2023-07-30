#include "character_service.hpp"
#include <async_grpc/client.hpp>
#include <protos/variable_service.grpc.pb.h>

class CharacterServiceGrpc final : public CharacterService {
public:
  struct Dependencies {
    async_grpc::CompletionQueueExecutor& executor;
  };

  explicit CharacterServiceGrpc(Dependencies deps);

  virtual async_game::Task<Player> GetPlayer() override;
  virtual async_game::Task<int64_t> GiveXp(int64_t ammount) override;
  virtual async_game::Task<int64_t> LevelUp() override;
  virtual async_game::Task<> Reset() override;

private:
  Dependencies m_dependencies;
  async_grpc::Client<variable_service::VariableService> m_client;

  async_grpc::Task<utils::expected<std::optional<int64_t>, grpc::Status>> Read(std::string_view name);
  async_grpc::Task<utils::expected<void, grpc::Status>> Write(std::string_view name, int64_t value);
  async_grpc::Task<utils::expected<void, grpc::Status>> Del(std::string_view name);
};