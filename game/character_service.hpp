#pragma once

#include "async_game.hpp"
#include <utils/expected.hpp>

struct Player {
  int64_t level = 1;
  int64_t xp = 0;
};

struct CharacterService {

  virtual ~CharacterService() = default;

  // If player doesn't exist, creates it at level 1 with 0 xp
  virtual async_game::Task<Player> GetPlayer() = 0;
  virtual async_game::Task<> GiveXp(int64_t ammount) = 0;

  // Will reset the ammount of xp and increase the level
  virtual async_game::Task<> LevelUp() = 0;

  // Will set the player back as a level 1 with 0 xp
  virtual async_game::Task<> Reset() = 0;
};