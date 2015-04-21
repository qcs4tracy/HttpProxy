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
#include <memory.h>
#include <list>


class CacheNode: public RedBlackEntry {

    CacheNode( const CacheNode& );  //undefined
    CacheNode& operator= (const CacheNode&); //undefined

public:
    enum Status { Release, Mark };
    CacheNode():_stat(Release),_tag(-1),_obj(NULL){}
    CacheNode(size_t obj_sz, void *obj): _stat(Mark), _sz(obj_sz), _obj(obj) {}
    virtual ~CacheNode() {}
    int _stat;    /* node status */
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
    
    //list of CacheNodes
    std::list<CacheNode *> _nodesList;
    
    //Lru Node
    CacheNode *Lru;
    
    //root for the red balck tree of cache nodes
    RedBlackTree root;
    
    //Cache Node allocator
    std::allocator<CacheNode> allocator;
    
    //make the node LRU
    CacheNode *make_lru(CacheNode *);
    
    //hook to clean the to-be-expelled cache node
    virtual void Proc(const CacheNode *) = 0;
    
public:
    LruCache(int nn,int objsz);
    virtual ~LruCache ();
    LruCache( const LruCache& );  //undefined
    LruCache& operator= (const LruCache&); //undefined
    CacheNode *search( ulong tag );
    CacheNode *addNew( ulong tag, size_t obj_sz );
    CacheNode *mark( ulong tag, CacheNode::Status stat);
    void Flush();
    
    
#if defined( TESTING )
    ulong Hits() const { return _hits; }
    ulong Miss() const { return _miss; }
    ulong Adds() const { return _adds; }
    CacheNode *LruNode() const { return Lru; }
#endif
};



#endif /* defined(__HttpProxy__cache__) */
