/*
*/

#include "rpc.h"
#include "mbuf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#if _MSC_VER >= 1400
#pragma warning( disable: 4996) //disable warning about strdup being deprecated
#endif

static void rpcSetClassPtr(rpcTable *table)
{
    rpcClassTable *classTable = table->classTable;
    rpcFunctionTable *funcTable = table->funcTable;

    int i, j;
    for (i = 0; i < classTable->elts; ++i) {
        rpcClass *cls = &classTable->rpcClassList[i];
        for (j = 0; j < cls->fieldCount; ++j) {
            rpcField *field = &cls->fields[j];
            if (field->clsIndex >= 0) {
                field->clsptr = &classTable->rpcClassList[field->clsIndex];
            }
        }
    }

    for (i = 0; i < funcTable->elts; ++i) {
        rpcFunction *func = &funcTable->rpcFuncList[i];
        rpcClass *argcls = &func->argClass;
        for (j = 0; j < argcls->fieldCount; ++j) {
            rpcField *field = &argcls->fields[j];
            if (field->clsIndex >= 0) {
                field->clsptr = &classTable->rpcClassList[field->clsIndex];
            }
        }
    }
}

static void freeClass(rpcClass *cls)
{
    if (cls == NULL) return;
    int i;
    for (i = 0; i < cls->fieldCount; ++i) {
        rpcField *field = &cls->fields[i];
        free(field->name);
    }

    free(cls->fields);
    free(cls->name);
}

static void freeFunction(rpcFunction *func)
{
    if (func != NULL) {
        free(func->module);
        freeClass(&func->argClass);
    }
}

void freeRpcTable(rpcTable *table)
{
    rpcClassTable *clsTable = table->classTable;
    rpcFunctionTable *funcTable = table->funcTable;

    int i;
    if (clsTable != NULL) {
        for (i = 0; i < clsTable->elts; ++i) {
            rpcClass *cls = &clsTable->rpcClassList[i];
            freeClass(cls);
        }
        free(clsTable);
    }

    if (funcTable != NULL) {
        for (i = 0; i < funcTable->elts; ++i) {
            rpcFunction *func = &funcTable->rpcFuncList[i];
            freeFunction(func);
        }
        free(funcTable->sparseRpcFunc);
        free(funcTable);
    }

    memset(table, 0, sizeof(*table));
}

static rpcClassMeta *findClassMeta(rpcClassMeta *classMetas, const char *name)
{
    rpcClassMeta *meta = NULL;
    for (meta = classMetas; meta->name != NULL; ++meta) {
        if (0 == strcmp(meta->name, name)) return meta;
    }
    return NULL;
}

static rpcFunctionMeta *findFunctionMeta(rpcFunctionMeta *funcMetas, const char *name)
{
    rpcFunctionMeta *meta = NULL;
    for (meta = funcMetas; meta->name != NULL; ++meta) {
        if (0 == strcmp(meta->name, name)) return meta;
    }

    return NULL;
}

static void fillMeta(rpcTable *table, rpcClassMeta *classMetas, rpcFunctionMeta *funcMetas)
{
    int i, j;
    rpcClassMeta *clsmeta;
    rpcFunctionMeta *funcmeta;
    rpcFieldMeta *fieldmeta;
    rpcClass *cls;

    rpcFunctionTable *funcTable = table->funcTable;
    rpcClassTable *clsTable = table->classTable;

    if (clsTable != NULL) {
        for (i = 0; i < clsTable->elts; ++i) {
            cls = &clsTable->rpcClassList[i];
            if (!cls->c_impl) continue;

            clsmeta = findClassMeta(classMetas, cls->name);
            if (clsmeta == NULL) fprintf(stderr, "errer: can not find clsmeta for %s\n", cls->name);
            assert(clsmeta);
            printf("fill meta cls: %s,%d,%s,%d\n", cls->name, cls->c_size, clsmeta->name, clsmeta->c_size);
            cls->c_size = clsmeta->c_size;
            fieldmeta = clsmeta->fieldMeta;
            for (j = 0; j < clsmeta->fieldCount; ++j) {
                cls->fields[j].c_offset = fieldmeta[j].c_offset;
            }
        }
    }

    if (funcTable != NULL) {
        for (i = 0; i < funcTable->elts; ++i) {
            rpcFunction *func =  &funcTable->rpcFuncList[i];
            rpcClass *argcls = &func->argClass;
            if (!argcls->c_impl) continue;

            funcmeta = findFunctionMeta(funcMetas, argcls->name);
            if (funcmeta != NULL) func->c_func = funcmeta->c_func;
        }
    }
}

