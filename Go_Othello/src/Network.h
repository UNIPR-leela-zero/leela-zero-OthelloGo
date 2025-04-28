#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
    #include "NetworkGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
    #include "NetworkOthello.h"
#else
    #error "Unsupported game selected"
#endif

#endif // NETWORK_H_INCLUDED
