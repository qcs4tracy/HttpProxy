//
//  cache.h
//  HttpProxy
//
//  Created by QiuChusheng on 4/21/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#ifndef __HttpProxy__cache__
#define __HttpProxy__cache__

#include "misc.h"
#include "RedBlackTree.h"
#include <iostream>
#include <list>


class CacheNode: public RedBlackEntry {

    CacheNode( const CacheNode& );  //undefined
    CacheNode& operator= (const CacheNode&); //undefined

public:
    enum Status { Release, Mark };
    CacheNode(ulong tag_ = -1, void *obj_ = 0, size_t obj_sz_ = 0, Status st = Release):_stat(st),_tag(tag_), _obj(obj_),_sz(obj_sz_) { }
    
    virtual ~CacheNode() {}
    
    Status _stat;    /* node status */
    size_t _sz;
    ulong _tag;   /* object id# */
    void * _obj;  /* -> user object storage */
    char _key[33];/*md5 digest*/
    bool operator == (CacheNode & p) { return _tag == p._tag && memcmp(_key, p._key, 33); }
    
    //key for red black tree
    virtual ulong GetKey() const { return _tag; }
};



class LruCache  {
protected:
#ifdef TESTING
    ulong _hits, _miss, _adds;  /* cache stats */
#endif
    
    int _nodes;     /* # CacheNodes allocated */

    //Lru Node
    CacheNode *Lru;
    
    //root for the red balck tree of cache nodes
    RedBlackTree root;
    
    //LRU list
    std::list<CacheNode *> lruList;
    
    //make the node LRU
    void make_lru(CacheNode * n);
    
    LruCache( const LruCache& );  //undefined
    LruCache& operator= (const LruCache&); //undefined
    
    //hook to clean the to-be-expelled cache node
    //virtual void Proc(const CacheNode *);
    
public:
    LruCache(int nn = 0);
    virtual ~LruCache () {}
    static void cacheName(const std::string &path, std::string &ret);
    
    //CacheNode *search(ulong tag );
    CacheNode *search(const std::string &key);
    //CacheNode *addNew(ulong tag, size_t obj_sz );
    CacheNode *addNew(const std::string &digest, void *obj);
    //CacheNode *mark( ulong tag, CacheNode::Status stat);
    //void Flush();
    
    
#if defined( TESTING )
    ulong Hits() const { return _hits; }
    ulong Miss() const { return _miss; }
    ulong Adds() const { return _adds; }
    CacheNode *LruNode() const { return Lru; }
#endif
};



#endif /* defined(__HttpProxy__cache__) */
