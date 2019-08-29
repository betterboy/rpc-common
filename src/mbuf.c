#include "mbuf.h"

#include <string.h>
#include <errno.h>

#define MIN_BLK_SIZE (4)
#define BLK_FACTOR (4)
#define BLK_SIZE (4096)

inline static void rpcMbufBlockInit(rpcMbufBlock *blk)
{
    blk->head = blk->tail = blk->buf;
}

rpcMbufBlock *rpcMbufAddBlock(rpcMbuf *mbuf, size_t size)
{
    size = mbuf->hintSize > size ? mbuf->hintSize : size;
    size = BLK_FACTOR * ((size + BLK_FACTOR - 1) / BLK_FACTOR);
    if (size < MIN_BLK_SIZE)
        size = MIN_BLK_SIZE;

    rpcMbufBlock *blk = (rpcMbufBlock *)malloc(sizeof(rpcMbufBlock) + size);

    blk->id = mbuf->blockCnt;
    blk->size = size;
    blk->buf = (char *)(blk + 1);
    blk->end = blk->buf + blk->size;
    rpcMbufBlockInit(blk);
    blk->next = NULL;

    if (mbuf->tail == NULL)
    {
        mbuf->head = mbuf->tail = blk;
    }
    else
    {
        mbuf->tail->next = blk;
        mbuf->tail = blk;
    }

    mbuf->blockCnt++;
    mbuf->allocSize += size;
    mbuf->blockEnq = blk;

    return blk;
}

void rpcMbufInit(rpcMbuf *mbuf, size_t blockSize)
{
    mbuf->hintSize = blockSize == 0 ? BLK_SIZE : blockSize;
    mbuf->dataSize = 0;
    mbuf->allocSize = 0;
    mbuf->blockCnt = 0;
    mbuf->head = NULL;
    mbuf->tail = NULL;
    mbuf->blockDeq = NULL;
    mbuf->blockEnq = NULL;

    mbuf->blockDeq = rpcMbufAddBlock(mbuf, blockSize);
}

void rpcMbufFree(rpcMbuf *mbuf)
{
    rpcMbufBlock *blk, *tmp;
    for (blk = mbuf->head; blk && (tmp = blk->next, 1); blk = tmp)
    {
        free(blk);
        mbuf->blockCnt--;
    }
}

void rpcMbufReset(rpcMbuf *mbuf, size_t size)
{
    if (mbuf->blockCnt > 1 || mbuf->allocSize < size)
    {
        rpcMbufFree(mbuf);
        rpcMbufInit(mbuf, (size > mbuf->allocSize) ? size : mbuf->allocSize);
    }
    else
    {
        assert(mbuf->blockCnt == 1);
        rpcMbufBlockInit(mbuf->head);
        mbuf->dataSize = 0;
    }

    assert(mbuf->head == mbuf->tail && mbuf->head == mbuf->blockEnq && mbuf->head == mbuf->blockDeq);
}

char *rpcMbufPullup(rpcMbuf *mbuf)
{
    size_t size, offset;
    rpcMbufBlock *blk;
    if (mbuf->blockCnt <= 0)
        return NULL;
    if (mbuf->blockCnt == 1) {
        printf("blockcnt=%d,size=%d,data=%d\n", mbuf->blockCnt, mbuf->head->size, mbuf->head->tail - mbuf->head->head);

        goto done;
    }

    size = mbuf->allocSize;
    blk = (rpcMbufBlock *)malloc(sizeof(rpcMbufBlock) + size);
    blk->buf = (char *)(blk + 1);
    blk->mbuf = mbuf;
    blk->id = 0;
    blk->size = size;
    blk->end = blk->buf + size;
    rpcMbufBlockInit(blk);
    blk->next = NULL;

    offset = 0;
    rpcMbufBlock *tblk, *tmp;
    for (tblk = mbuf->head; tblk && (tmp = tblk->next, 1); tblk = tmp)
    {
        size_t len = rpcMbufBlockDataLen(tblk);
        if (len > 0)
        {
            memcpy(blk->buf + offset, tblk->head, len);
            offset += len;
        }
        free(tblk);
    }

    blk->tail = blk->buf + offset;
    mbuf->head = mbuf->tail = mbuf->blockDeq = mbuf->blockEnq = blk;
    mbuf->blockCnt = 1;
done:
    return mbuf->head->head;
}

void rpcMbufEnqSpan(rpcMbuf *mbuf, void *data, size_t len)
{
    rpcMbufBlock *blk = mbuf->blockEnq;
    size_t capacity = rpcMbufBlockCap(blk);
    char *dat = (char *)data;
    if (capacity < len)
    {
        memcpy(blk->tail, dat, capacity);
        rpcMbufAdvance(mbuf, blk, capacity);
        len -= capacity;
        dat += capacity;
        blk = rpcMbufAddBlock(mbuf, len);
    }

    memcpy(blk->tail, dat, len);
    rpcMbufAdvance(mbuf, blk, len);
}

void *rpcMbufPush(rpcMbuf *mbuf, void *data, size_t len)
{
    void *p = rpcMbufAlloc(mbuf, len);
    if (data)
        memcpy(p, data, len);
    return p;
}

size_t rpcMbufDeq(rpcMbuf *mbuf, void *ret, size_t len)
{
    rpcMbufBlock *blk = mbuf->blockDeq;
    size_t slen = len;

    do
    {
        size_t payload = blk->end - blk->tail;
        size_t min = payload < len ? payload : len;
        if (min > 0)
        {
            if (ret != NULL)
            {
                memcpy(ret, blk->head, min);
            }
            blk->head += min;
            mbuf->dataSize -= min;
            len -= min;
        }
    } while (len > 0 && NULL != (blk = mbuf->blockDeq = blk->next));
    return slen - len;
}

void rpcMbufDrain(rpcMbuf *mbuf, size_t len)
{
    rpcMbufDeq(mbuf, NULL, len);
}
