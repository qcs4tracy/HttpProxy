//
//  wordfilter.h
//  HttpProxy
//
//  Created by QiuChusheng on 4/26/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#ifndef __HttpProxy__wordfilter__
#define __HttpProxy__wordfilter__

#include <string>

//forward declarations
namespace rtv {
    
    template < typename T,typename V, typename Cmp > class SetItems;
    
    template <typename T, typename V, typename Cmp = std::less<T>, typename Items = SetItems<T, V, Cmp>> class Trie;
}

class WordFilter {
    
public:
    
    WordFilter();
    
    virtual ~WordFilter();
    
    void addWord(const std::string &word);
    
    void loadWords(const std::string &path);
    
    void filter(char *buff, size_t sz);
    
private:
    rtv::Trie<char, std::string> *_dict;
};


#endif /* defined(__HttpProxy__wordfilter__) */
