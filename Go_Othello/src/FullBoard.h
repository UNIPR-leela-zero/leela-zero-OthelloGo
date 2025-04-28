#ifndef FULLBOARD_H_INCLUDED
#define FULLBOARD_H_INCLUDED

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "FullBoardGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FullBoardOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif
