#include <stdio.h>

#define HTML "\
<!DOCTYPE html>\
<html lang=\"en\">\
	<head>\
		<link rel=\"manifest\" href=\"/manifest.json\">\
		<meta charset=\"utf-8\">\
		<title>Woonkamer volume</title>\
		<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\">\
<style> * { margin: 0; padding: 0; }\
button { font-size:30vh; margin: 0 auto; display: block; background: #111; color: white; border: 0; }\
body { font-family: sans-serif; background: #111; padding: 24px; }\
		</style>\
	</head>\
	<body>\
		<form method=\"POST\">\
			<button name=\"volume\" value=\"up\">▲</button>\
		</form>\
		<form method=\"POST\">\
			<button name=\"volume\" value=\"down\">▼</button>\
		</form>\
		<script>\
			document.addEventListener('submit', function(evt) {\
				evt.preventDefault();\
				window.fetch(window.location.href, {\
					method: 'POST',\
					body: `volume=${evt.target.volume.value}`\
				});\
			});\
		</script>\
	</body>\
</html>"

#define MANIFEST "\
{\
  \"name\": \"Woonkamer volume\",\
  \"display\": \"standalone\",\
  \"background_color\": \"#111\"\
}"

#define HTTPSERVER_IMPL
#include "httpserver.h"

void readbuf(char *dest, http_string_t str);

int http_string_compare(struct http_string_s s, char expected[]) {
    return strncmp(s.buf, expected, strlen(expected)) == 0;
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

    struct http_response_s *response = http_response_init();
    http_response_status(response, 200);
    if (http_string_compare(http_request_target(request), "/manifest.json"))
    {
        http_response_header(response, "Content-Type", "application/json");
        http_response_body(response, MANIFEST, sizeof(MANIFEST) - 1);
    }
    else
    {
        http_response_header(response, "Content-Type", "text/html");
        http_response_body(response, HTML, sizeof(HTML) - 1);
    }
    http_respond(request, response);
}

int main()
{
    struct http_server_s *server = http_server_init(8080, handle_request);
    http_server_listen(server);
}

void readbuf(char *dest, http_string_t str)
{
    dest[0] = '\0';

    int i;
    for (i = 0; i < str.len; i++)
    {
        dest[i] = str.buf[i];
    }
    dest[i] = '\0';
}