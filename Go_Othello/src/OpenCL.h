#ifndef OPENCL_H_INCLUDED
#define OPENCL_H_INCLUDED

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "OpenCLGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "OpenCLOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif
