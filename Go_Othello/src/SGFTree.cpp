#include "config.h"

// SGFTree.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "SGFTreeGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "SGFTreeOthello.cpp"
#else
#error "Unsupported game selected"
#endif