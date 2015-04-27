//
//  wordfilter.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 4/26/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#include "wordfilter.h"
#include "trie.h"
#include "porter2_stemmer.h"
#include "../json/json-forwards.h"
#include "../json/json.h"
#include <fstream>

#define IS_WHITESPACE(c) (c=='\n' || c== '\t' || c == ' ' || c == '\r')
#define IS_LETTER(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))

WordFilter::WordFilter(): _dict(new rtv::Trie<char, std::string>('\0')) {}

WordFilter::~WordFilter() {
    _dict->clear();
    delete _dict;
}

//add a insult word to the Trie-Tree
void WordFilter::addWord(const std::string &word) {
    std::string w(word);
    std::transform(w.begin(), w.end(), w.begin(), ::tolower);
    Porter2Stemmer::stem(w);
    _dict->insert(w.c_str(), "stemmed insult word to filter");
}


void WordFilter::loadWords(const std::string &path) {
    
    Json::Reader reader;
    Json::Value root;
    std::ifstream in(path, std::ifstream::binary);
    
    //parse the json file to array of words
    if (reader.parse(in, root)) {
        Json::Value &words = root["words"];
        if (words.isArray()) {
            for (Json::ArrayIndex i = 0; i < words.size(); i++) {
                addWord(words[i].asString());
            }
        }
    }
    
    //close the file stream
    if (in.is_open()) {
        in.close();
    }
}


void WordFilter::filter(char *buff, size_t sz) {
    
    char *ptr = buff;
    char *bk, *ptr_tmp, *buf_end = buff + sz;
    std::string word;
    
    while (ptr < buf_end) {
        
        //tokenize words by whitespace
        //strip out all leading whitespaces and non-letter characters
        while (IS_WHITESPACE(*ptr) || !IS_LETTER(*ptr)) {
            ptr++;
        }
        
        bk = ptr;
        
        //scan letters
        while (IS_LETTER(*ptr) && ptr < buf_end) {
            ptr++;
        }
        
        //word at least of length 1
        if (ptr - bk > 1) {
            
            word.assign(bk, ptr);
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            Porter2Stemmer::stem(word);
            
            //the word is in the dictionary
            if (!_dict->hasKey(word.c_str())) {
                
                //test if it is the prefix of bi-gram or `-` connected word
                rtv::Trie<char, std::string>::Iterator itr = _dict->startsWith(word.c_str());
                //if there's no word with this prefix, discard it
                if (itr == _dict->end()) {
                    continue;
                }
                //may be used to backtrack
                ptr_tmp = ptr;
                //look ahead to the next letter
                while (!IS_LETTER(*ptr) && ptr < buf_end) {
                    ptr++;
                }
                //scan letters
                while (IS_LETTER(*ptr) && ptr < buf_end) {
                    ptr++;
                }
                //reform the word
                word.assign(bk, ptr);
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                Porter2Stemmer::stem(word);
                //still not a insult word
                if (!_dict->hasKey(word.c_str())) {
                    //backtrack and continue
                    ptr = ptr_tmp;
                    continue;
                }
                
            }
            
            //simple filtering
            for (;bk < ptr; bk++) {
                *bk = '*';
            }
        }
    }
    
}




