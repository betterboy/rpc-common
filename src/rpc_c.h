#ifndef _RPC_C_H_
#define _RPC_C_H_

#include "rpc.h"

#if defined(__cplusplus)
extern "C" {
#endif

static int packFromC(rpcObject *obj, rpc_pto_id_t pid, void *argcls);

static size_t unpackToC(rpcObject *obj, const char *input, size_t len, rpc_pto_id_t pid, void **retcls);

static void *scriptUnpackToC(rpcObject *obj);

int bindCFunction(rpcObject *obj, const char *name, rpcFunction *cfunc);


#if defined(__cplusplus)
}
#endif

#endif //_RPC_C_H_