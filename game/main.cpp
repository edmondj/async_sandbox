#include <memory>
#include <iostream>
#include <string_view>
#include <utils/logs.hpp>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include "async_game.hpp"
#include "character_service_grpc.hpp"
#include "character_service_memory.hpp"

using namespace std::literals::string_view_literals;

static auto Log() {
  return utils::Log() << "[Game] ";
}

using Elapsed = std::chrono::nanoseconds;

struct Entity {
public:
  virtual ~Entity() = default;

  void PreUpdate() {
    m_executor.Update();
  }

  virtual void ProcessInput([[maybe_unused]] std::string_view input) {}

  virtual void Update([[maybe_unused]] Elapsed elapsed) {}

protected:
  async_game::Executor m_executor;
};

class Character : public Entity {
public:
  struct Dependencies {
    CharacterService& characterService;
  };

  explicit Character(Dependencies dependencies)
    : m_dependencies(std::move(dependencies))
  {}

  virtual void ProcessInput(std::string_view input) override {
    if (m_state == State::Idle && input == "fight") {
      Log() << "Let's fight!";
      m_state = State::Fighting;
      m_timeSinceXp = Elapsed(0);
    } else if (m_state == State::Fighting && input == "rest") {
      Log() << "Time for a break";
      m_state = State::Idle;
    } else if (m_state == State::Idle && input == "reincarnate") {
      async_lib::Spawn(m_executor, Reincarnate());
    }
  }

  virtual void Update(Elapsed elapsed) override {
    switch (m_state) {
    case State::Uninit: {
      m_state = State::Initializing;
      async_lib::Spawn(m_executor, Init());
      break;
    }
    case State::Fighting: {
      m_timeSinceXp += elapsed;
      if (m_timeSinceXp >= std::chrono::seconds(1)) {
        m_timeSinceXp -= std::chrono::seconds(1);
        async_lib::Spawn(m_executor, EarnXp());
      }
      break;
    }
    default:
      break;
    }
  }

private:
  async_game::Task<> Init() {
    Player player = co_await m_dependencies.characterService.GetPlayer();
    Log() << "You are level " << player.level << " with " << player.xp << "xp";
    m_state = State::Idle;
  }

  async_game::Task<> EarnXp() {
    auto log = Log() << "You earned 100xp! ";
    int64_t newXp = co_await m_dependencies.characterService.GiveXp(100);
    if (newXp >= 1000) {
      int64_t newLevel = co_await m_dependencies.characterService.LevelUp();
      log << "You leveled up! You are now level " << newLevel;
    } else {
      log << "You now have " << newXp << " experience points";
    }
  }

  async_game::Task<> Reincarnate() {
    Log() << "You forgo your previous life...";
    co_await m_dependencies.characterService.Reset();
    Log() << "It is done, you are now a blank slate, ready for the upcoming challenges";
  }

  Dependencies m_dependencies;
  enum class State {
    Uninit,
    Initializing,
    Idle,
    Fighting
  };
  State m_state = State::Uninit;
  Elapsed m_timeSinceXp = Elapsed(0);
};

class InputHandler {
public:
  InputHandler()
    : m_thread(std::bind_front(&InputHandler::LaunchThread, this))
  {
    while (!m_ready)
      ;
  }

  ~InputHandler() {
    m_thread.request_stop();
    m_cv.notify_one();
  }

  std::future<std::string> GetString() {
    m_promise = std::promise<std::string>();
    m_cv.notify_one();
    return m_promise.get_future();
  }

private:
  void LaunchThread(std::stop_token stop) {
    auto lock = std::unique_lock(m_mutex);
    while (!stop.stop_requested()) {
      m_ready = true;
      m_cv.wait(lock);
      if (!stop.stop_requested()) {
        std::string input;
        std::getline(std::cin, input);
        m_promise.set_value(std::move(input));
      }
    }
  }


  std::mutex m_mutex;
  std::atomic<bool> m_ready{ false };
  std::promise<std::string> m_promise;
  std::condition_variable m_cv;
  std::jthread m_thread;
};

class Game {
public:
  using clock = std::chrono::steady_clock;

  struct Dependencies {
    std::unique_ptr<CharacterService> characterService;
  };

  explicit Game(Dependencies&& dependencies)
    : m_dependencies(std::move(dependencies))
    , m_lastTick(clock::now())
  {
    m_entities.push_back(std::make_unique<Character>(Character::Dependencies{
      .characterService = *m_dependencies.characterService
      }
    ));
    m_pendingInput = m_inputHandler.GetString();
  }

  bool IsDone() { return m_done; }

  void Update() {
    if (m_pendingInput.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready) {
      std::string input = m_pendingInput.get();
      if (input == "quit") {
        m_done = true;
        return;
      }
      for (auto& entityPtr : m_entities) {
        entityPtr->ProcessInput(input);
      }
      m_pendingInput = m_inputHandler.GetString();
    }
    for (auto& entityPtr : m_entities) {
      entityPtr->PreUpdate();
    }
    auto now = clock::now();
    auto elapsed = now - m_lastTick;
    for (auto& entityPtr : m_entities) {
      entityPtr->Update(now - m_lastTick);
    }
    m_lastTick = now;

    std::this_thread::sleep_for(std::chrono::milliseconds(100) - elapsed);
  }

private:
  Dependencies m_dependencies;
  InputHandler m_inputHandler;
  std::future<std::string> m_pendingInput;
  std::vector<std::unique_ptr<Entity>> m_entities;
  bool m_done = false;
  clock::time_point m_lastTick;
};

std::unique_ptr<CharacterService> MakeCharacterService(std::string_view type) {
  if (type == "memory") {
    return std::make_unique<CharacterServiceMemory>();
  }
  else if (type == "grpc") {
    return std::make_unique<CharacterServiceGrpc>();
  }
  return nullptr;
}

int main(int ac, char** av) {
  if (ac != 2 || (av[1] != "memory"sv && av[1] != "grpc"sv)) {
    std::cout << "Usage: " << av[0] << " memory|grpc" << std::endl;
    return 1;
  }
  auto game = Game(Game::Dependencies{
    .characterService = MakeCharacterService(av[1])
    }
  );

  while (!game.IsDone()) {
    game.Update();
  }
}
