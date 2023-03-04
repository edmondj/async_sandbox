#include "character_service_memory.hpp"

async_game::Task<Player> CharacterServiceMemory::GetPlayer()
{
  co_return m_player;
}

async_game::Task<> CharacterServiceMemory::GiveXp(int64_t ammount)
{
  m_player.xp += ammount;
  co_return;
}

async_game::Task<> CharacterServiceMemory::LevelUp()
{
  ++m_player.level;
  m_player.xp = 0;
  co_return;
}

async_game::Task<> CharacterServiceMemory::Reset()
{
  m_player = Player{};
  co_return;
}