static void checkDuplicatedName(rpcTable *table)
{
    int i, j;
    unsigned c;
    rpcClassTable *clsTable = table->classTable;
    rpcFunctionTable *funcTable = table->funcTable;
    
    if (clsTable != NULL) {
        for (i = 0; i < clsTable->elts; ++i) {
            rpcClass *cls = &clsTable->rpcClassList[i];
            for (j = 0, c = 0; j < clsTable->elts; ++j) {
                rpcClass *tmp = &clsTable->rpcClassList[j];
                if (tmp != cls && !strcmp(cls->name, tmp->name)) ++c;
            }

            if (c > 0) {
                fprintf(stderr, "error! declaration duplicated class: %s\n", cls->name);
                assert(0);
            }
        }
    }

    if (funcTable != NULL) {
        for (i = 0; i < funcTable->elts; ++i) {
            rpcFunction *func = &funcTable->rpcFuncList[i];
            rpcClass *argcls = &func->argClass;
            for (j = 0, c = 0; j < funcTable->elts; ++j) {
                rpcClass *tmp = &funcTable->rpcFuncList[j].argClass;
                if (tmp != argcls && !strcmp(argcls->name, tmp->name)) ++c;
            }

            if (c > 0) {
                fprintf(stderr, "error! rpc declaration deplicated func: %s\n", argcls->name);
                assert(0);
            }
        }
    }
}

static void checkRpcTable(rpcTable *table, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas)
{
    checkDuplicatedName(table);

    int i, j;
    rpcClassMeta *clsmeta;
    rpcFunctionMeta *funcmeta;
    rpcFieldMeta *fieldmeta;
    rpcClass *cls;

    rpcFunctionTable *funcTable = table->funcTable;
    rpcClassTable *clsTable = table->classTable;

    if (clsTable != NULL) {
        for (i = 0; i < clsTable->elts; ++i) {
            cls = &clsTable->rpcClassList[i];
            if (!cls->c_impl) continue;

            clsmeta = findClassMeta(clsMetas, cls->name);
            assert(clsmeta);
            assert(0 == strcmp(clsmeta->name, cls->name));
            assert(clsmeta->fieldCount == cls->fieldCount);
            assert(clsmeta->c_size == cls->c_size);

            fieldmeta = clsmeta->fieldMeta;
            for (j = 0; j < cls->fieldCount; ++j) {
                assert( 0 == strcmp(cls->fields[j].name, fieldmeta[j].name));
                assert( cls->fields[j].c_offset == fieldmeta[j].c_offset);
            }
        }
    }

    if (funcTable != NULL) {
        for (i = 0; i < funcTable->elts; ++i) {
            rpcFunction *func = &funcTable->rpcFuncList[i];
            rpcClass *argcls = &func->argClass;
            if (!argcls->c_impl) continue;

            funcmeta = findFunctionMeta(funcMetas, argcls->name);
            if (funcmeta == NULL) continue;
            assert(func->c_func == funcmeta->c_func);
            printf("fieldcout=%d, field->type=%d\n", argcls->fieldCount, argcls->fields[1].type);
            assert(RPC_C_CHECK_ARG(argcls));
            cls = RPC_STRUCT_ARG(argcls);
            clsmeta = findClassMeta(clsMetas, cls->name);
            printf("argcls->name=%s\n", argcls->name);
            assert(clsmeta);
            assert(cls->c_size == clsmeta->c_size);
            assert(0 == strcmp(cls->name, clsmeta->name));
            assert(cls->fieldCount == clsmeta->fieldCount);

            fieldmeta = clsmeta->fieldMeta;
            for (j = 0; j < cls->fieldCount; ++j) {
                assert( 0 == strcmp(cls->fields[j].name, fieldmeta[j].name));
                assert(cls->fields[j].c_offset == fieldmeta[j].c_offset);
            }
        }
    }
}

static int sortFunction(const void *a, const void *b)
{
    return ((rpcFunction*)a)->pto_id - ((rpcFunction*)b)->pto_id;
}

static void rpcFunctionQsort(rpcFunctionTable *table)
{
    qsort(table->rpcFuncList, table->elts, sizeof(rpcFunction), sortFunction);
}

