#include "config.h"

// CPUPipe.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "CPUPipeGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "CPUPipeOthello.cpp"
#else
#error "Unsupported game selected"
#endif
