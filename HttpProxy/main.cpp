//
//  main.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 15/4/16.
//  Copyright (c) 2015年 QiuChusheng. All rights reserved.
//

#include "socket.h"  // For Socket, ServerSocket, and SocketException
#include <iostream>           // For cout, cerr
#include <cstdlib>            // For atoi()
#include <pthread.h>          // For POSIX threads
#include "http_parser.h"
#include "httplib.h"
#include <cstdio>
#include <cstring>
#include <sstream>

#include "mem_alloc.h"
#include "message.h"

using namespace zproxy;
using namespace std;
const int RCVBUFSIZE = 1024;

#ifdef WIN32
#include <winsock2.h>
#endif // WIN32

size_t ncount=0;

void OnBegin(httplib::Response* r, void* userdata) {
    
    ostringstream oss;
    std::map<string, string>::const_iterator itr;
    TCPSocket *sock = (TCPSocket *) userdata;
    
    if (!r->isChunked()) {
        //send the header of the response message
        oss << r->getStatusLine() << "\r\n";
        for (itr = r->getHeaders().begin(); itr != r->getHeaders().end(); ++itr) {
            oss << itr->first << ": " << itr->second << "\r\n";
        }
        oss << "\r\n";
        sock->send(oss.str().data(), oss.str().size());
    }
}


void OnData(httplib::Response* r, void* userdata, const char* data, size_t n) {
    
    TCPSocket *sock = (TCPSocket *) userdata;
    
    if(!r->isChunked()) {
        sock->send(data, n);
    }
    
    r->getInternalBuff()->write(data, n);
    
}


void OnComplete(httplib::Response* r, void* userdata) {
    
    TCPSocket *sock = (TCPSocket *) userdata;
    
    if (r->isChunked()) {
        ostringstream oss;
        std::map<string, string> &hd = r->getHeaders();
        std::map<string, string>::const_iterator itr;
        
        //send the header of the response message
        oss << r->getStatusLine() << "\r\n";
        hd.erase("transfer-encoding");
        hd["content-length"] = r->bodyLen();
        for (itr = r->getHeaders().begin(); itr != r->getHeaders().end(); ++itr) {
            oss << itr->first << ": " << itr->second << "\r\n";
        }
        oss << "\r\n";
        
        r->getInternalBuff()->flushToSock(sock);
    } else {//unchunked
    
    }
    
}


//copy the request url to our buffer
int request_url_cb (http_parser *p, const char *buf, size_t len) {
    struct message *msg = (struct message *)p->data;
    http_parser_url url;
    int16_t port = 80;
    
    //exclude the heading `/`
    buf++;
    len--;
    
    msg->request_url = string(buf, len);
    http_parser_parse_url(buf, len, 1, &url);
    
    if (url.field_set & (1 << http_parser_url_fields::UF_HOST)) {
        msg->host = string(buf + url.field_data[http_parser_url_fields::UF_HOST].off, url.field_data[http_parser_url_fields::UF_HOST].len);
    } else {
        msg->host = "";
        /*error: Host must be specified*/
    }
    
    if (url.field_set & (1 << http_parser_url_fields::UF_PORT)) {
        port = stoi(string(buf + url.field_data[http_parser_url_fields::UF_PORT].off, url.field_data[http_parser_url_fields::UF_PORT].len));
    }
    
    msg->port = port;
    
    if (url.field_set & (1 << http_parser_url_fields::UF_PATH)) {
        msg->request_path = string(buf + url.field_data[http_parser_url_fields::UF_PATH].off, url.field_data[http_parser_url_fields::UF_PATH].len);
    } else {
        msg->request_path = "/";
    }
    
    return 0;
}

int header_field_cb (http_parser *p, const char *buf, size_t len) {
    
    struct message *msg = (struct message *)p->data;
    string field(buf, len);
    
    if (msg->last_header_element != message::FIELD) {
        msg->headers.push_back(make_pair(field, ""));
        msg->num_headers++;
    }
    
    msg->last_header_element = message::FIELD;
    return 0;
}


int header_value_cb (http_parser *p, const char *buf, size_t len) {
    
    struct message *msg = (struct message *)p->data;
    string val(buf, len);
    msg->headers[msg->num_headers-1].second = val;
    
    return 0;
}


static http_parser_settings settings =
{    .on_message_begin = EMPTY_CB()
    ,.on_header_field = header_field_cb
    ,.on_header_value = header_value_cb
    ,.on_url = request_url_cb
    ,.on_body = EMPTY_DATA_CB()
    ,.on_headers_complete = EMPTY_DATA_CB()
    ,.on_message_complete = EMPTY_CB()
    ,.on_reason = EMPTY_DATA_CB()
    ,.on_chunk_header = EMPTY_CB()
    ,.on_chunk_complete = EMPTY_CB()
};

void HandleTCPClient(TCPSocket *sock);     // TCP client handling function
void *ThreadMain(void *arg);               // Main program of a thread

int main(int argc, char *argv[]) {
    
    try {
        TCPServerSocket servSock(5999);   // Socket descriptor for server
        
        for (;;) {      // Run forever
            // Create separate memory for client argument
            TCPSocket *clntSock = servSock.accept();
            
            // Create client thread
            pthread_t threadID;              // Thread ID from pthread_create()
            if (pthread_create(&threadID, NULL, ThreadMain,
                               (void *) clntSock) != 0) {
                cerr << "Unable to create thread" << endl;
                exit(1);
            }
        }
    } catch (SocketException &e) {
        cerr << e.what() << endl;
        exit(1);
    }
    // NOT REACHED
    return 0;
}



// TCP client handling function
void HandleTCPClient(TCPSocket *sock) {
    
    cout << "Handling client ";
    try {
        cout << sock->getForeignAddress() << ":";
    } catch (SocketException &e) {
        cerr << "Unable to get foreign address" << endl;
    }
    try {
        cout << sock->getForeignPort();
    } catch (SocketException &e) {
        cerr << "Unable to get foreign port" << endl;
    }
    cout << " with thread " << pthread_self() << endl;
    
    // Send received string and receive again until the end of transmission
    char buff[RCVBUFSIZE];
    
    ssize_t recvMsgSize;
    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    struct message *msg = new message;
    parser.data = (void *)msg;
    httplib::Connection *conn;
    
    //read the request from the client
    while ((recvMsgSize = sock->recv(buff, RCVBUFSIZE)) > 0) { // Zero means end of transmission
        
        http_parser_execute(&parser, &settings, buff, recvMsgSize);
        conn = new httplib::Connection(msg->host, msg->port);
        conn->putRequest("GET", msg->request_path.c_str());
        
        for (int i = 0; i < msg->headers.size(); ++i) {
            conn->addHeader(msg->headers[i].first, msg->headers[i].second);
        }
        
        //TODO: change callbacks for response data
        conn->setcallbacks(OnBegin, OnData, OnComplete, (void *)sock);
        conn->sendRequest();
        
        //TODO: process response data
        while(conn->outstanding())
            conn->pump();
    }
    
    // Destructor closes socket
}

void *ThreadMain(void *clntSock) {
    
    int exit_code = 0;
    // Guarantees that thread resources are deallocated upon return
    pthread_detach(pthread_self());
    
    // Extract socket file descriptor from argument  
    HandleTCPClient((TCPSocket *) clntSock);
    
    delete (TCPSocket *) clntSock;
    pthread_exit(&exit_code);
    return NULL;
}







