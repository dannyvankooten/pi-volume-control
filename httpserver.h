/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* MIT License
* 
* Copyright (c) 2019 Jeremy Williams
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* httpserver.h (0.6.0)
*
* Description:
*
*   A single header C library for building non-blocking event driven HTTP
*   servers
*
* Usage:
*
*   Do this:
*      #define HTTPSERVER_IMPL
*   before you include this file in *one* C or C++ file to create the
*   implementation.
*
*   // i.e. it should look like this:
*   #include ...
*   #include ...
*   #include ...
*   #define HTTPSERVER_IMPL
*   #include "httpserver.h"
*
*   There are some #defines that can be configured. This must be done in the
*   same file that you define HTTPSERVER_IMPL These defines have default values
*   and will need to be #undef'd and redefined to configure them.
*
*     HTTP_REQUEST_BUF_SIZE - default 1024 - The initial size in bytes of the
*       read buffer for the request. This buffer grows automatically if it's
*       capacity is reached but it certain environments it may be optimal to
*       change this value.
*
*     HTTP_RESPONSE_BUF_SIZE - default 512 - Same as above except for the
*       response buffer.
*
*     HTTP_REQUEST_TIMEOUT - default 20 - The amount of seconds the request will
*       wait for activity on the socket before closing. This only applies mid
*       request. For the amount of time to hold onto keep-alive connections see
*       below.
*
*     HTTP_KEEP_ALIVE_TIMEOUT - default 120 - The amount of seconds to keep a
*       connection alive a keep-alive request has completed.
*
*     HTTP_MAX_CONTENT_LENGTH - default 8388608 (8MB) - The max size in bytes
*       of the request content length. It should be noted that the request body
*       will be fully read into memory so while this could be redefined to a
*       value as large as INT_MAX it will allocate a lot of memory. I would
*       reccommend using chunked encoding for large requests.
*
*     HTTP_MAX_TOTAL_EST_MEM_USAGE - default 4294967296 (4GB) - This is the
*       amount of read/write buffer space that is allowed to be allocated across
*       all requests before new requests will get 503 responses.
*
*     HTTP_MAX_TOKEN_LENGTH - default 8192 (8KB) - This is the max size of any
*       non body http tokens. i.e: header names, header values, url length, etc.
*
*   For more details see the documentation of the interface and the example
*   below.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#ifdef __cplusplus
extern "C" {
#endif

// String type used to read the request details. The char pointer is NOT null
// terminated.
struct http_string_s {
  char const * buf;
  int len;
};

struct http_server_s;
struct http_request_s;
struct http_response_s;

// Returns the event loop id that the server is running on. This will be an
// epoll fd when running on Linux or a kqueue on BSD. This can be used to
// listen for activity on sockets, etc. The only caveat is that the user data
// must be set to a struct where the first member is the function pointer to
// a callback that will handle the event. i.e:
//
// For kevent:
//
//   struct foo {
//     void (*handler)(struct kevent*);
//     ...
//   }
//
//   // Set ev.udata to a foo pointer when registering the event.
//
// For epoll:
//
//   struct foo {
//     void (*handler)(struct epoll_event*);
//     ...
//   }
//
//   // Set ev.data.ptr to a foo pointer when registering the event.
int http_server_loop(struct http_server_s* server);

// Allocates and initializes the http server. Takes a port and a function
// pointer that is called to process requests.
struct http_server_s* http_server_init(int port, void (*handler)(struct http_request_s*));

// Starts the event loop and the server listening. During normal operation this
// function will not return. Return value is the error code if the server fails
// to start.
int http_server_listen(struct http_server_s* server);

// Use this listen call in place of the one above when you want to integrate
// an http server into an existing application that has a loop already and you
// want to use the polling functionality instead. This works well for
// applications like games that have a constant update loop.
int http_server_listen_poll(struct http_server_s* server);

// Call this function in your update loop. It will trigger the request handler
// once if there is a request ready. Returns 1 if a request was handled and 0
// if no requests were handled. It should be called in a loop until it returns
// 0.
int http_server_poll(struct http_server_s* server);

// Returns the request method as it was read from the HTTP request line.
struct http_string_s http_request_method(struct http_request_s* request);

// Returns the full request target (url) as it was read from the HTTP request
// line.
struct http_string_s http_request_target(struct http_request_s* request);

// Returns the request body. If no request body was sent buf and len of the
// string will be set to 0.
struct http_string_s http_request_body(struct http_request_s* request);

// Returns the request header value for the given header key. The key is case
// insensitive.
struct http_string_s http_request_header(struct http_request_s* request, char const * key);

// Procedure used to iterate over all the request headers. iter should be
// initialized to zero before calling. Each call will set key and val to the
// key and value of the next header. Returns 0 when there are no more headers.
int http_request_iterate_headers(
  struct http_request_s* request,
  struct http_string_s* key,
  struct http_string_s* val,
  int* iter
);

// Retrieve the opaque data pointer.
void* http_request_userdata(struct http_request_s* request);

// Stores a pointer for future retrieval. This is not used by the library in
// any way and is strictly for you, the application programmer to make use
// of.
void http_request_set_userdata(struct http_request_s* request, void* data);

#define HTTP_KEEP_ALIVE 1
#define HTTP_CLOSE 0

// By default the server will inspect the Connection header and the HTTP
// version to determine whether the connection should be kept alive or not.
// Use this function to override that behaviour to force the connection to
// keep-alive or close by passing in the HTTP_KEEP_ALIVE or HTTP_CLOSE
// directives respectively. This may provide a minor performance improvement
// in cases where you control client and server and want to always close or
// keep the connection alive.
void http_request_connection(struct http_request_s* request, int directive);

// When reading in the HTTP request the server allocates a buffer to store
// the request details such as the headers, method, body, etc. By default this
// memory will be freed when http_respond is called. This function lets you
// free that memory before the http_respond call. This can be useful if you
// have requests that take a long time to complete and you don't require the
// request data. Accessing any http_string_s's will be invalid after this call.
void http_request_free_buffer(struct http_request_s* request);

// Allocates an http response. This memory will be freed when http_respond is
// called.
struct http_response_s* http_response_init();

// Set the response status. Accepts values between 100 and 599 inclusive. Any
// other value will map to 500.
void http_response_status(struct http_response_s* response, int status);

// Set a response header. Takes two null terminated strings.
void http_response_header(struct http_response_s* response, char const * key, char const * value);

// Set the response body. The caller is responsible for freeing any memory that
// may have been allocated for the body. It is safe to free this memory AFTER
// http_respond has been called.
void http_response_body(struct http_response_s* response, char const * body, int length);

// Starts writing the response to the client. Any memory allocated for the
// response body or response headers is safe to free after this call.
void http_respond(struct http_request_s* request, struct http_response_s* response);

// Writes a chunk to the client. The notify_done callback will be called when
// the write is complete. This call consumes the response so a new response
// will need to be initialized for each chunk. The response status of the
// request will be the response status that is set when http_respond_chunk is
// called the first time. Any headers set for the first call will be sent as
// the response headers. Headers set for subsequent calls will be ignored.
void http_respond_chunk(
  struct http_request_s* request,
  struct http_response_s* response,
  void (*notify_done)(struct http_request_s*)
);

// Ends the chunked response. Any headers set before this call will be included
// as what the HTTP spec refers to as 'trailers' which are essentially more
// response headers.
void http_respond_chunk_end(struct http_request_s* request, struct http_response_s* response);

// If a request has Transfer-Encoding: chunked you cannot read the body in the
// typical way. Instead you need to call this function to read one chunk at a
// time. You pass a callback that will be called when the chunk is ready. When
// the callback is called you can use `http_request_chunk` to get the current
// chunk. When done with that chunk call this function again to request the
// next chunk. If the chunk has size 0 then the request body has been completely
// read and you can now respond.
void http_request_read_chunk(
  struct http_request_s* request,
  void (*chunk_cb)(struct http_request_s*)
);

// Returns the current chunk of the request body. This chunk is only valid until
// the next call to `http_request_read_chunk`.
struct http_string_s http_request_chunk(struct http_request_s* request);

#ifdef __cplusplus
}
#endif

