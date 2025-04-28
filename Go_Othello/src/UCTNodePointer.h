#ifndef UCTNODEPOINTER_H_INCLUDED
#define UCTNODEPOINTER_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTNodePointerGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTNodePointerOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif