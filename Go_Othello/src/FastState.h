#ifndef FASTSTATE_H_INCLUDED
#define FASTSTATE_H_INCLUDED

#if CURRENT_GAME == GAME_GO
#include "FastStateGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "FastStateOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif