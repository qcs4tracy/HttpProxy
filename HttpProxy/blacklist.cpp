//
//  blacklist.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 4/23/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#include "blacklist.h"
#include <map>
#include <fstream>
#include <iostream>
#include "json/json-forwards.h"
#include "json/json.h"



#define BLOCKED 1
#define UNBLOCKED 2

void BlackList::loadBlackList(const std::string conf) {

    Json::Value confRoot;
    Json::Reader reader;
    std::ifstream cf(conf, std::ifstream::binary);
    
    if (reader.parse(cf, confRoot)) {//parsed successfully
        Json::Value &bl = confRoot["blacklist"];
        if (!bl.isNull() && bl.isArray()) {
            for (Json::ArrayIndex i = 0; i < bl.size(); i++) {
                _bl[bl[i].asString()] = BLOCKED;
            }
        }
    }
    if (cf.is_open())
        cf.close();
}


void BlackList::addBlockedSite(const std::string &ws) {
    _bl[ws] = BLOCKED;
}

void BlackList::removeBlockedSite(const std::string &ws) {
    
    if (_bl.count(ws) > 0) {
        _bl.erase(ws);
    }
}


bool BlackList::isBlocked(const std::string &host) {
    return _bl.count(host) > 0 && _bl[host] == BLOCKED;
}








