#include <iostream>
#include <ncurses.h>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <unordered_map>

using namespace std;

// Game constants
const unsigned int MIN_Y = 0;
const unsigned int MAX_Y = 24;
const unsigned int MIN_X = 0;
const unsigned int MAX_X = 80;
const unsigned int alienSpawnIntervalMs = 3000;
const unsigned int rocketPerformMovementIntervalMs = 100;
const unsigned int gameLoopIntervalMs = 1;
const unsigned int maxGameLoopIterations = 60000; // 60.000 * 10^(-3) = 60s

// Game set-once parameters (no concurrency)
unsigned int alienHeightDecrementationIntervalInMs;
unsigned int maxRockets;
unsigned int rocketRechargeIntervalInMs;

struct Rocket
{
  int x, y, angle;
};

struct Alien
{
  int x;
  int y;
  bool destroyed;
};

unsigned int globalDestroyedAliens = 0;
pthread_mutex_t destroyedAliensLock;

unsigned int globalSuccessfullAliens = 0;
pthread_mutex_t successfulAliensLock;

unordered_map<unsigned long, Rocket> globalRocketMap;
pthread_mutex_t rocketsMapLock;

unordered_map<unsigned long, Alien> globalAlienMap;
pthread_mutex_t alienMapLock;

enum Difficulty
{
  EASY = 1,
  MEDIUM = 2,
  HARD = 3
};

std::string difficultyStringMapper(Difficulty difficulty)
{
  switch (difficulty)
  {
  case Difficulty::EASY:
    return "easy";
  case Difficulty::MEDIUM:
    return "medium";
  case Difficulty::HARD:
    return "hard";
  default:
    return "nao ta existindo";
  }
}

void renderHud(int rockets, Difficulty difficulty, int currentGameLoopIteration)
{
  mvprintw(0, 0, "Foguetes: %d/%d |", rockets, maxRockets);
  mvprintw(0, 16, "Dificuldade: %s | ", difficultyStringMapper(difficulty).c_str());
  mvprintw(0, 40, "Kills: %d | ", globalDestroyedAliens);
  mvprintw(0, 50, "Falhas: %d | ", globalSuccessfullAliens);
  mvprintw(0, 63, "Tempo: %d/%d | ", currentGameLoopIteration / 1000, maxGameLoopIterations / 1000);

  mvprintw(23, 40, "|");
  mvprintw(24, 39, "[#]");
}

Difficulty parseGameDifficulty(int argc, char *argv[])
{
  Difficulty difficulty = Difficulty::EASY;
  if (argc > 1)
  {
    string arg = argv[1];
    if (arg == difficultyStringMapper(Difficulty::EASY))
    {
      difficulty = Difficulty::EASY;
    }
    else if (arg == difficultyStringMapper(Difficulty::MEDIUM))
    {
      difficulty = Difficulty::MEDIUM;
    }
    else if (arg == difficultyStringMapper(Difficulty::HARD))
    {
      difficulty = Difficulty::HARD;
    }
    else
    {
      cout << "Dificuldade passada [ " + arg + " ] inválida. Passe o parâmetro como easy, medium ou hard.";
      exit(1);
    }
  }

  return difficulty;
}

bool destroyAlienIfPossible(int x, int y)
{
  bool alienDestroyed = false;

  pthread_mutex_lock(&alienMapLock);

  for (const auto &pair : globalAlienMap)
  {
    if (pair.second.destroyed)
      continue;
    if (pair.second.x == x && pair.second.y == y)
    {
      unsigned int alienThreadId = pair.first;
      globalAlienMap[alienThreadId].destroyed = true;

      pthread_mutex_lock(&destroyedAliensLock);
      globalDestroyedAliens++;
      pthread_mutex_unlock(&destroyedAliensLock);
      break;
    }
  }

  pthread_mutex_unlock(&alienMapLock);

  return alienDestroyed;
}

