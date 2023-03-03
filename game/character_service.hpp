#pragma once

#include "async_game.hpp"
#include <utils/expected.hpp>

struct Error {
  std::string message;
};

struct Player {
  int64_t id;
  int64_t level;
};

template<typename T>
using Result = async_game::Task<utils::expected<T, Error>>;

struct CharacterService {

  virtual ~CharacterService() = default;

  virtual Result<std::vector<Player>> GetAllPlayers() = 0;
  virtual Result<void> LevelupPlayer(int64_t id) = 0;
  virtual Result<Player> CreatePlayer() = 0;
  virtual Result<void> DeletePlayer() = 0;
};