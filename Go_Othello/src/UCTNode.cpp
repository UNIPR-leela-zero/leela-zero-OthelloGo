#include "config.h"

// Seleziona il file sorgente appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTNodeGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTNodeOthello.cpp"
#else
#error "Unsupported game selected"
#endif