//
//  cache.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 4/21/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#include "cache.h"
#include "md5.h"
#include "misc.h"
#include "mem_alloc.h"

LruCache::LruCache(int n):_nodes(n), Lru(0) {

}

void LruCache::cacheName(const std::string &path, std::string &ret) {
    MD5 md5(path);
    ret = path.substr(0, path.find('/')) + md5.hexdigest();
}

void LruCache::make_lru(CacheNode * n) {
    lruList.remove(n);
    Lru = n;
    lruList.push_back(n);
}

CacheNode* LruCache::addNew(const std::string &digest, void *obj) {
    std::string hex;
    LruCache::cacheName(digest, hex);
    ulong tag = inline_hash_func(hex.c_str(), (uint)hex.length());
    BufferChain *bc = (BufferChain *)obj;
    CacheNode *n = new CacheNode(tag, obj, bc->size(), CacheNode::Mark);
    root.Insert(n);
    _nodes++;
    return n;
}

CacheNode* LruCache::search(const std::string &key) {
    std::string hex;
    LruCache::cacheName(key, hex);
    ulong tag = inline_hash_func(hex.c_str(), (uint)hex.length());
    vector<RedBlackTreeNode *> *v = root.Enumerate(tag, tag);
    if (!v->empty()) {
        return (CacheNode *)(*v)[0];
    }
    return NULL;
}



