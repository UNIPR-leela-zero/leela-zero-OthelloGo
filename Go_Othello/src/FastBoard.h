#ifndef FASTBOARD_H_INCLUDED
#define FASTBOARD_H_INCLUDED

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "FastBoardGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FastBoardOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif