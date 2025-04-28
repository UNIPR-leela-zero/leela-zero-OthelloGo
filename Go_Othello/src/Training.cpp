#include "config.h"

// Training.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "TrainingGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "TrainingOthello.cpp"
#else
#error "Unsupported game selected"
#endif