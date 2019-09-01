/*
Implement c rpc.
*/

#include "rpc_c.h"
#include "mbuf.h"

inline static int unpackTypeClass(rpcField *fields, int fieldCount, rpcMbuf *mbuf, char *unpackbuf, rpcInputBuf *inbuf);

inline static int unpackField(rpcField *field, rpcMbuf *mbuf, char *value, rpcInputBuf *inbuf)
{
    switch (field->type) {
    case RPC_INT: {
        rpc_int32_t *v = (rpc_int32_t*)value;
        rpcInputBufGetType(inbuf, v, rpc_int32_t);
        break;
    }
    case RPC_STRING: {
        rpcString *ptr = (rpcString*)value;
        rpcInputBufGetType(inbuf, &ptr->n, rpc_int32_t);
        if (ptr->n <= 0) {
            ptr->buf = NULL;
        } else {
            rpcInputBufCheck(inbuf, ptr->n);
            ptr->buf = (char*)rpcMbufAlloc(mbuf, (ptr->n + 1) * sizeof(*ptr->buf));
            rpcInputBufGetNoCheck(inbuf, ptr->buf, ptr->n);
            ptr->buf[ptr->n] = '\0';
        }
        break;
    }
    case RPC_CLASS: {
        rpcClass *cls = field->clsptr;
        if (unpackTypeClass(cls->fields, cls->fieldCount, mbuf, value, inbuf)) return -1;
        break;
    }

    default:
        return -1;
    }
    return 0;
}


inline static int unpackTypeClass(rpcField *field, int fieldCount, rpcMbuf *mbuf, char *clsValue, rpcInputBuf *inbuf)
{
    int i, j;
    char *fieldValue;
    for (i = 0; i < fieldCount; ++i, ++field) {
        fieldValue = clsValue + field->c_offset;
        if (field->array == RPC_ARRAY_VAR) {
            rpcArray *ptr = (rpcArray*)fieldValue;
            rpcInputBufGetType(inbuf, &ptr->n, rpc_size_t);
            if (ptr->n <= 0) {
                ptr->data = NULL;
            } else {
                char *data;
                rpcInputBufCheck(inbuf, ptr->n);
                size_t fsz = getFieldSize(field);
                ptr->data = rpcMbufAlloc(mbuf, ptr->n * fsz);
                for (j = 0, data = (char*)ptr->data; j < ptr->n; ++j, data += fsz) {
                    if (unpackField(field, mbuf, data, inbuf)) return -1;
                }
            }

        } else {
            if (unpackField(field, mbuf, fieldValue, inbuf)) return -1;
        }
    }
    return 0;
}

void *scriptUnpackToC(rpcObject *obj)
{
    rpcFunction *func = obj->func;
    rpcInputBuf *inbuf = obj->inBuf;
    rpcMbuf *mbuf = obj->unpackBuf;
    rpcMbufReset(mbuf, obj->unpackBufInitSize);

    rpcClass *argcls = RPC_STRUCT_ARG(&func->argClass);
    char *clsValue = (char*)rpcMbufAlloc(mbuf, argcls->c_size);
    if (unpackTypeClass(argcls->fields, argcls->fieldCount, mbuf, clsValue, inbuf)) return NULL;

    obj->cPackCnt++;
    return clsValue;
}

//pack struct
inline static int packTypeClass(rpcField *field, int fieldCount, void *clsValue, rpcMbuf *mbuf);

inline static int packField(rpcField *field, void *value, rpcMbuf *mbuf)
{
    switch (field->type) {
    case RPC_INT: {
        rpc_int32_t *v = (rpc_int32_t*)value;
        rpcMbufEnqType(mbuf, v, rpc_int32_t);
        break;
    }
    case RPC_STRING: {
        rpcString *ptr = (rpcString*)value;
        rpcMbufEnqType(mbuf, &ptr->n, rpc_size_t);
        rpcMbufEnq(mbuf, ptr->buf, ptr->n);
        break;
    }
    case RPC_CLASS: {
        rpcClass *cls = field->clsptr;
        if (packTypeClass(cls->fields, cls->fieldCount, value, mbuf)) return -1;
        break;
    }
    default:
        assert(0);
        return -1;
    }

    return 0;
}

inline static int packTypeClass(rpcField *field, int fieldCount, void *clsValue, rpcMbuf *mbuf)
{
    int i, j;
    char *fieldValue;
    for (i = 0; i < fieldCount; ++i, ++field) {
        fieldValue = (char*)clsValue + field->c_offset;
        if (field->array == RPC_ARRAY_VAR) {
            size_t fsz;
            char *data;
            rpcArray *ptr = (rpcArray*)fieldValue;
            rpcMbufEnqType(mbuf, &ptr->n, rpc_size_t);
            fsz = getFieldSize(field);
            for (j = 0, data = (char*)ptr->data; j < ptr->n; ++j, data += fsz) {
                if (packField(field, data, mbuf)) return -1;
            }
        } else {
            if (packField(field, fieldValue, mbuf)) return -1;
        }
    }

    return 0;
}

int c_pack(rpcObject *obj)
{
    rpcFunction *func = obj->func;
    rpcClass *cls = &func->argClass;
    rpcMbuf *mbuf = obj->packBuf;
    rpcMbufReset(mbuf, obj->packBufInitSize);
    rpcMbufEnqType(mbuf, &func->pto_id, rpc_pto_id_t);

    //skip the first arg vfd
    int ret = packTypeClass(cls->fields + 1, cls->fieldCount - 1, obj->packArgs, mbuf);
    if (ret) {
        return ret;
    }

    obj->cPackCnt++;
    return ret;
}

int packFromC(rpcObject *obj, rpc_pto_id_t pid, void *arg)
{
    rpcFunction *func = findFunctionByPid(obj->table->funcTable, pid);

    if (func == NULL) return -1;

    obj->func = func;
    obj->packArgs = arg;
    int ret = c_pack(obj);

    return ret;
}

size_t unpackToC(rpcObject *obj, const char *input, size_t len, rpc_pto_id_t *pid, void **retcls)
{
    const char *pinbuf = input;
    if (len < sizeof(rpc_pto_id_t)) return 0;

    rpc_pto_id_t tmpid = *((rpc_pto_id_t*)input);
    rpcFunction *func = findFunctionByPid(obj->table->funcTable, tmpid);
    if (func == NULL) {
        fprintf(stderr, "pid:%d not exists\n", tmpid);
        return 0;
    }

    obj->func = func;
    rpcInputBuf *inbuf = obj->inBuf;
    pinbuf += sizeof(rpc_pto_id_t);
    rpcInputBufInit(inbuf, pinbuf, len - (pinbuf - input));


    void *arg = scriptUnpackToC(obj);
    *retcls = arg;
    *pid = tmpid;
     if (func->c_func != NULL) {
        func->c_func(0, arg);
     }

     size_t plen = (rpc_size_t)inbuf->offset + sizeof(rpc_pto_id_t);
     return plen;
}

int bindCFunction(rpcObject *obj, const char *name, rpc_c_func_t cfunc)
{
    rpcFunction *func = findFunctionByName(obj->table->funcTable, name);
    if (func == NULL) return -1;

    if (func->c_func != NULL) {
        printf("rebind cfunc:%s %p->%p\n", name, func->c_func, cfunc);
    }

    func->c_func = cfunc;
    return 0;
}
