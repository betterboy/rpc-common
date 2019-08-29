#include "src/rpc.h"
#include "rpc_meta.h"

static rpcFieldMeta position_t_fields[] = {
    {"x", offsetof(struct position_t, x), },
    {"y", offsetof(struct position_t, y), },
};

static rpcFieldMeta move_t_fields[] = {
    {"speed", offsetof(struct move_t, speed),},
    {"path", offsetof(struct move_t, path),},
};

rpcClassMeta auto_rpc_class_metas[] = {
    {"position_t", sizeof(struct position_t), 2, position_t_fields, },
    {"move_t", sizeof(struct move_t), 2, move_t_fields, },
};

extern void rpc_server_move(int, struct move_t *);
rpcFunctionMeta auto_rpc_function_metas[] = {
    {"rpc_server_move", (rpc_c_func_t)rpc_server_move,},
    {NULL, NULL,},
};


