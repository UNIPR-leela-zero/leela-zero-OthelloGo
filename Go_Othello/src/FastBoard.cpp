#include "config.h"

// FastBoard.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "FastBoardGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FastBoardOthello.cpp"
#else
#error "Unsupported game selected"
#endif
