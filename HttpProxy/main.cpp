//
//  main.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 15/4/16.
//  Copyright (c) 2015 QiuChusheng. All rights reserved.
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
#include <fstream>

#include "mem_alloc.h"
#include "message.h"
#include "blacklist.h"
#include "filter/wordfilter.h"

using namespace zproxy;
using namespace std;
const int RCVBUFSIZE = 1024;

//forward declarations
void HandleTCPClient(TCPSocket *sock);     // TCP client handling function
void *ThreadMain(void *arg);               // Main program of a thread
BlackList blacklist;
char buff403[1024];
size_t buff403len;

//word filter
WordFilter filter;


#ifdef WIN32
#include <winsock2.h>
#endif // WIN32

//callback that is called after the header is completely parsed
void onBegin(httplib::Response* r, void* userdata) {
    
    ostringstream oss;
    std::map<string, string>::const_iterator itr;
    std::map<string, string>& hdr = r->getHeaders();
    TCPSocket *sock = (TCPSocket *) userdata;
    
    //if this a keep alive connection from downstream, i.e. client to proxy connection
    if (!sock->willClose()) {
        hdr["connection"] = "keep-alive";
    }
    
    //if the http packet is not chunked, we can send response along writing the buffer cache
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


//callback that is called each time some body data comes in
void onData(httplib::Response* r, void* userdata, char* data, size_t n) {
    
    TCPSocket *sock = (TCPSocket *) userdata;
    filter.filter(data, n);
    
    if(!r->isChunked()) {
        sock->send(data, n);
    }
    
    r->getInternalBuff()->write(data, n);
    
}

//callback that is called when the whole response message is parsed
void onComplete(httplib::Response* r, void* userdata) {
    
    TCPSocket *sock = (TCPSocket *) userdata;
    
    if (r->isChunked()) {
        ostringstream oss;
        std::map<string, string> &hd = r->getHeaders();
        std::map<string, string>::const_iterator itr;
        
        //send the header of the response message
        oss << r->getStatusLine() << "\r\n";
        hd.erase("transfer-encoding");
        hd["content-length"] = to_string(r->bodyLen());
        for (itr = hd.begin(); itr != hd.end(); ++itr) {
            oss << itr->first << ": " << itr->second << "\r\n";
        }
        oss << "\r\n";
        sock->send(oss.str().data(), oss.str().size());
        r->getInternalBuff()->flushToSock(sock);
    } //else {//unchunked has been sent already//}
    
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


//callback called when encounter header field in the buffer
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

//callback called when encounter header value in the buffer
int header_value_cb (http_parser *p, const char *buf, size_t len) {
    
    struct message *msg = (struct message *)p->data;
    string val(buf, len);
    msg->headers[msg->num_headers-1].second = val;
    
    msg->last_header_element = message::VALUE;
    
    return 0;
}

//callbacks for http parser
static http_parser_settings settings = {
    .on_message_begin = EMPTY_CB()
    ,.on_url = request_url_cb
    ,.on_header_field = header_field_cb
    ,.on_header_value = header_value_cb
    ,.on_headers_complete = EMPTY_DATA_CB()
    ,.on_body = EMPTY_DATA_CB()
    ,.on_message_complete = EMPTY_CB()
    ,.on_reason = EMPTY_DATA_CB()
    ,.on_chunk_header = EMPTY_CB()
    ,.on_chunk_complete = EMPTY_CB()
};


void load403Html(const string path) {
    
    map<string, string> hds;
    ostringstream oss;
    std::map<string, string>::const_iterator itr;
    
    std::ifstream file(path, std::ios::binary);
    file.seekg(0, std::ios::end);
    std::streamsize _size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    hds["Connection"] = "keep-alive";
    hds["Content-Type"] = "text/html; charset=iso-8859-1";
    hds["Content-Length"] = to_string(_size);
    
    //send the header of the response message
    oss << DenyResponse << "\r\n";
    for (itr = hds.begin(); itr != hds.end(); ++itr) {
        oss << itr->first << ": " << itr->second << "\r\n";
    }
    oss << "\r\n";
    buff403len = oss.str().length();
    ::memcpy(buff403, oss.str().c_str(), buff403len);
    
    if (file.read(buff403+buff403len, _size)) {
        buff403len += _size;
    }
    
    if (file.is_open()) {
        file.close();
    }
}

int main(int argc, char *argv[]) {
    
    try {
        //main thread loads black list from configure file
        blacklist.loadBlackList("blacklist.json");
        //pre-load the 403 page
        load403Html("403 Forbidden.html");
        //pre-load the insult words that needs to be filtered out
        filter.loadWords("insults.json");
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
    struct message *msg = new message;
    
    //initialize the parser
    http_parser_init(&parser, HTTP_REQUEST);
    parser.data = (void *)msg;
    httplib::Connection *conn;
    
    //start receive request
    try {
        //read the request from the client
        while ((recvMsgSize = sock->recv(buff, RCVBUFSIZE )) > 0) { // Zero means end of transmission
            
            //reset message to begin parsing HTTP request
            msg->headers.clear();
            msg->num_headers = 0;
            msg->last_header_element = message::NONE;
        
            http_parser_execute(&parser, &settings, buff, recvMsgSize);
            
            //handle black list here, can be implemented as a filter chain or plugin.
            //TODO: refactor later
            if(blacklist.isBlocked(msg->host)) {// if the site is block
                sock->send(buff403, buff403len);
                continue;
            }
            
            conn = new httplib::Connection(msg->host, msg->port);
            conn->putRequest("GET", msg->request_path.c_str());
            
            //process the headers of the request that will be used to access original server
            for (int i = 0; i < msg->headers.size(); ++i) {
                
                //if it is keep-alive connection
                if (msg->headers[i].first == "Connection") {
                    if (msg->headers[i].second != "keep-alive") {
                        sock->setWillClose(true);
                    }
                    continue;
                }
                
                //the proxy Host should be strip out
                /*Accept-Encoding: gzip ...  should be strip out because we don't wanna add a large gzip
                  library to our program to uncompress data, filter words and then compress it before send
                  to the browser. Ignore this field.*/
                if (msg->headers[i].first == "Host" || msg->headers[i].first == "Accept-Encoding") {
                    continue;
                }
                
                //add the header field-value pair to request headers to the original server
                conn->addHeader(msg->headers[i].first, msg->headers[i].second);
            }
            
            //filter the connection and host header and replace them with the appropriate values
            conn->addHeader("Connection", "close");
            
            //change callbacks for response data
            conn->setcallbacks(onBegin, onData, onComplete, (void *)sock);
            conn->sendRequest();

            try {
                //process response data
                while(conn->outstanding()) {
                    conn->pump();
                }
            } catch (httplib::Wobbly &wle) {
                cerr << wle.what() << endl;
            }
            
            delete conn;
            
            if (sock->willClose()) {
                break;
            }
        }
    } catch (SocketException &cke) {
        cerr << cke.what() << endl;
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







