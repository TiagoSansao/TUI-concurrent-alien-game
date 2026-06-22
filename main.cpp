#include <iostream>
#include <ncurses.h>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <unordered_map>

using namespace std;

const unsigned int MIN_Y = 0;
const unsigned int MAX_Y = 24;
const unsigned int MIN_X = 0;
const unsigned int MAX_X = 80;
const unsigned int maxGameLoopIterations = 600; // 0.1s por iteração do game looop, ent 1 min de jogo

struct Rocket
{
  int x, y, angle;
};

unsigned int destroyed_aliens = 0;
pthread_mutex_t destroyed_aliens_lock;

unsigned int successful_aliens = 0;
pthread_mutex_t successful_aliens_lock;

unordered_map<unsigned long, Rocket> globalRocketMap;
pthread_mutex_t rocketsMapLock;

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

void renderBaseElements(int maxRockets, int rockets, Difficulty difficulty)
{
  mvprintw(0, 0, "Foguetes: %d/%d", rockets, maxRockets);
  mvprintw(0, 20, "Dificuldade: %s", difficultyStringMapper(difficulty).c_str());

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

void *
rocketThreadFunc(void *arg)
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

    if (globalRocketMap[threadId].x > MAX_X)
      valid = false;
    if (globalRocketMap[threadId].x < MIN_X)
      valid = false;
    if (globalRocketMap[threadId].y > MAX_Y)
      valid = false;
    if (globalRocketMap[threadId].y < MIN_Y)
      valid = false;

    if (!valid)
    {
      return NULL;
    }

    this_thread::sleep_for(chrono::seconds(1));
  }
}

int main(int argc, char *argv[])
{
  Difficulty difficulty = parseGameDifficulty(argc, argv);
  unsigned int Krockets = 5;
  unsigned int rechargeTimeInSeconds = 1;

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  curs_set(1); // depois colocar 0 pra n aparecer o cursor no terminal

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
    clear();
    renderBaseElements(Krockets, 4, difficulty);

    pthread_mutex_lock(&rocketsMapLock);
    for (const auto &pair : globalRocketMap)
    {
      mvprintw(pair.second.y, pair.second.x, "o");
    }
    pthread_mutex_unlock(&rocketsMapLock);

    refresh();
    this_thread::sleep_for(chrono::milliseconds(100));
  }

  endwin();
  return 0;
}