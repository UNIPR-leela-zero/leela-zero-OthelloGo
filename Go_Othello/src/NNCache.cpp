#include "config.h"

// NNCache.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
    #include "NNCacheGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
    #include "NNCacheOthello.cpp"
#else
    #error "Unsupported game selected"
#endif
