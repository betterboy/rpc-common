#if defined(RPC_PYTHON)
#include "rpc_script_py.h"
#elif defined(RPC_LUA)
#include "rpc_script_lua.h"
#endif

#include "rpc_script.h"
#include "mbuf.h"
#include "rpc_c.h"

static int scriptPackTypeClass(rpcObject *obj, rpcField *field, int fieldCount, void *args, rpcMbuf *mbuf);

static int scriptPackField(rpcObject *obj, rpcField *field, void *value, rpcMbuf *mbuf)
{
    switch (field->type)
    {
    case RPC_INT:
        if (!SCRIPT_INT_CHECK(obj, value)) {
            goto error;
        }
        rpc_int32_t v32 = (rpc_int32_t)SCRIPT_TO_INT(obj, value);
        rpcMbufEnqType(mbuf, &v32, rpc_int32_t);
        break;
    case RPC_INT8:
        if (!SCRIPT_INT_CHECK(obj, value)) {
            goto error;
        }
        rpc_int8_t v8 = (rpc_int8_t)SCRIPT_TO_INT(obj, value);
        rpcMbufEnqType(mbuf, &v8, rpc_int8_t);
        break;
    case RPC_INT16:
        if (!SCRIPT_INT_CHECK(obj, value)) {
            goto error;
        }
        rpc_int16_t v16 = (rpc_int16_t)SCRIPT_TO_INT(obj, value);
        rpcMbufEnqType(mbuf, &v16, rpc_int16_t);
        break;
    case RPC_INT64:
        if (!SCRIPT_INT64_CHECK(obj, value)) goto error;
        rpc_int64_t v64 = (rpc_int64_t)SCRIPT_TO_INT64(obj, value);
        rpcMbufEnqType(mbuf, &v64, rpc_int64_t);
        break;
    case RPC_FLOAT:
        if (!SCRIPT_FLOAT_CHECK(obj, value)) goto error;
        rpc_float_t f = (rpc_float_t)SCRIPT_TO_FLOAT(obj, value);
        rpcMbufEnqType(mbuf, &f, rpc_float_t);
        break;
    case RPC_DOUBLE:
        if (!SCRIPT_DOUBLE_CHECK(obj, value)) goto error;
        rpc_double_t d = (rpc_double_t)SCRIPT_TO_DOUBLE(obj, value);
        rpcMbufEnqType(mbuf, &d, rpc_double_t);
        break;
    case RPC_STRING:
        if (!SCRIPT_STRING_CHECK(obj, value)) goto error;
        RPC_APPEND_STRING(mbuf, obj, value);
        break;
    case RPC_CLASS:
        if (!SCRIPT_DICT_CHECK(obj, value)) goto error;
        rpcClass *cls = field->clsptr;
        if (scriptPackTypeClass(obj, cls->fields, cls->fieldCount, value, mbuf)) {
            return -1;
        }
        break;
    
    default:
        break;
    }

    return 0;

error:
    fprintf(stderr, "ERROR: field type mismatch %s->%s, type=%d\n", field->parent->name, field->name, field->type);
    return -1;
}

static int scriptPackArray(rpcObject *obj, rpcField *field, void *list, rpcMbuf *mbuf)
{
    rpc_size_t i;
    if (!SCRIPT_LIST_CHECK(obj, list)) {
        fprintf(stderr, "ERROR: pack array %s->%s, except:%s, given:%s\n", field->parent->name, field->name, "array", getScriptObjectType(obj, list));
        return -1;
    }

    rpc_size_t len = (rpc_size_t)SCRIPT_LIST_LENGTH(obj, list);
    rpcMbufEnqType(mbuf, &len, rpc_size_t);
    for (i = 0; i < len; ++i) {
        void *value = SCRIPT_LIST_GET_ITEM(obj, list, i);
        int ret = scriptPackField(obj, field, value, mbuf);
        if (ret) return -1;
    }

    SCRIPT_AFTER_PACK_ARRAY(obj, list);
    return 0;
}

static int scriptPackTypeClass(rpcObject *obj, rpcField *field, int fieldCount, void *args, rpcMbuf *mbuf)
{
    if (!SCRIPT_DICT_CHECK(obj, args)) {
        fprintf(stderr, "ERROR: pack class %s->%s, except:%s, given:%s\n", field->parent->name, field->name, "class", getScriptObjectType(obj, args));
        return -1;
    }

    int i, ret;
    void *value;

    for (i = 0; i < fieldCount; ++i, ++field) {
        value = SCRIPT_DICT_GET_BY_STRING(obj, args, field->name);
        if (value == NULL) {
            fprintf(stderr, "ERROR: pack class missing key %s->%s\n", field->parent->name, field->name);
            return -1;
        }

        if (field->array == RPC_ARRAY_VAR) {
            ret = scriptPackArray(obj, field, value, mbuf);
        } else {
            ret = scriptPackField(obj, field, value, mbuf);
        }

        if (ret) return -1;
    }

    SCRIPT_AFTER_PACK_CLASS(obj, args);
    return 0;
}

int scriptPack(rpcObject *obj)
{
    int i;
    int ret;

    rpcFunction *func = obj->func;
    rpcClass *cls = &func->argClass;
    rpcField *field = cls->fields + 1;
    int fieldCount = cls->fieldCount - 1;

    rpcMbuf *mbuf = obj->packBuf;
    rpcMbufReset(mbuf, obj->packBufInitSize);
    rpcMbufEnqType(mbuf, &func->pto_id, rpc_pto_id_t);

    void *tuple = obj->packArgs;
    void *value;

    if (fieldCount != SCRIPT_TUPLE_LENGTH(obj, tuple)) {
        fprintf(stderr, "ERROR: pack argument count dismatch. func=%s,expect=%d,given=%d\n", cls->name, fieldCount, (int)SCRIPT_TUPLE_LENGTH(obj, tuple));
        return -1;
    }

    for (i = 0; i < fieldCount; ++i, ++field) {
        value = SCRIPT_TUPLE_GET_ITEM(obj, tuple, i);
        if (field->array == RPC_ARRAY_VAR) {
            ret = scriptPackArray(obj, field, value, mbuf);
        } else {
            ret = scriptPackField(obj, field, value, mbuf);
        }

        if (ret) return -1;
    }

    obj->scriptPackCnt++;
    tuple = NULL;
    return 0;
}

