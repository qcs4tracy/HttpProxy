
#ifndef HTTPLIB_H
#define HTTPLIB_H


#include <string>
#include <map>
#include <vector>
#include <deque>
#include "socket.h"
#include "mem_alloc.h"

// forward decl
struct in_addr;

namespace httplib {

class Response;

// Helper Functions
void BailOnSocketError(const char* context);
struct in_addr *atoaddr(const char* address);


typedef void (*ResponseBegin_CB)(Response* r, void* userdata);
typedef void (*ResponseData_CB)(Response* r, void* userdata, char* data, size_t numbytes);
typedef void (*ResponseComplete_CB)(Response* r, void* userdata);


// HTTP status codes
enum {
	// 1xx informational
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,
	PROCESSING = 102,

	// 2xx successful
	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,
	MULTI_STATUS = 207,
	IM_USED = 226,

	// 3xx redirection
	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305,
	TEMPORARY_REDIRECT = 307,
	
	// 4xx client error
	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	PROXY_AUTHENTICATION_REQUIRED = 407,
	REQUEST_TIMEOUT = 408,
	CONFLICT = 409,
	GONE = 410,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	REQUEST_ENTITY_TOO_LARGE = 413,
	REQUEST_URI_TOO_LONG = 414,
	UNSUPPORTED_MEDIA_TYPE = 415,
	REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,
	UNPROCESSABLE_ENTITY = 422,
	LOCKED = 423,
	FAILED_DEPENDENCY = 424,
	UPGRADE_REQUIRED = 426,

	// 5xx server error
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	INSUFFICIENT_STORAGE = 507,
	NOT_EXTENDED = 510,
};

    
#define DenyResponse "HTTP/1.1 403 Forbidden"
#define DenyResponseLen sizeof(DenyResponse)


// Exception class
class Wobbly : public exception
{
public:
    Wobbly(const char* fmt, ...);
    virtual ~Wobbly() { delete _msg; }
    virtual const char* what() const throw() { return _msg->c_str(); }
    
protected:
    string *_msg;
};



//-------------------------------------------------
// Connection
//
// Handles the socket connection, issuing of requests and managing
// responses.
// ------------------------------------------------

class Connection {
    
	friend class Response;
public:
	// doesn't connect immediately
	Connection(const char* host, int16_t port);
    Connection(const string &host, int16_t port);
	virtual ~Connection();

	// Set up the response handling callbacks. These will be invoked during
	// calls to pump().
	// begincb		- called when the responses headers have been received
	// datacb		- called repeatedly to handle body data
	// completecb	- response is completed
	// userdata is passed as a param to all callbacks.
	void setcallbacks(
		ResponseBegin_CB begincb,
		ResponseData_CB datacb,
		ResponseComplete_CB completecb,
		void* userdata );

	// Don't need to call connect() explicitly as issuing a request will
	// call it automatically if needed.
	// But it could block (for name lookup etc), so you might prefer to
	// call it in advance.
	void connect();

	// close connection, discarding any pending requests.
	void close();

	// Update the connection (non-blocking)
	// Just keep calling this regularly to service outstanding requests.
	void pump();

	// any requests still outstanding?
	bool outstanding() const
		{ return !m_Outstanding.empty(); }

	// ---------------------------
	// high-level request interface
	// ---------------------------
	
	// method is "GET", "POST" etc...
	// url is only path part: eg  "/index.html"
	// headers is array of name/value pairs, terminated by a null-ptr
	// body & bodysize specify body data of request (eg values for a form)
    void request(const string &method, const string &url);
    void appendBody(const char*body, size_t size);
    void sendRequest();
    

	// ---------------------------
	// low-level request interface
	// ---------------------------

	// begin request
	// method is "GET", "POST" etc...
	// url is only path part: eg  "/index.html"
	void putRequest(const char* method, const char* url);