static void buildFunctionSparseIndex(rpcFunctionTable *table)
{
    rpcFunctionQsort(table);

    if (table->elts == 0) {
        table->sparseElts = 0;
        table->sparseRpcFunc = NULL;
    } else {
        rpcFunction *last = &table->rpcFuncList[table->elts - 1];
        table->sparseElts = last->pto_id + 1;
        table->sparseRpcFunc = (rpcFunction**) calloc (table->sparseElts, sizeof(rpcFunction));

        int i, j;
        for (i = 0; i < table->elts; ++i) {
            rpcFunction *func = &table->rpcFuncList[i];
            table->sparseRpcFunc[func->pto_id] = func;

            rpcClass *argcls = &func->argClass;
            for (j = 0; j < argcls->fieldCount; ++j) {
                argcls->fields[j].parent = argcls;
            }
        }
    }
}

int rpcTableCreateFromBuf(rpcTable *table, const char *fileBuf, size_t len, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas)
{

    char *line;
    int i, j;
    int tmp;
    rpcClassTable *clsTable;
    rpcFunctionTable *funcTable;
    char buf[2048];
    char buf2[2048];

    line = strtok((char*)fileBuf, "\n");
    sscanf(line, "class_table_num:%d", &tmp);
    clsTable = (rpcClassTable*)calloc(1, sizeof(rpcClassTable) + sizeof(rpcClass) * tmp);

    clsTable->elts = tmp;
    clsTable->rpcClassList = (rpcClass*)(clsTable + 1);

    for (i = 0; i < clsTable->elts; ++i) {
        rpcClass *cls = &clsTable->rpcClassList[i];
        memset(buf, 0, sizeof(buf));
        line = strtok(NULL, "\n");
        sscanf(line, "field_count:%d,c_imp:%d,class_name=%s", &cls->fieldCount, &cls->c_impl, buf);
        cls->name = strdup(buf);
        cls->fields = (rpcField*)calloc(cls->fieldCount, sizeof(rpcField));

        //fetch class fields
        for (j = 0; j < cls->fieldCount; ++j) {
            rpcField *field = &cls->fields[j];
            field->parent = cls;
            memset(buf, 0, sizeof(buf));
            line = strtok(NULL, "\n");
            sscanf(line, "field_type:%d,class_index:%d,array:%d,field_name:%s", 
                &field->type, &field->clsIndex, &field->array, buf);
                field->name = strdup(buf);
        }
    }

    line = strtok(NULL, "\n");
    sscanf(line, "function_table_num:%d", &tmp);
    funcTable = (rpcFunctionTable*)calloc(1, sizeof(rpcFunctionTable) + sizeof(rpcFunction) * tmp);
    funcTable->elts = tmp;
    funcTable->rpcFuncList = (rpcFunction*)(funcTable + 1);

    for (i = 0; i < funcTable->elts; ++i) {
        rpcFunction *func = &funcTable->rpcFuncList[i];
        rpcClass *argcls = &func->argClass;

        memset(buf, 0, sizeof(buf));
        memset(buf2, 0, sizeof(buf2));
        line = strtok(NULL, "\n");
        sscanf(line, "function_id:%d,c_imp:%d,arg_count:%d,module:%[^,]%*cfunction_name:%s", &tmp, &argcls->c_impl, &argcls->fieldCount, buf, buf2);
        func->pto_id = (rpc_pto_id_t)tmp;
        func->module = strdup(buf);
        func->data = NULL;
        argcls->name = strdup(buf2);
        argcls->fields = (rpcField*)calloc(argcls->fieldCount, sizeof(rpcField));

        for (j = 0; j < argcls->fieldCount; ++j) {
            rpcField *field = &argcls->fields[j];
            field->parent = argcls;
            memset(buf, 0, sizeof(buf));
            line = strtok(NULL, "\n");
            sscanf(line, "arg_type:%d,class_index:%d,array:%d,arg_name:%s", &field->type, &field->clsIndex, &field->array, buf);
            field->name = strdup(buf);
        }
    }


    table->funcTable = funcTable;
    table->classTable = clsTable;
    rpcSetClassPtr(table);
    fillMeta(table, clsMetas, funcMetas);
    checkRpcTable(table, clsMetas, funcMetas);
    buildFunctionSparseIndex(table->funcTable);
    return 0;
}

rpcFunction *findFunctionByName(rpcFunctionTable *table, const char *name)
{
    int i;
    if (name != NULL) {
        for (i = 0; i < table->elts; ++i) {
            rpcFunction *func = &table->rpcFuncList[i];
            if (0 == strcmp(func->argClass.name, name)) return func;
        }
    }

    return NULL;
}

rpcFunction *findFunctionByPid(rpcFunctionTable *table, rpc_pto_id_t funcId)
{
    if (funcId < table->sparseElts) {
        return table->sparseRpcFunc[funcId];
    }
    return NULL;
}

