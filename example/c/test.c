#include "src/rpc.h"
#include "src/rpc_c.h"
#include "rpc_meta.h"

extern rpcClassMeta auto_rpc_class_metas[];
extern rpcFunctionMeta auto_rpc_function_metas[];

void rpc_server_move_rebind(int vfd, move_t *m)
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

int main(int argc, char *argv[])
{
    rpcObject *obj = (rpcObject*)malloc(sizeof(rpcObject));
    obj = rpcObjectInit(obj, "rpc.cfg", auto_rpc_class_metas, auto_rpc_function_metas);
    if (obj == NULL) {
        fprintf(stderr, "init rpc table fail\n");
        exit(-1);
    }

    bindCFunction(obj, "rpc_server_move", (rpc_c_func_t)rpc_server_move_rebind);

    move_t m;
    m.speed = 8888;
    position_t path[] = {
        {1, 1},
        {1, 2},
        {1, 3},
        {1, 4},
    };

    rpcArray array;
    array.n = 4;
    array.data = (void*)path;
    m.path = array;

    int i;
    rpc_pto_id_t pid = 0;
    rpcFunctionTable *tbl = obj->table->funcTable;
    for (i = 0; i < tbl->elts; ++i) {
        rpcFunction *func = &tbl->rpcFuncList[i];
        if (strcmp(func->argClass.name, "rpc_server_move") == 0) {
            pid = func->pto_id;
            break;
        }
    }
    int ret = packFromC(obj, pid, &m);
    printf("pid=%d, ret=%d, plen=%d\n", pid, ret, (int)obj->packBuf->dataSize);

    //unpack

    char buf[1024];
    const char *input = rpcMbufPullup(obj->packBuf);
    memcpy(buf, input, 36);
    buf[37] = 0;
    move_t *unpack_m;
    rpc_pto_id_t unpack_pid;
    size_t unpack_ret = unpackToC(obj, input, obj->packBuf->dataSize, &unpack_pid, (void**)&unpack_m);
    printf("unpack_ret=%d\n", unpack_ret);
    return 0;
}