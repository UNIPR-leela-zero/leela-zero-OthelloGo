#include "config.h"

// TimeControl.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "TimeControlGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "TimeControlOthello.cpp"
#else
#error "Unsupported game selected"
#endif