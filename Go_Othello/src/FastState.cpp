#include "config.h"

// FastState.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "FastStateGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FastStateOthello.cpp"
#else
#error "Unsupported game selected"
#endif
