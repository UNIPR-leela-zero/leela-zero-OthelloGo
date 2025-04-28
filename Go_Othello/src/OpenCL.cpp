#include "config.h"

// OpenCL.cpp (wrapper)
#if CURRENT_GAME == GAME_GO
#include "OpenCLGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "OpenCLOthello.cpp"
#else
#error "Unsupported game selected"
#endif
