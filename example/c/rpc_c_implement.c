#include <stdio.h>

#include "src/rpc.h"
#include "rpc_meta.h"

void rpc_server_move(int vfd, move_t *m)
{
    printf("%s:%d in c rpc_server_move, speed: %d\npaths:", __FILE__, __LINE__, m->speed);

    unsigned int i;
    rpcArray *array = &(m->path);
    position_t *path = (position_t*) array->data;
    for (i = 0; i < array->n; ++i) {
        printf("Test c rpc, postion=(%d, %d)\n", path[i].x, path[i].y);
    }
    printf("\n");
}