// Minimal example usage.
#ifdef HTTPSERVER_EXAMPLE

#define RESPONSE "Hello, World!"

void handle_request(struct http_request_s* request) {
  struct http_response_s* response = http_response_init();
  http_response_status(response, 200);
  http_response_header(response, "Content-Type", "text/plain");
  http_response_body(response, RESPONSE, sizeof(RESPONSE) - 1);
  http_respond(request, response);
}

int main() {
  struct http_server_s* server = http_server_init(8080, handle_request);
  http_server_listen(server);
}

#endif

#endif

#ifdef HTTPSERVER_IMPL
#ifndef HTTPSERVER_IMPL_ONCE
#define HTTPSERVER_IMPL_ONCE

#ifdef __linux__
#define EPOLL
#define _POSIX_C_SOURCE 199309L
#else
#define KQUEUE
#endif

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <limits.h>
#include <assert.h>

#ifdef KQUEUE
#include <sys/event.h>
#else
#include <sys/epoll.h>
#include <sys/timerfd.h>
#endif

// *** macro definitions

// Application configurable
#define HTTP_REQUEST_BUF_SIZE 1024
#define HTTP_RESPONSE_BUF_SIZE 512
#define HTTP_REQUEST_TIMEOUT 20
#define HTTP_KEEP_ALIVE_TIMEOUT 120
#define HTTP_MAX_CONTENT_LENGTH 8388608 // 8mb
#define HTTP_MAX_TOKEN_LENGTH 8192 // 8kb
#define HTTP_MAX_TOTAL_EST_MEM_USAGE 4294967296 // 4gb

#define HTTP_MAX_HEADER_COUNT 127

#define HTTP_FLAG_SET(var, flag) var |= flag
#define HTTP_FLAG_CLEAR(var, flag) var &= ~flag
#define HTTP_FLAG_CHECK(var, flag) (var & flag)

// token types / parser states
#define HTTP_METHOD 0
#define HTTP_TARGET 1
#define HTTP_VERSION 2
#define HTTP_HEADER_KEY 3
#define HTTP_HEADER_VALUE 4
#define HTTP_HEADER_END 5
#define HTTP_NONE 6
#define HTTP_BODY 7
#define HTTP_PARSE_ERROR 13

// error sub types
#define HTTP_ERR_PAYLOAD_TOO_LARGE 0
#define HTTP_ERR_BAD_REQUEST 1

// chunked token types / parser state
#define HTTP_CHUNK_SIZE 8
#define HTTP_CHUNK_EXTN 9
#define HTTP_CHUNK_BODY 10
#define HTTP_CHUNK_BODY_END 11
#define HTTP_CHUNK_BODY_PARTIAL 12

// parser flags
#define HS_PF_TRANSFER_ENCODING 0x1
#define HS_PF_CONTENT_LENGTH 0x2
#define HS_PF_CHUNKED 0x4

// parser sub states
#define HTTP_LWS 2
#define HTTP_CR 3
#define HTTP_CRLF 4

// parser comparisons
#define HS_CONTENT_LENGTH_LOW "content-length"
#define HS_CONTENT_LENGTH_UP "CONTENT-LENGTH"
#define HS_TRANSFER_ENCODING_LOW "transfer-encoding"
#define HS_TRANSFER_ENCODING_UP "TRANSFER-ENCODING"
#define HS_CHUNKED_LOW "chunked"
#define HS_CHUNKED_UP "CHUNKED"

// parser sentinel lengths
#define HTTP_CHUNKED_LEN -1

// http session states
#define HTTP_SESSION_INIT 0
#define HTTP_SESSION_READ_HEADERS 1
#define HTTP_SESSION_READ_BODY 2
#define HTTP_SESSION_WRITE 3
#define HTTP_SESSION_READ_CHUNK 4
#define HTTP_SESSION_NOP 5

// http session flags
#define HTTP_RESPONSE_READY 0x4
#define HTTP_AUTOMATIC 0x8
#define HTTP_RESPONSE_PAUSED 0x10
#define HTTP_CHUNKED_RESPONSE 0x20

// http version indicators
#define HTTP_1_0 0
#define HTTP_1_1 1

// *** declarations ***

// structs

typedef struct {
  int index;
  int len;
  int type;
} http_token_t;

typedef struct {
  int content_length;
  int len;
  int token_start_index;
  int start;
  int body_start_index;
  char header_count;
  char content_length_i;
  char transfer_encoding_i;
  char flags;
  char state;
  char sub_state;
} http_parser_t;

typedef struct {
  http_token_t* buf;
  int capacity;
  int size;
} http_token_dyn_t;

#ifdef EPOLL
typedef void (*epoll_cb_t)(struct epoll_event*);
#endif

typedef struct http_ev_cb_s {
#ifdef KQUEUE
  void (*handler)(struct kevent* ev);
#else
  epoll_cb_t handler;
#endif
} ev_cb_t;

typedef struct http_request_s {
#ifdef KQUEUE
  void (*handler)(struct kevent* ev);
#else
  epoll_cb_t handler;
  epoll_cb_t timer_handler;
  int timerfd;
#endif
  void (*chunk_cb)(struct http_request_s*);
  void* data;
  http_parser_t parser;
  int state;
  int socket;
  char* buf;
  int bytes;
  int written;
  int capacity;
  int timeout;
  struct http_server_s* server;
  http_token_t token;
  http_token_dyn_t tokens;
  char flags;
} http_request_t;

typedef struct http_server_s {
#ifdef KQUEUE
  void (*handler)(struct kevent* ev);
#else
  epoll_cb_t handler;
  epoll_cb_t timer_handler;
#endif
  long memused;
  int socket;
  int port;
  int loop;
  int timerfd;
  socklen_t len;
  void (*request_handler)(http_request_t*);
  struct sockaddr_in addr;
  char* date;
} http_server_t;

