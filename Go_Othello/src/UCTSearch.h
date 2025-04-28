#ifndef UCTSEARCH_H_INCLUDED
#define UCTSEARCH_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTSearchGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTSearchOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif