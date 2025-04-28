#include "config.h"

// Seleziona il file sorgente appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTNodePointerGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTNodePointerOthello.cpp"
#else
#error "Unsupported game selected"
#endif