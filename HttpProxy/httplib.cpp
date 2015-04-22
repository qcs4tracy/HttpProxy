
#include "httplib.h"

#ifndef _WIN32
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <errno.h>
	#include <unistd.h>
#else
	#include <WinSock2.h>
	#define vsnprintf _vsnprintf
#endif

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <assert.h>

#include <string>
#include <vector>
#include <string>
#include <algorithm>

#include <sstream>

#ifndef _WIN32
	#define _stricmp strcasecmp
#endif


using namespace std;


namespace httplib
{

#ifdef WIN32
const char* GetWinsockErrorString( int err );
#endif


//---------------------------------------------------------------------
// Helper functions
//---------------------------------------------------------------------

void BailOnSocketError( const char* context )
{
#ifdef WIN32

	int e = WSAGetLastError();
	const char* msg = GetWinsockErrorString( e );
#else
	const char* msg = strerror(errno);
#endif
	throw Wobbly("%s: %s", context, msg);
}


#ifdef WIN32

const char* GetWinsockErrorString( int err )
{
	switch( err)
	{
	case 0:					return "No error";
    case WSAEINTR:			return "Interrupted system call";
    case WSAEBADF:			return "Bad file number";
    case WSAEACCES:			return "Permission denied";
    case WSAEFAULT:			return "Bad address";
    case WSAEINVAL:			return "Invalid argument";
    case WSAEMFILE:			return "Too many open sockets";
    case WSAEWOULDBLOCK:	return "Operation would block";
    case WSAEINPROGRESS:	return "Operation now in progress";
    case WSAEALREADY:		return "Operation already in progress";
    case WSAENOTSOCK:		return "Socket operation on non-socket";
    case WSAEDESTADDRREQ:	return "Destination address required";
    case WSAEMSGSIZE:		return "Message too long";
    case WSAEPROTOTYPE:		return "Protocol wrong type for socket";
    case WSAENOPROTOOPT:	return "Bad protocol option";
	case WSAEPROTONOSUPPORT:	return "Protocol not supported";
	case WSAESOCKTNOSUPPORT:	return "Socket type not supported";
    case WSAEOPNOTSUPP:		return "Operation not supported on socket";
    case WSAEPFNOSUPPORT:	return "Protocol family not supported";
    case WSAEAFNOSUPPORT:	return "Address family not supported";
    case WSAEADDRINUSE:		return "Address already in use";
    case WSAEADDRNOTAVAIL:	return "Can't assign requested address";
    case WSAENETDOWN:		return "Network is down";
    case WSAENETUNREACH:	return "Network is unreachable";
    case WSAENETRESET:		return "Net connection reset";
    case WSAECONNABORTED:	return "Software caused connection abort";
    case WSAECONNRESET:		return "Connection reset by peer";
    case WSAENOBUFS:		return "No buffer space available";
    case WSAEISCONN:		return "Socket is already connected";
    case WSAENOTCONN:		return "Socket is not connected";
    case WSAESHUTDOWN:		return "Can't send after socket shutdown";
    case WSAETOOMANYREFS:	return "Too many references, can't splice";
    case WSAETIMEDOUT:		return "Connection timed out";
    case WSAECONNREFUSED:	return "Connection refused";
    case WSAELOOP:			return "Too many levels of symbolic links";
    case WSAENAMETOOLONG:	return "File name too long";
    case WSAEHOSTDOWN:		return "Host is down";
    case WSAEHOSTUNREACH:	return "No route to host";
    case WSAENOTEMPTY:		return "Directory not empty";
    case WSAEPROCLIM:		return "Too many processes";
    case WSAEUSERS:			return "Too many users";
    case WSAEDQUOT:			return "Disc quota exceeded";
    case WSAESTALE:			return "Stale NFS file handle";
    case WSAEREMOTE:		return "Too many levels of remote in path";
	case WSASYSNOTREADY:	return "Network system is unavailable";
	case WSAVERNOTSUPPORTED:	return "Winsock version out of range";
    case WSANOTINITIALISED:	return "WSAStartup not yet called";
    case WSAEDISCON:		return "Graceful shutdown in progress";
    case WSAHOST_NOT_FOUND:	return "Host not found";
    case WSANO_DATA:		return "No host data of that type was found";
	}

	return "unknown";
};

#endif // WIN32


// return true if socket has data waiting to be read
bool datawaiting(int sock) {
    
	fd_set fds;
	FD_ZERO( &fds );
	FD_SET( sock, &fds );

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	int r = select( sock+1, &fds, NULL, NULL, &tv);
	if (r < 0)
		BailOnSocketError("select");

	if(FD_ISSET( sock, &fds ))
		return true;
	else
		return false;
}


// Try to work out address from string
// returns 0 if bad
struct in_addr *atoaddr(const char* address)
{
	struct hostent *host;
	static struct in_addr saddr;

