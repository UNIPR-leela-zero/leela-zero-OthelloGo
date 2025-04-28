#ifndef GTP_H_INCLUDED
#define GTP_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "GTPGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "GTPOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif // GTP_H_INCLUDED
