#ifndef OPENCLSCHEDULER_H_INCLUDED
#define OPENCLSCHEDULER_H_INCLUDED

#if CURRENT_GAME == GAME_GO
#include "OpenCLSchedulerGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "OpenCLSchedulerOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif
