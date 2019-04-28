#ifndef _RPC_MBUF_H_
#define _RPC_MBUF_H_

#include "copyable.h"
#include <assert.h>
#include <list>
#include <vector>
#include <memory>
#include <iostream>

//MbufBlock modeled after org.jboss.netty.buffer.ChannelBuffer
//
//---------+-------------------+---------------------|
//
//readed   | readableBytes     |  writableBytes      |
//
//---------+-------------------+---------------------|
//0 <= readerIndex_    <= writerIndex_     <=       size
//
//---------+-------------------+---------------------+
//
//

namespace crpc {

class MbufBlock : public copyable {
public:
    const static size_t kBlockSize = 2048;
    const static size_t kBlockMinSize = 4;
    const static size_t kBlockSizeFactor = 4;

    explicit MbufBlock(size_t len=kBlockSize);
    ~MbufBlock() = default;

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    void hasWriten(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    void unWriten(size_t len)
    {
        writerIndex_ -= len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = 0;
    }

    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        if (len <= readableBytes()) {
            readerIndex_ += len;
        } else {
            retrieveAll();
        }
    }

    void append(const char* data, size_t len)
    {
        assert (len <= writableBytes());
        std::copy(data, data + len, beginWrite());
        hasWriten(len);
    }

    void append(const void* data, size_t len)
    {
        append(static_cast<const char*>(data), len);
    }

    void swap(MbufBlock& rhs)
    {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }

    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

private:
    size_t readerIndex_;
    size_t writerIndex_;
    std::vector<char> buffer_;
};

class Mbuf : public copyable {
public:
    typedef std::shared_ptr<MbufBlock> MbufBlockPtr;

    static const size_t kInitialBlkSize = 2048;
    Mbuf(size_t blockSize = kInitialBlkSize);
    ~Mbuf() = default;

    size_t size() const { return size_; }
    size_t blockCount() const { return mbuf_.size(); }

    void reset();
    void append(const void* data, size_t len);
    void append(const char* data, size_t len);
    void append(const std::string& data)
    {
        return append(data.c_str(), data.size());
    }

    size_t dequeue(char* ret, size_t len);
    const char* pullup();
    void retrieve(size_t len);

private:
    std::list<MbufBlockPtr> mbuf_;
    size_t size_;
};

} //crpc
#endif
