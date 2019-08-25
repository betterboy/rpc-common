/*
The implementation of mbuf.
*/
#ifndef _RPC_MBUF_H_
#define _RPC_MBUF_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct rpc_mbuf_t rpcMbuf;

typedef struct rpc_mbuf_block_t {
    struct rpc_mbuf_block_t *next;
    rpcMbuf *mbuf;
    unsigned id;
    size_t size;
    char *buf;
    char *head;
    char *tail;
    char *end;

} rpcMbufBlock;

struct rpc_mbuf_t {
    rpcMbufBlock *head;
    rpcMbufBlock *tail;

    rpcMbufBlock *blockEnq;
    rpcMbufBlock *blockDeq;
    int blockCnt;
    size_t dataSize;
    size_t hintSize;
    size_t allocSize;
};

#define rpcMbufAdvance(mbuf, blk, len) do {\
    (mbuf)->dataSize += len; \
    (blk)->tail += len; \
} while(0)

#define rpcMbufBlockCap(blk) ((blk)->end - (blk)->tail)
#define rpcMbufBlockDataLen(blk) ((blk)->tail - (blk)->head)

#define rpcMbufEnq(mbuf, data, len) do {\
    void *p = rpcMbufAlloc(mbuf, len); \
    memcpy(p, data, len); \
} while(0)

#define rpcMbufEnqType(mbuf, data, type) do {\
    type *p = rpcMbufAlloc(mbuf, sizeof(type)); \
    *p = *(data); \
} while(0)

typedef struct rpc_input_buf_t {
    const char *buf;
    size_t offset;
    size_t size;

} rpcInputBuf;

#define rpcInputBufInit(in, data, len) do {\
    (in)->buf = (data); \
    (in)->size = (len); \
    (in)->offset = 0; \
} while(0)

#define rpcInputBufPos(in) ((void*)((in)->buf + (in)->offset))
#define rpcInputBufLen(in) ((in)->size - (in)->offset)
#define rpcInputBufAdvance(in, len) ((in)->offset += (len))
#define rpcInputBufCheck(in, len) do {\
    if (rpcInputBufLen((in)) < (len)) return -1; \
} while(0)

#define rpcInputBufGetNoCheck(in, retv, len) do {\
    memcpy((retv), rpcInputBufPos((in)), (len)); \
    rpcInputBufAdvance((in), (len)); \
} while(0)

#define rpcInputBufGetType(in, retv, type) do {\
    rpcInputBufCheck((in), sizeof(type)); \
    *(type*)(retv) = *(type*)rpcInputBufPos((in)); \
    rpcInputBufAdvance((in), sizeof(type)); \
} while(0)

/*
public interface
*/

#if defined(__cplusplus)
extern "C" {
#endif

void rpcMbufInit(rpcMbuf *mbuf, size_t blockSize);
void rpcMbufFree(rpcMbuf *mbuf);
rpcMbufBlock *rpcMbufAddBlock(rpcMbuf *mbuf, size_t size);
void *rpcMbufPush(rpcMbuf *mbuf, void *data, size_t len);
void rpcMbufEnqSpan(rpcMbuf *mbuf, void *data, size_t len);
size_t rpcMbufDeq(rpcMbuf *mbuf, void *ret, size_t len);
void rpcMbufReset(rpcMbuf *mbuf, size_t size);
char *rpcMbufPullup(rpcMbuf *mbuf);
void rpcMbufDrain(rpcMbuf *mbuf, size_t len);

inline static void *rpcMbufAlloc(rpcMbuf *mbuf, size_t len)
{
    while ((size_t)rpcMbufBlockCap(mbuf->blockEnq) < len) {
        mbuf->blockEnq = mbuf->blockEnq->next;
        if (mbuf->blockEnq == NULL) {
            rpcMbufAddBlock(mbuf, len);
            break;
        }
        assert(rpcMbufBlockDataLen(mbuf->blockEnq) == 0);
    }
    rpcMbufAdvance(mbuf, mbuf->blockEnq, len);
    return mbuf->blockEnq->tail - len;
}

#if defined(__cplusplus)
}
#endif

#endif