#ifndef NNCACHE_H_INCLUDED
#define NNCACHE_H_INCLUDED

// Seleziona il file header appropriato in base al gioco corrente
#if CURRENT_GAME == GAME_GO
    #include "NNCacheGo.h"
#elif CURRENT_GAME == GAME_OTHELLO
    #include "NNCacheOthello.h"
#else
    #error "Unsupported game selected"
#endif

#endif // NNCACHE_H_INCLUDED