typedef struct http_header_s {
  char const * key;
  char const * value;
  struct http_header_s* next;
} http_header_t;

typedef struct http_response_s {
  http_header_t* headers;
  char const * body;
  int content_length;
  int status;
} http_response_t;

typedef struct http_string_s http_string_t;

// prototypes

void hs_add_server_sock_events(struct http_server_s* serv);
void hs_server_init(struct http_server_s* serv);
void hs_delete_events(struct http_request_s* request);
void hs_add_events(struct http_request_s* request);
void hs_add_write_event(struct http_request_s* request);

void hs_exec_response_handler(http_request_t* request, void (*handler)(http_request_t*));

#ifdef KQUEUE

void hs_server_listen_cb(struct kevent* ev);
void hs_session_io_cb(struct kevent* ev);

#else

void hs_server_listen_cb(struct epoll_event* ev);
void hs_session_io_cb(struct epoll_event* ev);
void hs_server_timer_cb(struct epoll_event* ev);
void hs_request_timer_cb(struct epoll_event* ev);

#endif

// constants

char const * hs_status_text[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  
  //100s
  "Continue", "Switching Protocols", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",

  //200s
  "OK", "Created", "Accepted", "Non-Authoritative Information", "No Content",
  "Reset Content", "Partial Content", "", "", "",

  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",

  //300s
  "Multiple Choices", "Moved Permanently", "Found", "See Other", "Not Modified",
  "Use Proxy", "", "Temporary Redirect", "", "",

  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",

  //400s
  "Bad Request", "Unauthorized", "Payment Required", "Forbidden", "Not Found",
  "Method Not Allowed", "Not Acceptable", "Proxy Authentication Required",
  "Request Timeout", "Conflict",

  "Gone", "Length Required", "", "Payload Too Large", "", "", "", "", "", "",

  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",

  //500s
  "Internal Server Error", "Not Implemented", "Bad Gateway", "Service Unavailable",
  "Gateway Timeout", "", "", "", "", ""

  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", ""
};

// *** http parser ***

#define HS_P_MATCH_HEADER(up, low, i) \
  if ((c == up[(int)i] || c == low[(int)i]) && i < (char)(sizeof(up) - 1)) i++;

http_token_t hs_parse_error(http_parser_t* parser, int subtype) {
  parser->len = 0;
  parser->state = HTTP_PARSE_ERROR;
  http_token_t token;
  token.index = subtype;
  token.type = HTTP_PARSE_ERROR;
  return token;
}

http_token_t http_parse(http_parser_t* parser, char* input, int n) {
  for (int i = parser->start; i < n; ++i, parser->start = i + 1, parser->len++) {
    char c = input[i];
    switch (parser->state) {
      case HTTP_METHOD:
        if (c == ' ') {
          http_token_t token;
          token.index = parser->token_start_index;
          token.type = parser->state;
          token.len = parser->len;
          parser->state = HTTP_TARGET;
          parser->len = 0;
          parser->token_start_index = i + 1;
          return token;
        }
        break;
      case HTTP_TARGET:
        if (c == ' ') {
          http_token_t token;
          token.index = parser->token_start_index;
          token.type = parser->state;
          token.len = parser->len;
          parser->state = HTTP_VERSION;
          parser->token_start_index = i + 1;
          parser->len = 0;
          return token;
        }
        break;
      case HTTP_VERSION:
        if (c == '\r') {
          parser->sub_state = HTTP_CR;
          http_token_t token;
          token.index = parser->token_start_index;
          token.type = HTTP_VERSION;
          token.len = parser->len;
          return token;
        } else if (parser->sub_state == HTTP_CR && c == '\n') {
          parser->sub_state = 0;
          parser->len = 0;
          parser->token_start_index = i + 1;
          parser->state = HTTP_HEADER_KEY;
        }
        break;
      case HTTP_HEADER_KEY:
        if (c == ':') {
          parser->state = HTTP_HEADER_VALUE;
          parser->sub_state = HTTP_LWS;
          if (parser->len == parser->content_length_i + 1) {
            HTTP_FLAG_SET(parser->flags, HS_PF_CONTENT_LENGTH);
          } else if (parser->len == parser->transfer_encoding_i + 1) {
            HTTP_FLAG_SET(parser->flags, HS_PF_TRANSFER_ENCODING);
          }
          parser->content_length_i = 0;
          parser->transfer_encoding_i = 0;
          http_token_t token;
          token.index = parser->token_start_index;
          token.type = HTTP_HEADER_KEY;
          token.len = parser->len - 1;
          return token;
        }
        HS_P_MATCH_HEADER(
          HS_CONTENT_LENGTH_UP,
          HS_CONTENT_LENGTH_LOW,
          parser->content_length_i
        )
        HS_P_MATCH_HEADER(
          HS_TRANSFER_ENCODING_UP,
          HS_TRANSFER_ENCODING_LOW,
          parser->transfer_encoding_i
        )
        break;
      case HTTP_HEADER_VALUE:
        if (parser->sub_state == HTTP_LWS && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
          continue;
        } else if (parser->sub_state == HTTP_LWS) {
          parser->sub_state = 0;
          parser->len = 0;
          parser->token_start_index = i;
          if (HTTP_FLAG_CHECK(parser->flags, HS_PF_CONTENT_LENGTH)) {
            parser->content_length *= 10;
            parser->content_length += c - '0';
          } else if (HTTP_FLAG_CHECK(parser->flags, HS_PF_TRANSFER_ENCODING)) {
            HS_P_MATCH_HEADER(HS_CHUNKED_UP, HS_CHUNKED_LOW, parser->transfer_encoding_i)
          }
        } else if (parser->sub_state != HTTP_LWS && c == '\r') {
          parser->sub_state = HTTP_CR;
          parser->state = HTTP_HEADER_END;
          HTTP_FLAG_CLEAR(parser->flags, HS_PF_TRANSFER_ENCODING);
          HTTP_FLAG_CLEAR(parser->flags, HS_PF_CONTENT_LENGTH);
          if (parser->len == parser->transfer_encoding_i) {
            HTTP_FLAG_SET(parser->flags, HS_PF_CHUNKED);
          }
          parser->transfer_encoding_i = 0;
          if (parser->header_count == HTTP_MAX_HEADER_COUNT) {
            return hs_parse_error(parser, HTTP_ERR_BAD_REQUEST);
          }
          parser->header_count++;
          http_token_t token;
          token.index = parser->token_start_index;
          token.type = HTTP_HEADER_VALUE;
          token.len = parser->len;
          return token;
        } else if (HTTP_FLAG_CHECK(parser->flags, HS_PF_CONTENT_LENGTH)) {
          int64_t new_content_length = parser->content_length * 10l;
          new_content_length += c - '0';
          if (new_content_length > HTTP_MAX_CONTENT_LENGTH) {
            return hs_parse_error(parser, HTTP_ERR_PAYLOAD_TOO_LARGE);
          } else {
            parser->content_length = (int)new_content_length;
          }
        } else if (HTTP_FLAG_CHECK(parser->flags, HS_PF_TRANSFER_ENCODING)) {
          HS_P_MATCH_HEADER(HS_CHUNKED_UP, HS_CHUNKED_LOW, parser->transfer_encoding_i)
        }
        break;
      case HTTP_HEADER_END:
        if (parser->sub_state == 0 && c == '\r') {
          parser->sub_state = HTTP_CR;
        } else if (parser->sub_state == HTTP_CR && c == '\n') {
          parser->sub_state = HTTP_CRLF;
        } else if (parser->sub_state == HTTP_CRLF && c == '\r') {
          parser->sub_state = 0;
          parser->state = HTTP_BODY;
          http_token_t token;
          token.index = i + 2;
          parser->body_start_index = token.index;
          token.type = HTTP_BODY;
          if (HTTP_FLAG_CHECK(parser->flags, HS_PF_CHUNKED)) {
            token.len = HTTP_CHUNKED_LEN;
          } else {
            token.len = parser->content_length;
          }
          parser->start++;
          return token;
        } else if (parser->sub_state == HTTP_CRLF && c != '\r') {
          parser->sub_state = 0;
          parser->len = 0;
          parser->token_start_index = i;
          i--;
          parser->state = HTTP_HEADER_KEY;
        }
        break;
    }
    if (parser->len >= HTTP_MAX_TOKEN_LENGTH && parser->state != HTTP_BODY) {
      return hs_parse_error(parser, HTTP_ERR_BAD_REQUEST);
    }
  }
  http_token_t token = { 0, 0, 0 };
  token.type = HTTP_NONE;
  return token;
}