int getFieldSize(rpcField *field)
{
    switch (field->type)
    {
    case RPC_INT: {
        return sizeof(rpc_int32_t);
    }
    case RPC_INT8: {
        return sizeof(rpc_int8_t);
    }
    case RPC_INT16: {
        return sizeof(rpc_int16_t);
    }
    case RPC_INT64: {
        return sizeof(rpc_int64_t);
    }
    case RPC_FLOAT: {
        return sizeof(rpc_float_t);
    }
    case RPC_DOUBLE: {
        return sizeof(rpc_double_t);
    }
    case RPC_STRING: {
        return sizeof(rpcString);
    }
    case RPC_CLASS: {
        return sizeof(field->clsptr->c_size);
    }
    default:
        assert(0);
        return -1;
    }
}

static const char *typeName[] = {
    "rpc_int",
    "rpc_string",
    "rpc_class",
    NULL,
    NULL,
    NULL,
    "rpc_int8",
    "rpc_int16",
    "rpc_float",
    "rpc_double",
    "rpc_int64",
};

const char *getTypeName(int type)
{
    if (type >= 0 && type < (int)(sizeof(typeName)/sizeof(char*))) {
        if (NULL == typeName[type]) return "rpc_unknow_type";
        return typeName[type];
    }

    return "rpc_unknow_type";

}

rpcObject *initRpcObjectFromBuf(rpcObject *obj, const char *buf, size_t size, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas)
{
    rpcTable *table = (rpcTable*)malloc(sizeof(rpcTable));
    if (rpcTableCreateFromBuf(table, buf, size, clsMetas, funcMetas)) {
        free(table);
        return NULL;
    }

    obj->table = table;
    obj->classMeta = clsMetas;
    obj->funcMeta = funcMetas;

    obj->packBufInitSize = 4096;
    obj->packBuf = (rpcMbuf*)malloc(sizeof(rpcMbuf));
    rpcMbufInit(obj->packBuf, obj->packBufInitSize);

    obj->unpackBufInitSize = 4096;
    obj->unpackBuf = (rpcMbuf*)malloc(sizeof(rpcMbuf));
    rpcMbufInit(obj->unpackBuf, obj->unpackBufInitSize);

    obj->inBuf = (rpcInputBuf*)malloc(sizeof(rpcInputBuf));

    obj->cPackCnt = 0;
    obj->cUnpackCnt = 0;
    obj->scriptPackCnt = 0;
    obj->scriptUnpackCnt = 0;

    return obj;
}

rpcObject *rpcObjectInit(rpcObject *obj, const char *file, rpcClassMeta *clsMetas, rpcFunctionMeta *funcMetas)
{
    FILE *f;
    char *buf;
    size_t len;
    size_t n;

    f = fopen(file, "r");
    if (f == NULL) {
        fprintf(stderr, "can not open file:%s\n", file);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);

    buf = (char*)malloc(sizeof(char) * len + 1);
    if (buf == NULL) {
        fprintf(stderr, "malloc: out of memory");
        fclose(f);
        return NULL;
    }

    n = fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);

    if (n != len) {
        fprintf(stderr, "read file [%s] fail. expect:%d,read:%d\n", file, (int)len, (int)n);
        free(buf);
        return NULL;
    }

    obj = initRpcObjectFromBuf(obj, buf, len, clsMetas, funcMetas);
    free(buf);
    return obj;
}

void rpcTableFree(rpcTable *table)
{
    int i;
    rpcClassTable *clsTable = table->classTable;
    rpcFunctionTable *funcTable = table->funcTable;

    if (clsTable != NULL) {
        for (i = 0; i < clsTable->elts; ++i) {
            rpcClass *cls = &clsTable->rpcClassList[i];
            freeClass(cls);
        }
        free(clsTable);
    }

    if  (funcTable != NULL) {
        for (i = 0; i < funcTable->elts; ++i) {
            rpcFunction *func = &funcTable->rpcFuncList[i];
            freeFunction(func);
        }
        free(funcTable);
    }

    memset(table, 0, sizeof(*table));
}

int rpcObjectFree(rpcObject *obj)
{
    if (obj == NULL) return 0;

    rpcMbufFree(obj->unpackBuf);
    rpcMbufFree(obj->packBuf);

    free(obj->packBuf);
    free(obj->unpackBuf);
    free(obj->inBuf);

    if (obj->table != NULL) {
        rpcTableFree(obj->table);
        free(obj->table);
    }

    return 0;
}