//
//  main.cpp
//  HttpProxy
//
//  Created by QiuChusheng on 15/4/16.
//  Copyright (c) 2015年 QiuChusheng. All rights reserved.
//

#include <iostream>
#include "http_parser.h"
#include <string>
using namespace std;
using namespace zproxy;

const char *raw = "GET /test HTTP/1.1\r\n"
"User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
"Host: 0.0.0.0=5000\r\n"
"Accept: */*\r\n"
"\r\n";

int request_url_cb (http_parser *p, const char *buf, size_t len)
{
    char buff[128];
    strncpy(buff, buf, len);
    cout << buff << endl;
    return 0;
}

static http_parser_settings settings =
{    .on_message_begin = EMPTY_CB()
    ,.on_header_field = EMPTY_DATA_CB()
    ,.on_header_value = EMPTY_DATA_CB()
    ,.on_url = EMPTY_DATA_CB()
    ,.on_body = EMPTY_DATA_CB()
    ,.on_headers_complete = EMPTY_DATA_CB()
    ,.on_message_complete = EMPTY_CB()
    ,.on_reason = EMPTY_DATA_CB()
    ,.on_chunk_header = EMPTY_CB()
    ,.on_chunk_complete = EMPTY_CB()
};

int main(int argc, const char * argv[]) {
    
    char buff[1024];
    strcpy(buff, raw);
    http_parser parser;
    struct http_parser_url url;
    
    settings.on_url = request_url_cb;
    
    http_parser_init(&parser, HTTP_REQUEST);
    //http_parser_execute(&parser, &settings, buff, strlen(buff));
    http_parser_parse_url("www.baidu.com/abc/hash?q=123&p=1", sizeof("www.baidu.com/abc/hash?q=123&p=1"), 1, &url);
    
    return 0;
}
//
//
//#include "httplib.h"
//#include <cstdio>
//#include <cstring>
//
//#ifdef WIN32
//#include <winsock2.h>
//#endif // WIN32
//
//int count=0;
//
//void OnBegin( const httplib::Response* r, void* userdata )
//{
//    printf( "BEGIN (%d %s)\n", r->getstatus(), r->getreason() );
//    count = 0;
//}
//
//void OnData( const httplib::Response* r, void* userdata, const unsigned char* data, int n )
//{
//    fwrite( data,1,n, stdout );
//    count += n;
//}
//
//void OnComplete( const httplib::Response* r, void* userdata )
//{
//    printf( "COMPLETE (%d bytes)\n", count );
//}
//
//
//
//void Test1()
//{
//    puts("-----------------Test1------------------------" );
//    // simple simple GET
//    httplib::Connection conn( "www.renren.com", 80 );
//    conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );
//    
//    conn.request("GET", "/", 0, 0,0);
//    
//    while( conn.outstanding() )
//        conn.pump();
//}
//
//
//
//void Test2()
//{
//    puts("-----------------Test2------------------------" );
//    // POST using high-level request interface
//    
//    const char* headers[] =
//    {
//        "Connection", "close",
//        "Content-type", "application/x-www-form-urlencoded",
//        "Accept", "text/plain",
//        0
//    };
//    
//    const char* body = "answer=42&name=Bubba";
//    
//    httplib::Connection conn( "scumways.com", 80 );
//    conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );
//    conn.request( "POST",
//                 "/happyhttp/test.php",
//                 headers,
//                 (const unsigned char*)body,
//                 strlen(body) );
//    
//    while( conn.outstanding() )
//        conn.pump();
//}
//
//void Test3()
//{
//    puts("-----------------Test3------------------------" );
//    // POST example using lower-level interface
//    
//    const char* params = "answer=42&foo=bar";
//    int l = strlen(params);
//    
//    httplib::Connection conn( "scumways.com", 80 );
//    conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );
//    
//    conn.putrequest( "POST", "/happyhttp/test.php" );
//    conn.addHeader( "Connection", "close" );
//    conn.addHeader( "Content-Length", l );
//    conn.addHeader( "Content-type", "application/x-www-form-urlencoded" );
//    conn.addHeader( "Accept", "text/plain" );
//    conn.sendHeader();
//    conn.send( (const unsigned char*)params, l );
//    
//    while( conn.outstanding() )
//        conn.pump();
//}
//
//
//
//
//int main( int argc, char* argv[] )
//{
//#ifdef WIN32
//    WSAData wsaData;
//    int code = WSAStartup(MAKEWORD(1, 1), &wsaData);
//    if( code != 0 )
//    {
//        fprintf(stderr, "shite. %d\n",code);
//        return 0;
//    }
//#endif //WIN32
//    try
//    {
//        Test1();
//        Test2();
//        Test3();
//    }
//    
//    catch( httplib::Wobbly& e )
//    {
//        fprintf(stderr, "Exception:\n%s\n", e.what() );
//    }
//    
//#ifdef WIN32
//    WSACleanup();
//#endif // WIN32
//    
//    return 0;
//}
//
//
//#include "md5.h"
//using namespace std;
//int main () {
//
//    cout << "md5 of 'grape': " << md5("grape") << endl;
//    return 0;
//}