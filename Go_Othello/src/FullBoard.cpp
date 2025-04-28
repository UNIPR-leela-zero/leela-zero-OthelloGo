#include "config.h"

// FullBoard.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "FullBoardGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FullBoardOthello.cpp"
#else
#error "Unsupported game selected"
#endif