// When we detect that the full chunk exists in the read buffer we can
// immediately emit the HTTP_CHUNK_BODY token and skip reading forward to the
// end of the chunk
http_token_t http_gen_body_token(http_parser_t* parser) {
  http_token_t token;
  token.index = parser->token_start_index;
  token.type = HTTP_CHUNK_BODY;
  token.len = parser->content_length;
  parser->start = parser->token_start_index + parser->content_length;
  parser->len = parser->content_length;
  parser->state = HTTP_CHUNK_BODY_END;
  return token;
}

http_token_t http_chunk_parse(http_request_t* request, char* input, int n) {
  http_parser_t* parser = &request->parser;
  for (int i = parser->start; i < n; ++i, parser->start = i + 1, parser->len++) {
    char c = input[i];
    int remaining = n - (i + 1);
    switch (parser->state) {
      case HTTP_CHUNK_SIZE:
        if (c == ';') {
          parser->state = HTTP_CHUNK_EXTN;
        } else if (c == '\n') {
          parser->token_start_index = i + 1;
          parser->len = 0;
          if (remaining >= parser->content_length) {
            // Full chunk exists in buffer
            return http_gen_body_token(parser);
          }
          parser->state = HTTP_CHUNK_BODY;
        } else if (c == '\r') {
          break;
        } else if (c >= 'A' && c <= 'F') {
          parser->content_length *= 0x10;
          parser->content_length += c - 55;
        } else if (c >= 'a' && c <= 'f') {
          parser->content_length *= 0x10;
          parser->content_length += c - 87;
        } else if (c >= '0' && c <= '9') {
          parser->content_length *= 0x10;
          parser->content_length += c - '0';
        }
        break;
      case HTTP_CHUNK_EXTN:
        if (c == '\n') {
          if (remaining >= parser->content_length) {
            // Full chunk exists in buffer
            parser->token_start_index = i + 1;
            return http_gen_body_token(parser);
          }
          parser->token_start_index = i + 1;
          parser->state = HTTP_CHUNK_BODY;
        }
        break;
      case HTTP_CHUNK_BODY:
        if (remaining >= parser->content_length) {
          // The remaining portion of the chunk exists in the read buffer
          return http_gen_body_token(parser);
        }
        break;
      case HTTP_CHUNK_BODY_END:
        if (c == '\n') {
          parser->state = HTTP_CHUNK_SIZE;
          parser->content_length = 0;
          parser->len = 0;
          parser->token_start_index = i + 1;
        }
        break;
    }
  }
  // This code is reached when we come to the end of our read buffer and no
  // token has been emitted during this call. If we are part way through parsing
  // a token at the end of the buffer or the next token would require us to grow
  // the buffer then we want to reset the parser to overwrite old chunks so that
  // we don't need to grow the buffer.
  if (parser->token_start_index != parser->body_start_index) {
    // Next time, start parsing as if the current token has been shifted to the
    // start of the http body.
    parser->start = parser->body_start_index + parser->len - 1;
    int tsi = parser->token_start_index;
    parser->token_start_index = parser->body_start_index;
    // This will make the next read overwrite the old bytes
    request->bytes = parser->start;
    // If parser->len is 1 then there is no current partial token. This is kind
    // of ugly because len gets incremented in the for loop before we get here
    // so we need to check for 1 instead of 0
    if (parser->len > 1) {
      char* dst = input + parser->body_start_index;
      char const* src = input + tsi;
      // Copy partial token to beginning of body
      memcpy(dst, src, n - tsi);
    }
  }
  http_token_t token = { 0, 0, 0 };
  token.type = HTTP_NONE;
  return token;
}

void http_parse_start_chunk_mode(http_parser_t* parser) {
  parser->token_start_index = parser->start;
  parser->content_length = 0;
  parser->state = HTTP_CHUNK_SIZE;
}

// *** http server ***

void http_token_dyn_push(http_token_dyn_t* dyn, http_token_t a) {
  if (dyn->size == dyn->capacity) {
    dyn->capacity *= 2;
    dyn->buf = (http_token_t*)realloc(dyn->buf, dyn->capacity * sizeof(http_token_t));
    assert(dyn->buf != NULL);
  }
  dyn->buf[dyn->size] = a;
  dyn->size++;
}

void http_token_dyn_init(http_token_dyn_t* dyn, int capacity) {
  dyn->buf = (http_token_t*)malloc(sizeof(http_token_t) * capacity);
  assert(dyn->buf != NULL);
  dyn->size = 0;
  dyn->capacity = capacity;
}

void hs_bind_localhost(int s, struct sockaddr_in* addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port = htons(port);
  int rc = bind(s, (struct sockaddr *)addr, sizeof(struct sockaddr_in));;
  if (rc < 0) {
    exit(1);
  }
}

