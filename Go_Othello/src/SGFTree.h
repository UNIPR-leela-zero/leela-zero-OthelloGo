#ifndef SGFTREE_H_INCLUDED
#define SGFTREE_H_INCLUDED

#include "config.h"

// SGFTree.h (wrapper)
#if CURRENT_GAME == GAME_GO
#include "SGFTreeGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
#include "SGFTreeOthello.h"
#else
#error "Unsupported game selected"
#endif

#endif  // SGFTREE_H_INCLUDED