// Wrapper per GameState.h
#if CURRENT_GAME == GAME_GO
#include "GameStateGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "GameStateOthello.h"
#else
#error "Unsupported game selected"
#endif