#include "config.h"

#if CURRENT_GAME == GAME_GO
#include "OpenCLSchedulerGo.cpp"
#elif CURRENT_GAME == GAME_OTHELLO
#include "OpenCLSchedulerOthello.cpp"
#else
#error "Unsupported game selected"
#endif
