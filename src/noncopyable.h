#ifndef _RPC_NONCOPYABLE_H_
#define _RPC_NONCOPYABLE_H_

namespace crpc {

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    const noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

} //namespace crpc

#endif
