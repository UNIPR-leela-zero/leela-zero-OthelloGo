#include "config.h"

// Seleziona il file sorgente appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTNodeRootGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTNodeRootOthello.cpp"
#else
#error "Unsupported game selected"
#endif