#include "character_service_memory.hpp"

async_game::Task<Player> CharacterServiceMemory::GetPlayer()
{
  co_return m_player;
}

async_game::Task<int64_t> CharacterServiceMemory::GiveXp(int64_t ammount)
{
  co_return m_player.xp += ammount;
}

async_game::Task<int64_t> CharacterServiceMemory::LevelUp()
{
  m_player.xp = 0;
  co_return ++m_player.level;
}

async_game::Task<> CharacterServiceMemory::Reset()
{
  m_player = Player{};
  co_return;
}
