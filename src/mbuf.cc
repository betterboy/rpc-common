#include "mbuf.h"
#include <algorithm>

using namespace crpc;

MbufBlock::MbufBlock(size_t len)
    :readerIndex_(0),
    writerIndex_(0)
{
     size_t size  = len < kBlockSize ? kBlockSize : len;
    size = kBlockSizeFactor * ((size + kBlockSizeFactor - 1) / kBlockSizeFactor);
    if (size < kBlockMinSize) size = kBlockMinSize;
    buffer_.resize(size);
}

//默认初始化一个block
Mbuf::Mbuf(size_t blockSize)
{
    mbuf_.push_back(std::unique_ptr<MbufBlock>(new MbufBlock(blockSize)));
    size_ = 0;
}

void Mbuf::reset()
{
    mbuf_.clear();
    mbuf_.push_back(std::unique_ptr<MbufBlock>(new MbufBlock(kInitialBlkSize)));
}

const char* Mbuf::pullup()
{
    if (mbuf_.size() <= 0) return nullptr;
    if (mbuf_.size() == 1) return mbuf_.front()->begin();

    MbufBlockPtr temp(new MbufBlock(size_));
    std::list<MbufBlockPtr>::const_iterator iter;
    for (iter = mbuf_.begin(); iter != mbuf_.end(); ++iter) {
        temp->append((*iter)->peek(), (*iter)->readableBytes());
    }
    mbuf_.clear();
    mbuf_.push_back(temp);
    return mbuf_.front()->begin();
}

void Mbuf::append(const void* data, size_t len)
{
    append(static_cast<const char*>(data), len);
}

void Mbuf::append(const char* data, size_t len)
{
    assert(len > 0);
    if (mbuf_.empty()) {
        mbuf_.push_back(std::unique_ptr<MbufBlock>(new MbufBlock(kInitialBlkSize)));
    }

    size_t lastBlkFreeSize = mbuf_.back()->writableBytes();
    if (lastBlkFreeSize > 0) {
        size_t writableSize = std::min(lastBlkFreeSize, len);
        mbuf_.back()->append(data, writableSize);
        len -= writableSize;
        size_ += writableSize;
    }
    if (len <= 0) return;

    MbufBlockPtr block(new MbufBlock(len));
    block->append(data + lastBlkFreeSize, len);
    mbuf_.push_back(block);
    size_ += len;
}

//如果要获取的数据量大于当前载荷，则返回实际取出的大小
size_t Mbuf::dequeue(char* ret, size_t len)
{
    assert(len > 0);
    size_t offset = 0;
    size_t dequeSize = 0;
    std::list<MbufBlockPtr>::iterator iter;
    for (iter = mbuf_.begin(); iter != mbuf_.end() && len >= 0; ++iter)
    {
        size_t load = (*iter)->readableBytes();
        size_t canReadBytes = std::min(load, len);
        std::copy((*iter)->peek(), (*iter)->peek() + canReadBytes, ret + offset);
        len -= canReadBytes;
        offset += canReadBytes;
        dequeSize += canReadBytes;
    }
    return dequeSize;
}

void Mbuf::retrieve(size_t len)
{
    assert(len > 0);
    while (len) {
        MbufBlockPtr blk = mbuf_.front();
        size_t load = blk->readableBytes();
        size_t readableSize = std::min(len, load);
        blk->retrieve(readableSize);
        size_ -= readableSize;
        len -= readableSize;
        if (blk->readableBytes() <= 0) {
            mbuf_.pop_front();
        }
    }
}

