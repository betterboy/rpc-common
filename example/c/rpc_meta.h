#ifndef _RPC_META_H_
#define _RPC_META_H_

#include "src/rpc.h"

typedef struct position_t {
    int x;
    int y;
} position_t;

typedef struct move_t
{
    int speed;
    rpcArray path;
} move_t;

#endif