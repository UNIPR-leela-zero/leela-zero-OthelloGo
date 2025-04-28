#ifndef UCTNODE_H_INCLUDED
#define UCTNODE_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "UCTNodeGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "UCTNodeOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif