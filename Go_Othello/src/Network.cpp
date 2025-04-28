#include "config.h"

// Network.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
    #include "NetworkGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
    #include "NetworkOthello.cpp"
#else
    #error "Unsupported game selected"
#endif