	//Try dot format xxx.xxx.xxx.xxx
	saddr.s_addr = inet_addr(address);
    if (saddr.s_addr != -1) {
		return &saddr;
    }
    
	host = gethostbyname(address);
    if(host) {
		return (struct in_addr *) *host->h_addr_list;
    }
    
	return NULL;
}
    
// Exception class
//
//---------------------------------------------------------------------

#define _MSG_LEN 512
Wobbly::Wobbly(const char* fmt, ...): std::exception() {
    
	va_list ap;
    char msg_[_MSG_LEN];
    
	va_start( ap,fmt);
	int n = vsnprintf(msg_, _MSG_LEN, fmt, ap);
	va_end( ap );
    
	if(n == _MSG_LEN)
		msg_[_MSG_LEN-1] = '\0';
    
    _msg = new string(msg_);
}

// Connection
Connection::Connection(const char* host, int16_t port) :m_ResponseBeginCB(0), m_ResponseDataCB(0),
	m_ResponseCompleteCB(0), m_UserData(0), m_State( IDLE ),
	m_Host(host), m_Port( port ) {
    
        _sock.connect(host, port);
}
    
Connection::Connection(const string &host, int16_t port): m_Host(host), m_Port(port), m_UserData(0),
    m_State( IDLE ),m_ResponseBeginCB(0), m_ResponseDataCB(0), m_ResponseCompleteCB(0) {
    
        _sock.connect(host, port);
}


void Connection::setcallbacks(ResponseBegin_CB begincb, ResponseData_CB datacb, ResponseComplete_CB completecb, void* userdata ) {
	m_ResponseBeginCB = begincb;
	m_ResponseDataCB = datacb;
	m_ResponseCompleteCB = completecb;
	m_UserData = userdata;
}
    

void Connection::connect() {
    _sock.connect(m_Host, m_Port);
}


void Connection::close() {
    
    _sock.~TCPSocket();
	// discard any incomplete responses
	while( !m_Outstanding.empty() ) {
		delete m_Outstanding.front();
		m_Outstanding.pop_front();
	}
}


Connection::~Connection() { close(); }


void Connection::putRequest(const char* method, const char* url) {
    
    ostringstream req_line;
    
    if( m_State != IDLE ) {
		throw Wobbly("request already issued");
    }
	m_State = REQ_STARTED;
    req_line << method << " " << url << " " << "HTTP/1.1";
    request_line = req_line.str();
	addHeader("Host", m_Host.c_str());//required for HTTP1.1

	// Push a new response onto the queue
	m_Outstanding.push_back(new Response(method, *this));
}
    
/*add `field: value` pair to the request Header*/
void Connection::addHeader(const std::string &header, const std::string &value) {
    
    if (m_State != REQ_STARTED) {
        throw Wobbly("Invalid State to Add Header Field to Request");
    }
    if (headers.count(header) > 0) {
        headers[header] += ";" + value;
    } else {
        headers[header] = value;
    }
    
}

    
void Connection::addHeader(const std::string &header, int numericvalue) {
	addHeader(header, to_string(numericvalue));
}

/*send the Header for Request*/
void Connection::sendHeader() {
    
    ostringstream request_header;
    map<string, string>::const_iterator itr;
    
	if(m_State != REQ_STARTED)
		throw Wobbly( "Invalid State to Send Request Header");

    request_header << request_line << "\r\n";
    //append the headers after request line
    for (itr = headers.begin(); itr != headers.end(); ++itr) {
        request_header << itr->first << ": " << itr->second << "\r\n";
    }
    //end of header (field:value) pairs
    request_header << "\r\n";
    //clear headers
    headers.clear();
    _sock.send((const char*)request_header.str().c_str(), (int)request_header.str().size());
    m_State = REQ_SENT;
    
}

    

void Connection::sendRequest() {
    if (_body && _body_size >0 && headers.count("content-length") <= 0) {
        addHeader("Content-Length", (int)_body_size);
    }
    sendHeader();
    if (_body && _body_size > 0) {
        _sock.send(_body, _body_size);
    }
    m_State = IDLE;
}
    
//    
//    void request(const string &method, const string &url);
//    void appendBody(const char*body, size_t size);
void Connection::request(const string &method, const string &url) {

    putRequest(method.c_str(), url.c_str());
    
    if (_body && _body_size >0 && headers.count("Content-Length") <= 0) {
        addHeader("Content-Length", (int)_body_size);
    }
    
    sendHeader();
    
    if (_body) {
        _sock.send(_body, _body_size);
    }
    
    m_State = IDLE;
    
}
    
void Connection::addHeaders(const std::map<std::string, std::string> headers) {

    std::map<std::string, std::string>::const_iterator itr = headers.begin();
    
    for (; itr != headers.end(); ++itr) {
        addHeader(itr->first, itr->second);
    }
}
    

void Connection::appendBody(const char *body, size_t size) {
    
    _body = body;
    _body_size = size;
}
    

void Connection::pump() {
    
#define BUF_SIZE 2048
    char buf[BUF_SIZE];
    
	if(m_Outstanding.empty())
		return;		// no requests outstanding
    
	if( !datawaiting(_sock.getSockFd()) )
		return;				// recv will block

    ssize_t nrecv_ = _sock.recv(buf, BUF_SIZE);
    Response* rsp;
    
	if(nrecv_ < 0)
		BailOnSocketError("recv()");

	if(nrecv_ == 0) {// connection has closed

		rsp = m_Outstanding.front();
        m_Outstanding.pop_front();
		rsp->notifyConnectionClosed();
		assert(rsp->completed());
		delete rsp;
		// any outstanding requests will be discarded
		close();
        
	} else {//a > 0
        
		size_t nread_ = 0;
        size_t n = 0;
		while(nread_ < nrecv_ && !m_Outstanding.empty()) {

			rsp = m_Outstanding.front();
			n = rsp->pump(&buf[nread_], (nrecv_ - nread_));

			// delete response once completed
			if(rsp->completed()) {
				delete rsp;
				m_Outstanding.pop_front();
			}
			nread_ += n;
		}

		// NOTE: will lose bytes if response queue goes empty
		// (but server shouldn't be sending anything if we don't have
		// anything outstanding anyway)
		assert(nread_ == nrecv_);	// all bytes should be used up by here.
	}
}

    
//---------------------------------------------------------------------
// Response
//---------------------------------------------------------------------
Response::Response(const char* method, Connection& conn) :m_Connection(conn),m_State(STATUSLINE), m_Method(method), m_Version(0), m_Status(0), m_BytesRead(0), m_Chunked(false),
	m_ChunkLeft(0), m_Length(-1), m_WillClose(false), _bufChain(new BufferChain) { }


Response::~Response() {
    _bufChain->freeBuffs();
    delete _bufChain;
}

const char* Response::getheader( const char* name ) const {
    
	std::string lname( name );
#ifdef _MSC_VER
	std::transform( lname.begin(), lname.end(), lname.begin(), tolower );
#else
	std::transform( lname.begin(), lname.end(), lname.begin(), ::tolower );
#endif

	std::map< std::string, std::string >::const_iterator it = m_Headers.find( lname );
	if( it == m_Headers.end() )
		return 0;
	else
		return it->second.c_str();
}


int Response::getstatus() const
{
	// only valid once we've got the statusline
	assert( m_State != STATUSLINE );
	return m_Status;
}


const char* Response::getreason() const
{
	// only valid once we've got the statusline
	assert( m_State != STATUSLINE );
	return m_Reason.c_str();
}



// Connection has closed
void Response::notifyConnectionClosed() {
    
	if(m_State == COMPLETE)
		return;

	// eof can be valid...
	if(m_State == BODY && !m_Chunked && m_Length == -1) {
		Finish();	// we're all done!
	} else {
		throw Wobbly( "Connection closed unexpectedly" );
	}
}



size_t Response::pump(const char* data, size_t datasize) {
    
	assert( datasize != 0 );
	size_t count = datasize;
    size_t n = 0;
    const char *lb =  data;

	while(count > 0 && m_State != COMPLETE) {
        
		if(m_State == STATUSLINE || m_State == HEADERS || m_State == TRAILERS
           || m_State == CHUNKLEN || m_State == CHUNKEND) {
            
            while (count > 0) {
                count--;
                if (*data == '\r') {//a line end
                    data++;
                    if (*data == '\n') {
                        data++;
                        count--;
                        break;
                    }
                    n++;
                } else if(*data++ != '\n') {
                    n++;
                } else {
                    break;
                }
            }
            
            if (n <= 0) {
                lb = data;
                m_LineBuf = "";
            } else {
                //copy to line buffer
                m_LineBuf.assign(lb, n);
            }
            
            
            switch(m_State) {
                    
                case STATUSLINE:
                    ProcessStatusLine(m_LineBuf);
                    break;
                    
                case HEADERS:
                    ProcessHeaderLine(m_LineBuf);
                    break;
                    
                case TRAILERS:
                    ProcessTrailerLine(m_LineBuf);
                    break;
                    
                case CHUNKLEN:
                    ProcessChunkLenLine(m_LineBuf);
                    break;
                    
                case CHUNKEND:
                    //just soak up the crlf after body and go to next state
                    assert(m_Chunked == true);
                    m_State = CHUNKLEN;
                    break;

                default:
                    break;
            }
            
            lb = data;
            n = 0;
            
		} else if(m_State == BODY) {
			size_t nread_ = 0;
			if(m_Chunked)
				nread_ = ProcessDataChunked(data, count);
			else
				nread_ = ProcessDataNonChunked(data, count);
			data += nread_;
			count -= nread_;
		}
	}

	// return number of bytes used
	return datasize - count;
}



void Response::ProcessChunkLenLine(std::string const& line) {
    
	// chunklen in hex at beginning of line
    m_ChunkLeft = (int)strtol(line.c_str(), NULL, 16);
	
	if (m_ChunkLeft == 0) {
		// got the whole body, now check for trailing headers
		m_State = TRAILERS;
		m_HeaderAccum.clear();
	} else {
		m_State = BODY;
	}
}


// handle some body data in chunked mode
// returns number of bytes used.
size_t Response::ProcessDataChunked(const char* data, size_t count) {

    assert( m_Chunked );

	size_t n = count;
	if( n>m_ChunkLeft )
		n = m_ChunkLeft;

	// invoke callback to pass out the data
	if( m_Connection.m_ResponseDataCB )
		(m_Connection.m_ResponseDataCB)(this, m_Connection.m_UserData, data, n);

	m_BytesRead += n;
	m_ChunkLeft -= n;
    
	assert(m_ChunkLeft >= 0);
	if(m_ChunkLeft == 0) {
		// chunk completed! now soak up the trailing CRLF before next chunk
		m_State = CHUNKEND;
	}
	return n;
}

// handle some body data in non-chunked mode.
// returns number of bytes used.
size_t Response::ProcessDataNonChunked(const char* data, size_t count) {
    
	size_t n = count;
	if(m_Length > 0) {
		// we know how many bytes to expect
		size_t remaining = m_Length - m_BytesRead;
		if(n > remaining)
			n = remaining;
	}

	// invoke callback to pass out the data
	if( m_Connection.m_ResponseDataCB )
		(m_Connection.m_ResponseDataCB)(this, m_Connection.m_UserData, data, n);

	m_BytesRead += n;

	// Finish if we know we're done. Else we're waiting for connection close.
	if(m_Length > 0 && m_BytesRead == m_Length)
		Finish();

	return n;
}


void Response::Finish() {
    
	m_State = COMPLETE;
    
	// invoke the callback
	if( m_Connection.m_ResponseCompleteCB )
		(m_Connection.m_ResponseCompleteCB)( this, m_Connection.m_UserData );
}


void Response::ProcessStatusLine(std::string const& line) {
    
	const char* p = line.c_str();

	// skip any leading space
	while (*p && *p == ' ')
		++p;

	// get version
	while (*p && *p != ' ')
		m_VersionString += *p++;
	while (*p && *p == ' ')
		++p;

	// get status code
	std::string status;
	while (*p && *p != ' ')
		status += *p++;
	while (*p && *p == ' ')
		++p;

	// rest of line is reason
	while (*p)
		m_Reason += *p++;

	m_Status = atoi(status.c_str());
	if( m_Status < 100 || m_Status > 999 )
		throw Wobbly("Bad Status Code: %s", status.c_str());
    
    //HTTP protocol version
    if(m_VersionString == "HTTP:/1.0") {
		m_Version = 10;
    } else if(!m_VersionString.compare(0,7,"HTTP/1.")) {
		m_Version = 11;
    } else {
        // TODO: support for HTTP/0.9
		throw Wobbly("Unsupported Protocol: %s", m_VersionString.c_str());
    }
	
    stat_line = line;
	// OK, now we expect headers!
	m_State = HEADERS;
	m_HeaderAccum.clear();
}


// process accumulated header data
void Response::FlushHeader() {
    
	if( m_HeaderAccum.empty() )
		return;	// no flushing required

	const char* p = m_HeaderAccum.c_str();

	std::string header;
	std::string value;
	while( *p && *p != ':' )
		header += tolower( *p++ );

	// skip ':'
	if( *p )
		++p;

	// skip space
	while( *p && (*p ==' ' || *p=='\t') )
		++p;

	value = p; // rest of line is value

	m_Headers[header] = value;
	m_HeaderAccum.clear();
}


void Response::ProcessHeaderLine(std::string const& line) {
    
	const char* p = line.c_str();
	if(line.empty()){
        FlushHeader();
        // HTTP code 100 handling (we ignore 'em)
        if(m_Status == CONTINUE) {
			m_State = STATUSLINE;	// reset parsing, expect new status line
        } else {
            BeginBody();			// start on body now!
        }
		return;
	}

	if(isspace(*p)){
		// it's a continuation line - just add it to previous data
		++p;
		while( *p && isspace( *p ) )
			++p;
        
		m_HeaderAccum += ' ';
		m_HeaderAccum += p;
	} else {
		// begin a new header
		FlushHeader();
		m_HeaderAccum = p;
	}
}


void Response::ProcessTrailerLine(std::string const& line)
{
	// TODO: handle trailers
	if( line.empty() )
		Finish();
	// just ignore all the trailers...
}



// OK, we've now got all the headers read in, so we're ready to start
// on the body. But we need to see what info we can glean from the headers
// first...
void Response::BeginBody() {

	m_Chunked = false;
	m_Length = -1;	// unknown
	m_WillClose = false;

	// using chunked encoding?
	const char* trenc = getheader("transfer-encoding");
	if(trenc && !_stricmp(trenc, "chunked")) {
		m_Chunked = true;
		m_ChunkLeft = -1;//unknown
	}

	m_WillClose = CheckClose();

	// length supplied?
	const char* contentlen = getheader("content-length");
	if( contentlen && !m_Chunked ){
		m_Length = atoi(contentlen);
	}

	// check for various cases where we expect zero-length body
	if(m_Status == NO_CONTENT ||
		m_Status == NOT_MODIFIED ||
		( m_Status >= 100 && m_Status < 200) ||		// 1xx codes have no body
		m_Method == "HEAD" ){
		m_Length = 0;
	}


	// if we're not using chunked mode, and no length has been specified,
	// assume connection will close at end.
	if( !m_WillClose && !m_Chunked && m_Length == -1 )
		m_WillClose = true;

	// Invoke the user callback, if any
	if( m_Connection.m_ResponseBeginCB )
		(m_Connection.m_ResponseBeginCB)(this, m_Connection.m_UserData);

	// now start reading body data!
	if(m_Chunked)
		m_State = CHUNKLEN;
	else
		m_State = BODY;
}


// return true if we think server will automatically close connection
bool Response::CheckClose() {
    
	if(m_Version == 11) {
		// HTTP1.1
		// the connection stays open unless "connection: close" is specified.
		const char* conn = getheader( "connection" );
		if(conn && !_stricmp( conn, "close" ))
			return true;
		else
			return false;
	}

	// Older HTTP
	// keep-alive header indicates persistant connection 
	if(getheader( "keep-alive" ))
		return false;

    return true;
}


}// end namespace


