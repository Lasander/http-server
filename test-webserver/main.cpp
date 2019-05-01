#include "http_server/http_server.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <sstream>
#include <assert.h>

int main(int argc, char* argv[])
{
    using namespace std::chrono_literals;
    using namespace http_server;

	std::vector<websocket_handle> websockets;

    server s;
    s.add_handler("/(index.*)?", [](const request& req, response& res) {
        // Don't handle, let the server serve the file
        return false;
    });
    s.add_handler("PUT|GET", "/A", [](const request& req, response& res) {
        res.set_status(200, "OK");
        res << "<html><body>"
			<< "<h2>This is the A handler!!!</h2>"
			<< "</body></html>\n";
        return true;
    });
    s.add_handler("/A(.*)", [](const request& req, response& res) {
        res.set_status(200, "OK");
        const auto& match = req.get_url_matches();
        res << "<html><body>"
			<< "<h2>Matches: " << match[0].str() << " " << match[1].str() << "</h2>"
			<< "</body></html>\n";
        return true;
    });
    s.add_handler("/B", [](const request&, response&) { return true; });

    s.add_handler("/websocket", [](const request& req, response& res) {
        res.set_status(200, "OK");

        res << R"--(<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8">
	<title>Embedded websocket example</title>
	<script>
		function load() {
			var wsproto = (location.protocol === 'https:') ? 'wss:' : 'ws:';
			connection = new WebSocket(wsproto + '//' + window.location.host + '/websocket');
			websock_text_field = document.getElementById('websock_text_field');

			connection.onmessage = function (e) {
				websock_text_field.innerHTML=e.data;
			}

			connection.onerror = function (error) {
				websock_text_field.innerHTML='WebSocket error';
				connection.close();
			}
		}
	</script>
</head>
<body onload="load()">
	<div id='websock_text_field'>No websocket connection yet</div>
</body>
</html>
)--";

        return 1;
    });

    s.add_websocket_handler(
        "/websocket",
        [&websockets](websocket_connection& connection) {
            std::cout << "/websocket - "
                      << "connecting client: " << connection.get_handle() << std::endl;

            connection.send("Hello from the websocket ready handler");

			websockets.push_back(connection.get_handle());
        },
        [&s, &websockets](websocket_connection& connection, const websocket_message& message) {
            std::cout << "/websocket - " << message.get_data().size() << " bytes of ";
            switch (message.get_opcode())
            {
            case websocket_opcode::CONTINUATION: std::cout << "continuation"; break;
            case websocket_opcode::TEXT: std::cout << "text"; break;
            case websocket_opcode::BINARY: std::cout << "binary"; break;
            case websocket_opcode::CONNECTION_CLOSE: std::cout << "close"; break;
            case websocket_opcode::PING: std::cout << "ping"; break;
            case websocket_opcode::PONG: std::cout << "pong"; break;
            default:
                std::cout << "unknown(" << static_cast<unsigned char>(message.get_opcode()) << ")";
                break;
            }
            std::cout << " data from client: " << connection.get_handle() << std::endl;
            std::cout << message.get_data();
            std::cout << std::endl;

            std::ostringstream oss;
            oss << connection.get_handle() << ": "
                << message.get_data();

            server::lock lk(s);
            for (auto& handle : websockets)
            {
                auto connection = s.get_websocket_connection(handle);
                assert(connection);
                connection->send(oss.str());
            }
        },
        [&websockets](const websocket_connection& connection) {
            std::cout << "/websocket - "
                      << "disconnecting client: " << connection.get_handle() << std::endl;

			auto it = std::find(websockets.begin(), websockets.end(), connection.get_handle());
			if (it != websockets.end())
			{
				websockets.erase(it);
			}
        });

    while (true)
    {
        std::this_thread::sleep_for(10s);

        static unsigned long cnt = 0;
        char text[32];

        sprintf(text, "From server: %lu", ++cnt);

        server::lock lk(s);
        for (auto& handle : websockets)
        {
			auto connection = s.get_websocket_connection(handle);
			assert(connection);
			connection->send(text);
        }
    }
    return EXIT_SUCCESS;
}
