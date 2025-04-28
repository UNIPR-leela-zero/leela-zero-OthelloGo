// Wrapper per GameState.cpp
#if CURRENT_GAME == GAME_GO
#include "GameStateGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "GameStateOthello.cpp"
#else
#error "Unsupported game selected"
#endif