int hs_read_client_socket(http_request_t* session) {
  if (!session->buf) {
    session->server->memused += HTTP_REQUEST_BUF_SIZE;
    session->buf = (char*)calloc(1, HTTP_REQUEST_BUF_SIZE);
    assert(session->buf != NULL);
    session->capacity = HTTP_REQUEST_BUF_SIZE;
    http_token_dyn_init(&session->tokens, 32);
  }
  int bytes;
  do {
    bytes = read(
      session->socket,
      session->buf + session->bytes,
      session->capacity - session->bytes
    );
    if (bytes > 0) session->bytes += bytes;
    if (session->bytes == session->capacity) {
      session->server->memused -= session->capacity;
      session->capacity *= 2;
      session->server->memused += session->capacity;
      session->buf = (char*)realloc(session->buf, session->capacity);
      assert(session->buf != NULL);
    }
  } while (bytes > 0);
  return bytes == 0 ? 0 : 1;
}

int hs_write_client_socket(http_request_t* session) {
  int bytes = write(
    session->socket,
    session->buf + session->written,
    session->bytes - session->written
  );
  if (bytes > 0) session->written += bytes;
  return errno == EPIPE ? 0 : 1;
}

void hs_free_buffer(http_request_t* session) {
  if (session->buf) {
    free(session->buf);
    session->server->memused -= session->capacity;
    session->buf = NULL;
    free(session->tokens.buf);
    session->tokens.buf = NULL;
  }
}

void hs_parse_tokens(http_request_t* session) {
  http_token_t token;
  int chunk_start = 0;
  do {
    token = http_parse(&session->parser, session->buf, session->bytes);
    if (token.type != HTTP_NONE) {
      session->token = token;
      http_token_dyn_push(&session->tokens, token);
    }
    chunk_start = token.type == HTTP_BODY && token.len == HTTP_CHUNKED_LEN;
  } while (token.type != HTTP_NONE && !chunk_start);
}

void hs_init_session(http_request_t* session) {
  session->flags = 0;
  session->flags |= HTTP_AUTOMATIC;
  session->parser = (http_parser_t){ };
  session->bytes = 0;
  session->written = 0;
  session->buf = NULL;
  session->token.len = 0;
  session->token.index = 0;
  session->token.type = HTTP_NONE;
}

int hs_parsing_headers(http_request_t* request) {
  return request->token.type != HTTP_BODY;
}

int hs_reading_body(http_request_t* request) {
  if (
    request->token.type != HTTP_BODY ||
    request->token.len == 0 ||
    request->token.len == HTTP_CHUNKED_LEN
  ) {
    return 0;
  }
  int size = request->token.index + request->token.len;
  return request->bytes < size;
}

void hs_end_session(http_request_t* session) {
  hs_delete_events(session);
  close(session->socket);
  hs_free_buffer(session);
  free(session);
}

void hs_reset_timeout(http_request_t* request, int time) {
  request->timeout = time;
}

void hs_write_response(http_request_t* request) {
  if (!hs_write_client_socket(request)) { return hs_end_session(request); }
  if (request->written != request->bytes) {
    // All bytes of the body were not written and we need to wait until the
    // socket is writable again to complete the write
    hs_add_write_event(request);
    request->state = HTTP_SESSION_WRITE;
    hs_reset_timeout(request, HTTP_REQUEST_TIMEOUT);
  } else if (HTTP_FLAG_CHECK(request->flags, HTTP_CHUNKED_RESPONSE)) {
    // All bytes of the chunk were written and we need to get the next chunk
    // from the application.
    request->state = HTTP_SESSION_WRITE;
    hs_reset_timeout(request, HTTP_REQUEST_TIMEOUT);
    hs_free_buffer(request);
    HTTP_FLAG_CLEAR(request->flags, HTTP_RESPONSE_READY);
    hs_exec_response_handler(request, request->chunk_cb);
  } else if (HTTP_FLAG_CHECK(request->flags, HTTP_KEEP_ALIVE)) {
    // All bytes of the response were successfully written. However the
    // keep-alive flag was set so we don't close the connection, we just clean
    // up
    request->state = HTTP_SESSION_INIT;
    hs_free_buffer(request);
    hs_reset_timeout(request, HTTP_KEEP_ALIVE_TIMEOUT);
  } else {
    // All response bytes were written and the connection should be closed
    return hs_end_session(request);
  }
}

void hs_exec_response_handler(http_request_t* request, void (*handler)(http_request_t*)) {
  handler(request);
  if (HTTP_FLAG_CHECK(request->flags, HTTP_RESPONSE_READY)) {
    // The request handler has been called and a response is immediately ready.
    hs_write_response(request);
  } else {
    // The response is not ready immediately and will be written out later.
    HTTP_FLAG_SET(request->flags, HTTP_RESPONSE_PAUSED);
  }
}

void hs_error_response(http_request_t* request, int code, char const * message) {
  struct http_response_s* response = http_response_init();
  http_response_status(response, code);
  http_response_header(response, "Content-Type", "text/plain");
  http_response_body(response, message, strlen(message));
  http_respond(request, response);
  hs_write_response(request);
}

// Application requesting next chunk of request body.
void http_request_read_chunk(
  struct http_request_s* request,
  void (*chunk_cb)(struct http_request_s*)
) {
  request->chunk_cb = chunk_cb;
  http_token_t token = http_chunk_parse(request, request->buf, request->bytes);
  if (token.type == HTTP_CHUNK_BODY) {
    // The next chunk was in the read buffer and is ready.
    request->token = token;
    chunk_cb(request);
  } else {
    // No chunk is in the read buffer, continue reading the socket.
    if (!hs_read_client_socket(request)) { return hs_end_session(request); }
    http_token_t token = http_chunk_parse(request, request->buf, request->bytes);
    if (token.type == HTTP_CHUNK_BODY) {
      // A chunk was in the kernel network buffer
      request->token = token;
      request->chunk_cb(request);
    } else {
      // No chunk ready, wait for IO.
      request->state = HTTP_SESSION_READ_CHUNK;
    }
  }
}

