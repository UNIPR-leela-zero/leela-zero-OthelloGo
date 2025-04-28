#ifndef TIMECONTROL_H_INCLUDED
#define TIMECONTROL_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "TimeControlGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "TimeControlOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif