#include "config.h"

// Seleziona il file sorgente appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "ZobristGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "ZobristOthello.cpp"
#else
#error "Unsupported game selected"
#endif