	// Add a header to the request (call after putrequest() )
	void addHeader(const std::string &header, int numericvalue );	// alternate version
    void addHeader(const std::string &header, const std::string &value);
    void addHeaders(const std::map<std::string, std::string> headers);

	// Finished adding headers, issue the request.
	void sendHeader();

protected:
	// some bits of implementation exposed to Response class

	// callbacks
	ResponseBegin_CB	m_ResponseBeginCB;
	ResponseData_CB		m_ResponseDataCB;
	ResponseComplete_CB	m_ResponseCompleteCB;
	void*				m_UserData;

private:
	enum { IDLE, REQ_STARTED, REQ_SENT } m_State;
	std::string m_Host;
	int16_t m_Port;
    const char *_body = NULL;
    size_t _body_size = 0;
    TCPSocket _sock;
    map<string, string> headers;
    
	string request_line;	// request line
	std::deque<Response*> m_Outstanding;	// responses for outstanding requests
};

    
//-------------------------------------------------
// Response:
// Handles parsing of response data.
// ------------------------------------------------

class Response {
    
	friend class Connection;
public:

	// retrieve a header (returns 0 if not present)
	const char* getheader( const char* name ) const;

	bool completed() const { return m_State == COMPLETE; }
    
	// get the HTTP status code
	int getstatus() const;

	// get the HTTP response reason string
	const char* getreason() const;

	// true if connection is expected to close after this response.
	bool willclose() const { return m_WillClose; }
    
    const string &getStatusLine() const { return stat_line; }
    std::map<std::string, std::string> &getHeaders() { return m_Headers; }
    
    const bool isChunked() const { return m_Chunked; }
    
    BufferChain *getInternalBuff() { return _bufChain; }
    
    size_t bodyLen() { return m_BytesRead; }
    
    virtual ~Response();

protected:
	// only Connection creates Responses.
    Response(const char* method, Connection& conn);

	// pump some data in for processing.
	// Returns the number of bytes used.
	// Will always return 0 when response is complete.
	size_t pump(const char* data, size_t datasize);

	// tell response that connection has closed
	void notifyConnectionClosed();
    
    
private:
	enum {
		STATUSLINE,		// start here. status line is first line of response.
		HEADERS,		// reading in header lines
		BODY,			// waiting for some body data (all or a chunk)
		CHUNKLEN,		// expecting a chunk length indicator (in hex)
		CHUNKEND,		// got the chunk, now expecting a trailing blank line
		TRAILERS,		// reading trailers after body.
		COMPLETE,		// response is complete!
	} m_State;

	Connection& m_Connection;	// to access callback ptrs
	std::string m_Method;		// req method: "GET", "POST" etc...

	// status line
    std::string stat_line;
	std::string	m_VersionString;	// HTTP-Version
	int	m_Version;			// 10: HTTP/1.0    11: HTTP/1.x (where x>=1)
	int m_Status;			// Status-Code
	std::string m_Reason;	// Reason-Phrase

	// header/value pairs
	std::map<std::string,std::string> m_Headers;

	size_t	m_BytesRead;		// body bytes read so far
	bool	m_Chunked;			// response is chunked?
	int		m_ChunkLeft;		// bytes left in current chunk
	int		m_Length;			// -1 if unknown
	bool	m_WillClose;		// connection will close at response end?

	std::string m_LineBuf;		// line accumulation for states that want it
	std::string m_HeaderAccum;	// accumulation buffer for headers
    
    //buffer for the content
    BufferChain *_bufChain;
    
	void FlushHeader();
	void ProcessStatusLine(std::string const& line);
	void ProcessHeaderLine(std::string const& line);
	void ProcessTrailerLine(std::string const& line);
	void ProcessChunkLenLine(std::string const& line);

	size_t ProcessDataChunked(const char* data, size_t count );
	size_t ProcessDataNonChunked(const char* data, size_t count );

	void BeginBody();
	bool CheckClose();
	void Finish();
};



}//End namespace httplib


#endif // HTTPLIB_H_


