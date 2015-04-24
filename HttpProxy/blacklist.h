//
//  blacklist.h
//  HttpProxy
//
//  Created by QiuChusheng on 4/23/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#ifndef __HttpProxy__blacklist__
#define __HttpProxy__blacklist__

#include <map>

class BlackList {
    
public:
    void loadBlackList(const std::string conf);
    bool isBlocked(const std::string &host);
    void addBlockedSite(const std::string &ws);
    void removeBlockedSite(const std::string &ws);
    
private:
    std::map<std::string, int> _bl;
};

#endif /* defined(__HttpProxy__blacklist__) */