// This is the heart of the request logic. This is the state machine that
// controls what happens when an IO event is received.
void http_session(http_request_t* request) {
  http_token_t token;
  switch (request->state) {
    case HTTP_SESSION_INIT:
      hs_init_session(request);
      request->state = HTTP_SESSION_READ_HEADERS;
      if (request->server->memused > HTTP_MAX_TOTAL_EST_MEM_USAGE) {
        return hs_error_response(request, 503, "Service Unavailable");
      }
      // fallthrough
    case HTTP_SESSION_READ_HEADERS:
      if (!hs_read_client_socket(request)) { return hs_end_session(request); }
      hs_reset_timeout(request, HTTP_REQUEST_TIMEOUT);
      hs_parse_tokens(request);
      if (request->token.type == HTTP_PARSE_ERROR) {
        switch (request->token.index) {
          case HTTP_ERR_BAD_REQUEST:
            return hs_error_response(request, 400, "Bad Request");
          case HTTP_ERR_PAYLOAD_TOO_LARGE:
            return hs_error_response(request, 413, "Payload Too Large");
        }
      } else if (hs_reading_body(request)) {
        // The full request body has not been ready. Need to wait for more IO.
        request->state = HTTP_SESSION_READ_BODY;
      } else if (!hs_parsing_headers(request)) {
        if (request->parser.flags & HS_PF_CHUNKED) {
          // Set state to NOP for chunked requests. This means we won't handle
          // any read events on the socket until the application has requested
          // to read a chunk.
          request->state = HTTP_SESSION_NOP;
          http_parse_start_chunk_mode(&request->parser);
        }
        return hs_exec_response_handler(request, request->server->request_handler);
      }
      break;
    case HTTP_SESSION_READ_BODY:
      if (!hs_read_client_socket(request)) { return hs_end_session(request); }
      hs_reset_timeout(request, HTTP_REQUEST_TIMEOUT);
      if (!hs_reading_body(request)) {
        // Full body has been read into the read buffer. Call the application
        // request handler
        return hs_exec_response_handler(request, request->server->request_handler);
      }
      // Full body has still not been read. Wait for more IO.
      break;
    case HTTP_SESSION_READ_CHUNK:
      if (!hs_read_client_socket(request)) { return hs_end_session(request); }
      hs_reset_timeout(request, HTTP_REQUEST_TIMEOUT);
      token = http_chunk_parse(request, request->buf, request->bytes);
      if (token.type == HTTP_CHUNK_BODY) {
        // Chunk is ready call the chunk handler
        request->token = token;
        request->state = HTTP_SESSION_NOP;
        request->chunk_cb(request);
      }
      break;
    case HTTP_SESSION_WRITE:
      hs_write_response(request);
      break;
  }
}

void hs_accept_connections(http_server_t* server) {
  int sock = 0;
  do {
    sock = accept(server->socket, (struct sockaddr *)&server->addr, &server->len);
    if (sock > 0) {
      http_request_t* session = (http_request_t*)calloc(1, sizeof(http_request_t));
      assert(session != NULL);
      session->socket = sock;
      session->server = server;
      session->timeout = HTTP_REQUEST_TIMEOUT;
      session->handler = hs_session_io_cb;
      int flags = fcntl(sock, F_GETFL, 0);
      fcntl(sock, F_SETFL, flags | O_NONBLOCK);
      hs_add_events(session);
      http_session(session);
    }
  } while (sock > 0);
}

void hs_generate_date_time(char** datetime) {
  time_t rawtime;
  struct tm * timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  *datetime = asctime(timeinfo);
}

http_server_t* http_server_init(int port, void (*handler)(http_request_t*)) {
  http_server_t* serv = (http_server_t*)malloc(sizeof(http_server_t));
  assert(serv != NULL);
  serv->port = port;
  serv->memused = 0;
  serv->handler = hs_server_listen_cb;
  hs_server_init(serv);
  hs_generate_date_time(&serv->date);
  serv->request_handler = handler;
  return serv;
}