static int scriptUnpackTypeClass(rpcObject *obj, rpcField *field, int fieldCount, rpcInputBuf *inbuf, void **result);

static int scriptUnpackField(rpcObject *obj, rpcField *field, rpcInputBuf *inbuf, void **result)
{
    void *node;

    switch (field->type) {
    case RPC_INT: {
        rpc_int32_t n;
        rpcInputBufGetType(inbuf, &n, rpc_int32_t);
        node = SCRIPT_CREATE_INT(obj, n);
        break;
    }
    case RPC_INT8: {
        rpc_int8_t n;
        rpcInputBufGetType(inbuf, &n, rpc_int8_t);
        node = SCRIPT_CREATE_INT(obj, n);
        break;
    }
    case RPC_INT16: {
        rpc_int16_t n;
        rpcInputBufGetType(inbuf, &n, rpc_int16_t);
        node = SCRIPT_CREATE_INT(obj, n);
        break;
    }
    case RPC_INT64: {
        rpc_int64_t n;
        rpcInputBufGetType(inbuf, &n, rpc_int64_t);
        node = SCRIPT_CREATE_INT64(obj, n);
        break;
    }
    case RPC_STRING: {
        rpc_size_t len;
        rpcInputBufGetType(inbuf, &len, rpc_size_t);
        rpc_size_t buflen = rpcInputBufLen(inbuf);
        if (len <= 0) {
            node = SCRIPT_CREATE_STRING(obj, "");
        } else if (buflen < len) {
            fprintf(stderr, "ERROR: unpack string lengh error. expect=%u,given=%d\n", len, (int)rpcInputBufLen(inbuf));
            return -1;
        } else {
            node = SCRIPT_CREATE_STRING_SIZE(obj, (const char*)rpcInputBufPos((inbuf)), len);
            rpcInputBufAdvance(inbuf, len);
        }
        break;
    }
    case RPC_CLASS: {
        rpcClass *cls = field->clsptr;
        if (scriptUnpackTypeClass(obj, cls->fields, cls->fieldCount, inbuf, &node)) return -1;
        break;
    }
    
    default:
        return -1;
    }

    *result = node;
    return 0;
}

static int scriptUnpackArray(rpcObject *obj, rpcField *field, rpcInputBuf *inbuf, void **result)
{
    rpc_size_t i;
    rpc_size_t len;
    rpcInputBufGetType(inbuf, &len, rpc_size_t);
    void *list = SCRIPT_CREATE_LIST(obj, len);
    if (len > 0) {
        for (i = 0; i < len; ++i) {
            void *node;
            if (scriptUnpackField(obj, field, inbuf, (void**)&node)) goto error;
            SCRIPT_LIST_SET_ITEM(obj, list, i, node);
        }
    }

    *result = &list;
    return 0;
error:
    SCRIPT_DECREF(obj, list);
    return -1;
}

static int scriptUnpackTypeClass(rpcObject *obj, rpcField *field, int fieldCount, rpcInputBuf *inbuf, void **result)
{
    int i;
    void *dict = SCRIPT_CREATE_DICT(obj, fieldCount);
    for (i = 0; i < fieldCount; ++i, ++field) {
        void *node;
        if (field->array == RPC_ARRAY_VAR) {
            if (scriptUnpackArray(obj, field, inbuf, &node)) goto error;
        } else {
            if (scriptUnpackField(obj, field, inbuf, &node)) goto error;
        }
        SCRIPT_DICT_SET_ITEM_STRING(obj, dict, field->name, node);
        SCRIPT_DECREF(obj, node);
    }

    *result = dict;
    return 0;

error:
    SCRIPT_DECREF(obj, dict);
    return -1;
}

int scriptUnpack(rpcObject *obj)
{
    rpcInputBuf *inbuf = obj->inBuf;
    rpcFunction *func = obj->func;
    rpcClass *cls = &func->argClass;

    rpcField *field = cls->fields + 1;
    int fieldCount = cls->fieldCount - 1;

    int i;
    void *tuple = SCRIPT_CREATE_TUPLE(obj, fieldCount);
    obj->unpackResult = tuple;

    if (fieldCount == 0) return 0;

    for (i = 0; i < fieldCount; ++i, ++field) {
        void *node;
        if (field->array == RPC_ARRAY_VAR) {
            if (scriptUnpackArray(obj, field, inbuf, &node)) {
                goto error;
            }
        } else {
            if (scriptUnpackField(obj, field, inbuf, &node)) {
                goto error;
            }
        }

        SCRIPT_TUPLE_SET_ITEM(obj, tuple, i, node);
    }
    obj->scriptUnpackCnt++;
    return 0;

error:
    SCRIPT_DECREF(obj, tuple);
    return -1;
}

void *c_func_implement(rpcObject *obj, rpc_size_t *len, rpc_pto_id_t *pid)
{
    rpcFunction *func = obj->func;
    void *result = scriptUnpackToC(obj);
    func->c_func(0, result);
    rpcInputBuf *inbuf = obj->inBuf;
    *len = (rpc_size_t)inbuf->offset + sizeof(rpc_pto_id_t);
    *pid = func->pto_id;
    return result;
}