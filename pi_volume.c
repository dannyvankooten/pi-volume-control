#include <stdio.h>

#define HTTPSERVER_IMPL
#include "httpserver.h"

char html[2048];
char manifest[512];

int http_string_compare(struct http_string_s s, char expected[]) {
    return strncmp(s.buf, expected, strlen(expected)) == 0;
}

void read_file(char filename[], char buf[])
{
    FILE *f = fopen(filename, "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);

    fseek(f, 0, SEEK_SET); /* same as rewind(f); */
    fread(buf, 1, fsize, f);

    fclose(f);
    buf[fsize] = 0;
}

void handle_request(struct http_request_s *request)
{    
    if (http_string_compare(http_request_method(request), "POST"))
    {
        if (http_string_compare(http_request_body(request), "volume=up"))
        {
            system("/usr/bin/amixer sset Digital 5%+");
        }
        else
        {
            system("/usr/bin/amixer sset Digital 5%-");
        }
    }

    char * content;
    struct http_response_s *response = http_response_init();
    http_response_status(response, 200);
    if (http_string_compare(http_request_target(request), "/manifest.json"))
    {
        content = manifest;
        http_response_header(response, "Content-Type", "application/json");
    }
    else
    {
        content = html;
        http_response_header(response, "Content-Type", "text/html");
    }

    http_response_body(response, content, strlen(content));
    http_respond(request, response);
}



int main()
{
    read_file("index.html", html);
    read_file("manifest.json", manifest);

    struct http_server_s *server = http_server_init(8080, handle_request);
    http_server_listen(server);
}