void *rocketThreadFunc(void *arg)
{
  pthread_t thread = pthread_self();
  const unsigned long threadId = (unsigned long)thread;
  pthread_mutex_lock(&rocketsMapLock);
  globalRocketMap[threadId] = {40, 23, 135};
  pthread_mutex_unlock(&rocketsMapLock);

  bool valid = true;
  while (valid)
  {
    switch (globalRocketMap[threadId].angle)
    {
    case 0:
      globalRocketMap[threadId].x--;
      break;
    case 45:
      globalRocketMap[threadId].x--;
      globalRocketMap[threadId].y--;
      break;
    case 90:
      globalRocketMap[threadId].y--;
      break;
    case 135:
      globalRocketMap[threadId].x++;
      globalRocketMap[threadId].y--;
      break;
    case 180:
      globalRocketMap[threadId].x++;
      break;
    }

    bool alienDestroyed = destroyAlienIfPossible(globalRocketMap[threadId].x, globalRocketMap[threadId].y);
    if (alienDestroyed)
    {
      valid = false;
    }

    if (globalRocketMap[threadId].x > MAX_X)
      valid = false;
    if (globalRocketMap[threadId].x < MIN_X)
      valid = false;
    if (globalRocketMap[threadId].y > MAX_Y)
      valid = false;
    if (globalRocketMap[threadId].y < MIN_Y)
      valid = false;

    this_thread::sleep_for(chrono::milliseconds(rocketPerformMovementIntervalMs));
  }

  pthread_mutex_lock(&rocketsMapLock);
  globalRocketMap.erase(threadId);
  pthread_mutex_unlock(&rocketsMapLock);
  return NULL;
}

void *alienThreadFunc(void *arg)
{
  pthread_t thread = pthread_self();
  const unsigned long threadId = (unsigned long)thread;

  int random_x = rand() % 41;

  pthread_mutex_lock(&alienMapLock);
  globalAlienMap[threadId] = {random_x, MIN_Y};
  pthread_mutex_unlock(&alienMapLock);

  while (globalAlienMap[threadId].y < MAX_Y)
  {
    if (globalAlienMap[threadId].destroyed)
    {
      pthread_mutex_lock(&alienMapLock);
      globalAlienMap.erase(threadId);
      pthread_mutex_unlock(&alienMapLock);

      return NULL;
    }

    globalAlienMap[threadId].y++;

    this_thread::sleep_for(chrono::milliseconds(alienHeightDecrementationIntervalInMs));
  }

  pthread_mutex_lock(&alienMapLock);
  pthread_mutex_lock(&successfulAliensLock);

  globalSuccessfullAliens++;
  globalAlienMap.erase(threadId);

  pthread_mutex_unlock(&successfulAliensLock);
  pthread_mutex_unlock(&alienMapLock);
}

void adjustGameParameters(Difficulty difficulty)
{
  switch (difficulty)
  {
  case Difficulty::EASY:
    alienHeightDecrementationIntervalInMs = 1000;
    maxRockets = 10;
    rocketRechargeIntervalInMs = 2000;
    break;
  case Difficulty::MEDIUM:
    alienHeightDecrementationIntervalInMs = 500;
    maxRockets = 5;
    rocketRechargeIntervalInMs = 2000;
    break;
  case Difficulty::HARD:
    alienHeightDecrementationIntervalInMs = 250;
    maxRockets = 3;
    rocketRechargeIntervalInMs = 2000;
    break;
  }
}

void render(Difficulty difficulty, int gameLoopIteration)
{
  clear();
  renderHud(4, difficulty, gameLoopIteration);

  pthread_mutex_lock(&rocketsMapLock);
  for (const auto &pair : globalRocketMap)
  {
    mvprintw(pair.second.y, pair.second.x, "o");
  }
  pthread_mutex_unlock(&rocketsMapLock);

  pthread_mutex_lock(&alienMapLock);
  for (const auto &pair : globalAlienMap)
  {
    mvprintw(pair.second.y, pair.second.x, "A");
  }
  pthread_mutex_unlock(&alienMapLock);

  refresh();
}

void configureTuiOptions()
{
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  curs_set(1);
}

void initMutexes()
{
  pthread_mutex_init(&destroyedAliensLock, NULL);
  pthread_mutex_init(&successfulAliensLock, NULL);
  pthread_mutex_init(&rocketsMapLock, NULL);
  pthread_mutex_init(&alienMapLock, NULL);
}

int main(int argc, char *argv[])
{
  initMutexes();
  configureTuiOptions();

  Difficulty difficulty = parseGameDifficulty(argc, argv);
  adjustGameParameters(difficulty);

  // for (int i = 22; i > 0; i--)
  // {
  //   if (i != 22)
  //     mvprintw(i + 1, 40, " ");
  //   mvprintw(i, 40, "o");
  //   this_thread::sleep_for(chrono::seconds(1));
  //   refresh();
  // }

  pthread_t rocketThread;
  pthread_create(&rocketThread, NULL, rocketThreadFunc, NULL);
  pthread_detach(rocketThread);

  for (int i = 0; i < maxGameLoopIterations; i++)
  {
    if (i % alienSpawnIntervalMs == 0)
    {
      pthread_t alienThread;
      pthread_create(&alienThread, NULL, alienThreadFunc, NULL);
      pthread_detach(alienThread);
    }

    render(difficulty, i);
    this_thread::sleep_for(chrono::milliseconds(gameLoopIntervalMs));
  }

  endwin();
  return 0;
}