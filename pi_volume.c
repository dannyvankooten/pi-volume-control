#include <stdio.h>

#define RESPONSE "\
<html>\
<head>\
<style>body { background: #333; color: white; }</style>\
</head>\
<body>\
<form><button>&uparrow;</button></form>\
<form><button>&downarrow;</button></form>\
</body>\
</html>"

#define HTTPSERVER_IMPL
#include "httpserver.h"

void readbuf(char * dest, http_string_t str);

void handle_request(struct http_request_s * request) {
//   char method[100];
//   readbuf(method, http_request_method(request));
//   char target[100];
//   readbuf(target, http_request_target(request));
//   printf("%s %s\n", method, target);

  system("actl +10%%");

  struct http_response_s* response = http_response_init();
  http_response_status(response, 200);
  http_response_header(response, "Content-Type", "text/html");
  http_response_body(response, RESPONSE, sizeof(RESPONSE) - 1);
  http_respond(request, response);
}

int main() {
  struct http_server_s* server = http_server_init(8080, handle_request);
  http_server_listen(server);
}

void readbuf(char * dest, http_string_t str) {
    int i;
    for (i=0; i < str.len; i++) {
        dest[i] = str.buf[i];
    }
    dest[i] = '\0';
}