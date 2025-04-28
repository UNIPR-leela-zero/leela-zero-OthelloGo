#include "config.h"

// Seleziona il file sorgente appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTSearchGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTSearchOthello.cpp"
#else
#error "Unsupported game selected"
#endif