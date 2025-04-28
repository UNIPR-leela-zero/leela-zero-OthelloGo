#ifndef TRAINING_H_INCLUDED
#define TRAINING_H_INCLUDED

#include "config.h"

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
#include "TrainingGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "TrainingOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif