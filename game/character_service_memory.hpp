#pragma once

#include "character_service.hpp"

class CharacterServiceMemory final : public CharacterService {
public:
  virtual async_game::Task<Player> GetPlayer() override;
  virtual async_game::Task<int64_t> GiveXp(int64_t ammount) override;
  virtual async_game::Task<int64_t> LevelUp() override;
  virtual async_game::Task<> Reset() override;

private:
  Player m_player;
};