void http_listen(http_server_t* serv) {
  // Ignore SIGPIPE. We handle these errors at the call site.
  signal(SIGPIPE, SIG_IGN);
  serv->socket = socket(AF_INET, SOCK_STREAM, 0);
  int flag = 1;
  setsockopt(serv->socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  hs_bind_localhost(serv->socket, &serv->addr, serv->port);
  serv->len = sizeof(serv->addr);
  int flags = fcntl(serv->socket, F_GETFL, 0);
  fcntl(serv->socket, F_SETFL, flags | O_NONBLOCK);
  listen(serv->socket, 128);
  hs_add_server_sock_events(serv);
}

int http_server_listen_poll(http_server_t* serv) {
  http_listen(serv);
  return 0;
}

int http_server_loop(http_server_t* server) {
  return server->loop;
}

// *** http request ***

http_string_t http_get_token_string(http_request_t* request, int token_type) {
  http_string_t str = { 0, 0 };
  if (request->tokens.buf == NULL) return str;
  for (int i = 0; i < request->tokens.size; i++) {
    http_token_t token = request->tokens.buf[i];
    if (token.type == token_type) {
      str.buf = &request->buf[token.index];
      str.len = token.len;
      return str;
    }
  }
  return str;
}

int hs_case_insensitive_cmp(char const * a, char const * b, int len) {
  for (int i = 0; i < len; i++) {
    char c1 = a[i] >= 'A' && a[i] <= 'Z' ? a[i] + 32 : a[i];
    char c2 = b[i] >= 'A' && b[i] <= 'Z' ? b[i] + 32 : b[i];
    if (c1 != c2) return 0;
  }
  return 1;
}

http_string_t http_request_method(http_request_t* request) {
  return http_get_token_string(request, HTTP_METHOD);
}

http_string_t http_request_target(http_request_t* request) {
  return http_get_token_string(request, HTTP_TARGET);
}

http_string_t http_request_body(http_request_t* request) {
  return http_get_token_string(request, HTTP_BODY);
}

int hs_assign_iteration_headers(
  http_request_t* request,
  http_string_t* key,
  http_string_t* val,
  int* iter
) {
  http_token_t token = request->tokens.buf[*iter];
  if (request->tokens.buf[*iter].type == HTTP_BODY) return 0;
  *key = (http_string_t) {
    .buf = &request->buf[token.index],
    .len = token.len
  };
  (*iter)++;
  token = request->tokens.buf[*iter];
  *val = (http_string_t) {
    .buf = &request->buf[token.index],
    .len = token.len
  };
  return 1;
}

int http_request_iterate_headers(
  http_request_t* request,
  http_string_t* key,
  http_string_t* val,
  int* iter
) {
  if (*iter == 0) {
    for ( ; *iter < request->tokens.size; (*iter)++) {
      http_token_t token = request->tokens.buf[*iter];
      if (token.type == HTTP_HEADER_KEY) {
        return hs_assign_iteration_headers(request, key, val, iter);
      }
    }
    return 0;
  } else {
    (*iter)++;
    return hs_assign_iteration_headers(request, key, val, iter);
  }
}

http_string_t http_request_header(http_request_t* request, char const * key) {
  int len = strlen(key);
  for (int i = 0; i < request->tokens.size; i++) {
    http_token_t token = request->tokens.buf[i];
    if (token.type == HTTP_HEADER_KEY && token.len == len) {
      if (hs_case_insensitive_cmp(&request->buf[token.index], key, len)) {
        token = request->tokens.buf[i + 1];
        return (http_string_t) {
          .buf = &request->buf[token.index],
          .len = token.len
        };
      }
    }
  }
  return (http_string_t) { };
}

void http_request_free_buffer(http_request_t* request) {
  hs_free_buffer(request);
}

void* http_request_userdata(http_request_t* request) {
  return request->data;
}

void http_request_set_userdata(http_request_t* request, void* data) {
  request->data = data;
}

void hs_auto_detect_keep_alive(http_request_t* request) {
  http_string_t str = http_get_token_string(request, HTTP_VERSION);
  if (str.buf == NULL) return;
  int version = str.buf[str.len - 1] == '1';
  str = http_request_header(request, "Connection");
  if (
    (str.len == 5 && hs_case_insensitive_cmp(str.buf, "close", 5)) ||
    (str.len == 0 && version == HTTP_1_0)
  ) {
    HTTP_FLAG_CLEAR(request->flags, HTTP_KEEP_ALIVE);
  } else {
    HTTP_FLAG_SET(request->flags, HTTP_KEEP_ALIVE);
  }
}

void http_request_connection(http_request_t* request, int directive) {
  if (directive == HTTP_KEEP_ALIVE) {
    HTTP_FLAG_CLEAR(request->flags, HTTP_AUTOMATIC);
    HTTP_FLAG_SET(request->flags, HTTP_KEEP_ALIVE);
  } else if (directive == HTTP_CLOSE) {
    HTTP_FLAG_CLEAR(request->flags, HTTP_AUTOMATIC);
    HTTP_FLAG_CLEAR(request->flags, HTTP_KEEP_ALIVE);
  }
}

http_string_t http_request_chunk(struct http_request_s* request) {
  return (http_string_t) {
    .buf = &request->buf[request->token.index],
    .len = request->token.len
  };
}

// *** http response ***

http_response_t* http_response_init() {
  http_response_t* response = (http_response_t*)calloc(1, sizeof(http_response_t));
  assert(response != NULL);
  response->status = 200;
  return response;
}

void http_response_header(http_response_t* response, char const * key, char const * value) {
  http_header_t* header = (http_header_t*)malloc(sizeof(http_header_t));
  assert(header != NULL);
  header->key = key;
  header->value = value;
  http_header_t* prev = response->headers;
  header->next = prev;
  response->headers = header;
}

void http_response_status(http_response_t* response, int status) {
  response->status = status > 599 || status < 100 ? 500 : status;
}

void http_response_body(http_response_t* response, char const * body, int length) {
  response->body = body;
  response->content_length = length;
}

typedef struct {
  char* buf;
  int capacity;
  int size;
  long* memused;
} grwprintf_t;

void grwprintf_init(grwprintf_t* ctx, int capacity, long* memused) {
  ctx->memused = memused;
  ctx->size = 0;
  ctx->buf = (char*)malloc(capacity);
  *ctx->memused += capacity;
  assert(ctx->buf != NULL);
  ctx->capacity = capacity;
}

void grwmemcpy(grwprintf_t* ctx, char const * src, int size) {
  if (ctx->size + size > ctx->capacity) {
    *ctx->memused -= ctx->capacity;
    ctx->capacity = ctx->size + size;
    *ctx->memused += ctx->capacity;
    ctx->buf = (char*)realloc(ctx->buf, ctx->capacity);
    assert(ctx->buf != NULL);
  }
  memcpy(ctx->buf + ctx->size, src, size);
  ctx->size += size;
}

void grwprintf(grwprintf_t* ctx, char const * fmt, ...) {
  va_list args;
  va_start(args, fmt);

  int bytes = vsnprintf(ctx->buf + ctx->size, ctx->capacity - ctx->size, fmt, args);
  if (bytes + ctx->size > ctx->capacity) {
    *ctx->memused -= ctx->capacity;
    while (bytes + ctx->size > ctx->capacity) ctx->capacity *= 2;
    *ctx->memused += ctx->capacity;
    ctx->buf = (char*)realloc(ctx->buf, ctx->capacity);
    assert(ctx->buf != NULL);
    bytes += vsnprintf(ctx->buf + ctx->size, ctx->capacity - ctx->size, fmt, args);
  }
  ctx->size += bytes;
 
  va_end(args);
}

void http_buffer_headers(
  http_request_t* request,
  http_response_t* response,
  grwprintf_t* printctx
) {
  http_header_t* header = response->headers;
  while (header) {
    grwprintf(printctx, "%s: %s\r\n", header->key, header->value);
    header = header->next;
  }
  if (!HTTP_FLAG_CHECK(request->flags, HTTP_CHUNKED_RESPONSE)) {
    grwprintf(printctx, "Content-Length: %d\r\n", response->content_length);
  }
  grwprintf(printctx, "\r\n");
}

void http_respond_headers(
  http_request_t* request,
  http_response_t* response,
  grwprintf_t* printctx
) {
  if (HTTP_FLAG_CHECK(request->flags, HTTP_AUTOMATIC)) {
    hs_auto_detect_keep_alive(request);
  }
  if (HTTP_FLAG_CHECK(request->flags, HTTP_KEEP_ALIVE)) {
    http_response_header(response, "Connection", "keep-alive");
  } else {
    http_response_header(response, "Connection", "close");
  }
  grwprintf(
    printctx, "HTTP/1.1 %d %s\r\nDate: %.24s\r\n",
    response->status, hs_status_text[response->status], request->server->date
  );
  http_buffer_headers(request, response, printctx);
}

void http_end_response(http_request_t* request, http_response_t* response, grwprintf_t* printctx) {
  http_header_t* header = response->headers;
  while (header) {
    http_header_t* tmp = header;
    header = tmp->next;
    free(tmp);
  }
  hs_free_buffer(request);
  free(response);
  request->buf = printctx->buf;
  request->written = 0;
  request->bytes = printctx->size;
  request->capacity = printctx->capacity;
  request->state = HTTP_SESSION_WRITE;
  // Signal that the response is ready for writing.
  HTTP_FLAG_SET(request->flags, HTTP_RESPONSE_READY);
  if (HTTP_FLAG_CHECK(request->flags, HTTP_RESPONSE_PAUSED)) {
    // If the request has been paused waiting for the response clear the flag
    HTTP_FLAG_CLEAR(request->flags, HTTP_RESPONSE_PAUSED);
    http_session(request);
  }
}

void http_respond(http_request_t* request, http_response_t* response) {
  grwprintf_t printctx;
  grwprintf_init(&printctx, HTTP_RESPONSE_BUF_SIZE, &request->server->memused);
  http_respond_headers(request, response, &printctx);
  if (response->body) {
    grwmemcpy(&printctx, response->body, response->content_length);
  }
  http_end_response(request, response, &printctx);
}

void http_respond_chunk(
  http_request_t* request,
  http_response_t* response,
  void (*cb)(http_request_t*)
) {
  grwprintf_t printctx;
  grwprintf_init(&printctx, HTTP_RESPONSE_BUF_SIZE, &request->server->memused);
  if (!HTTP_FLAG_CHECK(request->flags, HTTP_CHUNKED_RESPONSE)) {
    HTTP_FLAG_SET(request->flags, HTTP_CHUNKED_RESPONSE);
    http_response_header(response, "Transfer-Encoding", "chunked");
    http_respond_headers(request, response, &printctx);
  }
  request->chunk_cb = cb;
  grwprintf(&printctx, "%X\r\n", response->content_length);
  grwmemcpy(&printctx, response->body, response->content_length);
  grwprintf(&printctx, "\r\n");
  http_end_response(request, response, &printctx);
}

void http_respond_chunk_end(http_request_t* request, http_response_t* response) {
  grwprintf_t printctx;
  grwprintf_init(&printctx, HTTP_RESPONSE_BUF_SIZE, &request->server->memused);
  grwprintf(&printctx, "0\r\n");
  http_buffer_headers(request, response, &printctx);
  grwprintf(&printctx, "\r\n");
  HTTP_FLAG_CLEAR(request->flags, HTTP_CHUNKED_RESPONSE);
  http_end_response(request, response, &printctx);
}

// *** kqueue platform specific ***

#ifdef KQUEUE

void hs_server_listen_cb(struct kevent* ev) {
  http_server_t* server = (http_server_t*)ev->udata;
  if (ev->filter == EVFILT_TIMER) {
    hs_generate_date_time(&server->date);
  } else {
    hs_accept_connections(server);
  }
}

void hs_session_io_cb(struct kevent* ev) {
  http_request_t* request = (http_request_t*)ev->udata;
  if (ev->filter == EVFILT_TIMER) {
    request->timeout -= 1;
    if (request->timeout == 0) hs_end_session(request);
  } else {
    http_session(request);
  }
}

void hs_server_init(http_server_t* serv) {
  serv->loop = kqueue();
  struct kevent ev_set;
  EV_SET(&ev_set, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_SECONDS, 1, serv);
  kevent(serv->loop, &ev_set, 1, NULL, 0, NULL);
}

void hs_add_server_sock_events(http_server_t* serv) {
  struct kevent ev_set;
  EV_SET(&ev_set, serv->socket, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, serv);
  kevent(serv->loop, &ev_set, 1, NULL, 0, NULL);
}

int http_server_listen(http_server_t* serv) {
  http_listen(serv);

  struct kevent ev_list[1];

  while (1) {
    int nev = kevent(serv->loop, NULL, 0, ev_list, 1, NULL);
    for (int i = 0; i < nev; i++) {
      ev_cb_t* ev_cb = (ev_cb_t*)ev_list[i].udata;
      ev_cb->handler(&ev_list[i]);
    }
  }
  return 0;
}

void hs_delete_events(http_request_t* request) {
  struct kevent ev_set;
  EV_SET(&ev_set, request->socket, EVFILT_TIMER, EV_DELETE, 0, 0, request);
  kevent(request->server->loop, &ev_set, 1, NULL, 0, NULL);
}

int http_server_poll(http_server_t* serv) {
  struct kevent ev;
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  int nev = kevent(serv->loop, NULL, 0, &ev, 1, &ts);
  if (nev <= 0) return nev;
  ev_cb_t* ev_cb = (ev_cb_t*)ev.udata;
  ev_cb->handler(&ev);
  return nev;
}

void hs_add_events(http_request_t* request) {
  struct kevent ev_set[2];
  EV_SET(&ev_set[0], request->socket, EVFILT_READ, EV_ADD, 0, 0, request);
  EV_SET(&ev_set[1], request->socket, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_SECONDS, 1, request);
  kevent(request->server->loop, ev_set, 2, NULL, 0, NULL);
}

void hs_add_write_event(http_request_t* request) {
  struct kevent ev_set[2];
  EV_SET(&ev_set[0], request->socket, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, request);
  kevent(request->server->loop, ev_set, 2, NULL, 0, NULL);
}

#else

// *** epoll platform specific ***

void hs_server_listen_cb(struct epoll_event* ev) {
  hs_accept_connections((http_server_t*)ev->data.ptr);
}

void hs_session_io_cb(struct epoll_event* ev) {
  http_session((http_request_t*)ev->data.ptr);
}

void hs_server_timer_cb(struct epoll_event* ev) {
  http_server_t* server = (http_server_t*)((char*)ev->data.ptr - sizeof(epoll_cb_t));
  uint64_t res;
  int bytes = read(server->timerfd, &res, sizeof(res));
  (void)bytes; // suppress warning
  hs_generate_date_time(&server->date);
}

void hs_request_timer_cb(struct epoll_event* ev) {
  http_request_t* request = (http_request_t*)((char*)ev->data.ptr - sizeof(epoll_cb_t));
  uint64_t res;
  int bytes = read(request->timerfd, &res, sizeof(res));
  (void)bytes; // suppress warning
  request->timeout -= 1;
  if (request->timeout == 0) hs_end_session(request);
}

void hs_add_server_sock_events(http_server_t* serv) {
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = serv;
  epoll_ctl(serv->loop, EPOLL_CTL_ADD, serv->socket, &ev);
}

void hs_server_init(http_server_t* serv) {
  serv->loop = epoll_create1(0);
  serv->timer_handler = hs_server_timer_cb;

  int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
  struct itimerspec ts = {};
  ts.it_value.tv_sec = 1;
  ts.it_interval.tv_sec = 1;
  timerfd_settime(tfd, 0, &ts, NULL);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = &serv->timer_handler;
  epoll_ctl(serv->loop, EPOLL_CTL_ADD, tfd, &ev);
  serv->timerfd = tfd;
}

int http_server_listen(http_server_t* serv) {
  http_listen(serv);
  struct epoll_event ev_list[1];
  while (1) {
    int nev = epoll_wait(serv->loop, ev_list, 1, -1);
    for (int i = 0; i < nev; i++) {
      ev_cb_t* ev_cb = (ev_cb_t*)ev_list[i].data.ptr;
      ev_cb->handler(&ev_list[i]);
    }
  }
  return 0;
}

void hs_delete_events(http_request_t* request) {
  epoll_ctl(request->server->loop, EPOLL_CTL_DEL, request->socket, NULL);
  epoll_ctl(request->server->loop, EPOLL_CTL_DEL, request->timerfd, NULL);
  close(request->timerfd);
}

int http_server_poll(http_server_t* serv) {
  struct epoll_event ev;
  int nev = epoll_wait(serv->loop, &ev, 1, 0);
  if (nev <= 0) return nev;
  ev_cb_t* ev_cb = (ev_cb_t*)ev.data.ptr;
  ev_cb->handler(&ev);
  return nev;
}

void hs_add_events(http_request_t* request) {
  request->timer_handler = hs_request_timer_cb;

  // Watch for read events
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = request;
  epoll_ctl(request->server->loop, EPOLL_CTL_ADD, request->socket, &ev);

  // Add timer to timeout requests.
  int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
  struct itimerspec ts = {};
  ts.it_value.tv_sec = 1;
  ts.it_interval.tv_sec = 1;
  timerfd_settime(tfd, 0, &ts, NULL);

  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = &request->timer_handler;
  epoll_ctl(request->server->loop, EPOLL_CTL_ADD, tfd, &ev);
  request->timerfd = tfd;
}

void hs_add_write_event(http_request_t* request) {
  struct epoll_event ev;
  ev.events = EPOLLOUT | EPOLLET;
  ev.data.ptr = request;
  epoll_ctl(request->server->loop, EPOLL_CTL_MOD, request->socket, &ev);
}

#endif

#endif
#endif
