//
//  mem_alloc.h
//  HttpProxy
//
//  Created by QiuChusheng on 4/19/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#ifndef __HttpProxy__mem_alloc__
#define __HttpProxy__mem_alloc__

#include <memory>
#include <list>
#include "socket.h"

class BuffAllocator;
class BufferChain;
class WordFilter;

class RawBuffer {

    friend class BuffAllocator;
    friend class BufferChain;
public:

    virtual ~RawBuffer() {}
    RawBuffer(char *buf, size_t size);
    char *buffEnd() { return _end; }
    char *buffStart() { return _start; }
    char *buffLast() { return _last; }
    char *getPos() { return _pos; }
    size_t write(const char *data, size_t len);
    size_t read(char *buf, size_t len);
    
    //reset the buffer for reuse
    void reset() {
        _pos = _last = _start;
        _avail = _size;
    }
    
    //is the buff full or not
    bool isFull() { return _avail <= 0; }

private:
    char *_start;//the start address of the buffer
    char *_end;//the end position for the entire buffer
    char *_pos;//the start position for valid part
    char *_last;//the end position for valid part
    size_t _avail;//how many bytes available
    size_t _size;//the size of the entire buffer
    
};


class BuffAllocator {
    
    //one RawBuffer is of size 4KB, i.e. 1 page
    const static size_t BUFF_SZ = 4096;
    
public:
    
    RawBuffer *createBuffer(size_t size = BUFF_SZ);
    RawBuffer *getBuffer();
    void freeBuffer(RawBuffer *buf, bool del = false);
    
    static BuffAllocator &getInstance() {
        
        static BuffAllocator _instance;
        return _instance;
    }
    
private:
    BuffAllocator(const BuffAllocator&);
    void operator=(const BuffAllocator&);
    BuffAllocator():alloc_size_(0) { }
    std::allocator<char> allocator;
    std::list<RawBuffer *> _buffPool;
    std::list<RawBuffer *> _freeBuffs;
    size_t alloc_size_;
};


//chain up a list of raw buffers
class BufferChain {

public:
    
    BufferChain(BuffAllocator &alloc = BuffAllocator::getInstance()):numOfBuffs(0), _lastAvail(NULL), _alloc(alloc), _totalBytes(0)  {}
    size_t write(const char *data, size_t size);
    bool empty() { return numOfBuffs == 0; }
    void freeBuffs();
    void flushToSock(TCPSocket *sock, WordFilter &filter);
    
private:
    RawBuffer *addNewBuffer();
    //list of Raw Buffers
    std::list<RawBuffer *> _chain;
    RawBuffer *_lastAvail;
    BuffAllocator &_alloc;
    int numOfBuffs;
    size_t _totalBytes;
};



#endif /* defined(__HttpProxy__mem_alloc__) */
