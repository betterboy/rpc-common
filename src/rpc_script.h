#ifndef _RPC_SCRIPT_H_
#define _RPC_SCRIPT_H

#include "rpc.h"

#if defined(_cplusplus)
extern "C" {
#endif

void *c_func_implement(rpcObject *obj, rpc_size_t *len, rpc_pto_id_t *pid);

int pack(rpcObject *obj);
int unpack(rpcObject *obj);

#if defined(_cplusplus)
}
#endif

#endif