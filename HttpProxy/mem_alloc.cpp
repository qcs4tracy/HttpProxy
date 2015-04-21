//
//  mem_alloc.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 4/19/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#include "mem_alloc.h"

RawBuffer::RawBuffer(char *buf, size_t size):_start(buf), _pos(buf), _last(buf),
                                             _end(buf+size), _size(size), _avail(size) {}


size_t RawBuffer::write(const char *data, size_t len) {
    
    size_t n = _avail < len? _avail: len;
    std::copy(data, data + n, _last);
    _last += n;
    _avail -= n;
    
    return n;
}


size_t RawBuffer::read(char *buf, size_t len) {
    
    size_t vaild_sz = _last - _pos;
    len = vaild_sz > len? len: vaild_sz;
    std::copy(_pos, _pos+len, buf);
    _pos += len;
    
    return len;
}


RawBuffer *BuffAllocator::createBuffer(size_t size) {

    char *buf = allocator.allocate(size);
    RawBuffer *raw = new RawBuffer(buf, size);
    _buffPool.push_back(raw);
    alloc_size_ += size;

    return raw;
}


RawBuffer *BuffAllocator::getBuffer() {
    
    RawBuffer *buf;
    
    if (!_freeBuffs.empty()) {
        buf = _freeBuffs.back();
        _freeBuffs.pop_back();
        return buf;
    }
    
    buf = createBuffer();
    return buf;
}


void BuffAllocator::freeBuffer(RawBuffer *buf, bool del) {
    //delete the buffer's memory
    if (del) {
        allocator.deallocate(buf->_start, buf->_size);
        _buffPool.remove(buf);
        delete buf;
        return;
    }
    //return to the free buffer pool
    buf->reset();
    _freeBuffs.push_back(buf);
}



//chain up a list of raw buffers
//class BufferChain {
//    
//public:
//    
//    BufferChain(BuffAllocator &alloc = BuffAllocator::getInstance()):numOfBuffs(0), _lastAvail(NULL), _alloc(alloc)  {}
//    size_t write(const char *data, size_t size);
//    bool empty() { return numOfBuffs == 0; }
//    void freeBuffs();
//    
//private:
//    //list of Raw Buffers
//    std::list<RawBuffer *> _chain;
//    RawBuffer *_lastAvail;
//    BuffAllocator &_alloc;
//    int numOfBuffs;
//    size_t _totalBytes;
//};



void BufferChain::freeBuffs() {
    //if the chain is not empty, free the buffers in it
    if (!_chain.empty()) {
        //C++11 foreach
        for(auto buf : _chain) {
            _alloc.freeBuffer(buf);
        }
    }
}

//add a new buffer to the chain
RawBuffer* BufferChain::addNewBuffer() {
    
    RawBuffer *buf = _alloc.getBuffer();
    _chain.push_back(buf);
    _lastAvail = buf;
    return buf;
}

size_t BufferChain::write(const char *data, size_t size) {
    
    RawBuffer *buf = _lastAvail;
    size_t nLeft = size;
    size_t nw = 0;
    
    //the buffer chain is empty
    if ((!buf && _chain.empty()) || buf->isFull()) {
        buf = addNewBuffer();
    }
    
    do {
        nw = buf->write(data, size);
        nLeft -= nw;
        data += nw;
        //if the current buffer is full
        if (buf->isFull()) {
            buf = addNewBuffer();
        }
    } while (nLeft > 0);
    
    _totalBytes += (size - nLeft);
    
    return size - nLeft;
}











