#ifndef _RPC_H_
#define _RPC_H_

#include "mbuf.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//for offsetof
#ifdef __FreeBSD__
#include <sys/stddef.h>
#elif defined(linux)
#include <stddef.h>
#include <linux/stddef.h>
#elif defined(__APPLE__)
#include <stddef.h>
#elif defined(MS_WINDOWS)
#include <stdint.h>
#endif

#define RPC_INT (0)
#define RPC_INT8 (6)
#define RPC_INT16 (7)
#define RPC_STRING (1)
#define RPC_CLASS (2)
#define RPC_FLOAT (8)
#define RPC_DOUBLE (9)
#define RPC_INT64 (10)

#define RPC_ARRAY_VAR (-1) //Variable-length array
#define RPC_ARRAY_NOT   (-2)
#define RPC_CLASS_INDEX_NOT (-1)

#define RPC_PID_START (256) //protocol id start index

typedef uint32_t rpc_pto_id_t;
typedef int32_t rpc_int32_t;
typedef uint32_t rpc_size_t;
typedef int8_t rpc_int8_t;
typedef int16_t rpc_int16_t;
typedef int64_t rpc_int64_t;
typedef float rpc_float_t;
typedef double rpc_double_t;

#define RPC_SERVER_PREFIX "c2s_"
#define RPC_CLIENT_PREFIX "s2c_"

//check c implement args
#define RPC_C_CHECK_ARG(arg_cls) ((arg_cls)->fieldCount == 2 && (arg_cls)->fields[1].type == RPC_CLASS)
#define RPC_STRUCT_ARG(arg_cls) ((arg_cls)->fields[1].clsptr)

typedef void (*rpc_c_func_t)(int vfd, void *arg);

//rpc data type decl
typedef struct rpc_array_t
{
    rpc_size_t n;
    void *data;
} rpcArray;

//include '\0' at the end fo buf;
typedef struct rpc_string_t
{
    rpc_size_t n;
    char *buf;
} rpcString;

typedef struct rpc_field_t rpcField;

typedef struct rpc_class_t {
    char *name;
    int c_size;
    int c_impl;
    int fieldCount;
    rpcField *fields;
} rpcClass;

struct rpc_field_t {
    char *name;
    int type;
    int array;
    int clsIndex;
    int c_offset;
    rpcClass *clsptr;
    rpcClass *parent;
};

typedef struct rpc_function_t {
    char *module;
    unsigned flags;
    rpc_pto_id_t pto_id;
    rpcClass argClass;
    rpc_c_func_t c_func;
    void *data;
} rpcFunction;

typedef struct rpc_field_meta_t {
    const char *name;
    int c_offset;
} rpcFieldMeta;

typedef struct rpc_class_meta_t {
    const char *name;
    int c_size;
    int fieldCount;
    rpcFieldMeta *fieldMeta;
} rpcClassMeta;

typedef struct rpc_function_meta_t {
    const char *name;
    rpc_c_func_t c_func;
} rpcFunctionMeta;

typedef struct rpc_meta_t
{
    rpcClassMeta *classMetaList;
    rpcFunctionMeta *funcMetaList;
} rpcMeta;

typedef struct rpc_class_table_t {
    int elts;
    rpcClass *rpcClassList;
} rpcClassTable;

typedef struct rpc_function_table_t {
    int elts;
    rpcFunction *rpcFuncList;
    rpcFunction **sparseRpcFunc;
    unsigned int sparseElts;
} rpcFunctionTable;

typedef struct rpc_table_t {
    rpcFunctionTable *funcTable;
    rpcClassTable *classTable;
} rpcTable;

typedef struct rpc_object_t {
    rpcTable *table;
    rpcClassMeta *classMeta;
    rpcFunctionMeta *funcMeta;

    rpcMbuf *packBuf;
    size_t packBufInitSize;

    rpcMbuf *unpackBuf;
    size_t unpackBufInitSize;

    rpcInputBuf *inBuf;

    rpcFunction *func; //the func in pack or unpack status
    void *packArgs;
    void *unpackResult;

    void *luaState;

    //statistic data
    size_t cPackCnt;
    size_t cUnpackCnt;
    size_t scriptPackCnt;
    size_t scriptUnpackCnt;
} rpcObject;

#if defined(__cplusplus)
extern "C" {
#endif
int rpcTableCreate(rpcTable *table, const char *file, rpcClassMeta *classMetas, rpcFunctionMeta *funcMetas);
void rpcTableFree(rpcTable *table);

rpcFunction *findFunctionByPid(rpcFunctionTable *table, rpc_pto_id_t pid);
rpcFunction *findFunctionByName(rpcFunctionTable *table, const char *name);

int getFieldSize(rpcField *field);
const char *getTypeName(int type);

rpcObject *initRpcObjectFromBuf(rpcObject *obj, const char *buf, size_t size, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas);
rpcObject *rpcObjectInit(rpcObject *obj, const char *cfg, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas);
int rpcObjectFree(rpcObject *obj);

#if defined(__cplusplus)
}
#endif


#endif