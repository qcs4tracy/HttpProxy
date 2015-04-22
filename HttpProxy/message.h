//
//  message.h
//  HttpProxy
//
//  Created by QiuChusheng on 4/20/15.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
//

#ifndef __HttpProxy__message__
#define __HttpProxy__message__

#include "http_parser.h"
#include <string>
#include <vector>

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 512

struct message {
    
    message():num_headers(0), last_header_element(NONE) {}
    
    const char *name; // for debugging purposes
    const char *raw;
    enum zproxy::http_parser_type type;
    enum zproxy::http_method method;
    int status_code;
    std::string response_status;
    
    std::string request_path;
    std::string request_url;
    std::string query_string;
    
    const char *body;
    size_t body_size;
    
    std::string host;
    const char *userinfo;
    uint16_t port;
    
    int num_headers;
    enum { NONE=0, FIELD, VALUE } last_header_element;
    std::vector<std::pair<std::string, std::string>> headers;
    
    int should_keep_alive;
    const char *upgrade; // upgraded body
    
    unsigned short http_major;
    unsigned short http_minor;
    
    //flags
    int message_begin_cb_called;
    int headers_complete_cb_called;
    int message_complete_cb_called;
    int message_complete_on_eof;
    int body_is_final;
};

#endif /* defined(__HttpProxy__message__) */
