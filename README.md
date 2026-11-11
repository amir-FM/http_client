# HTTP Client

## Description
For this project, I chose to implement the solution in C++, taking advantage of the STL library. I also modularized the program's functionalities using multiple interacting classes, with the note that they are written in a single file rather than separately.

## Technical Overview

### Classes Used
* **TCP** — enables the connection to the server and provides an interface for sending and receiving TCP messages.
  * `start()` — opens a socket for communication.
  * `stop()` — closes the socket.
  * `send_message()` — sends a TCP message (the function ensures the entire message is transmitted).
  * `recv_message()` — receives a message; by parsing the received data, it determines the body size (if applicable) and ensures the entire message is fully received.
* **HTTP** — handles the generation of HTTP requests for GET, POST, DELETE, and PUT methods, taking into account cookies, JWT tokens, paths, URL parameters, and JSON data.
* **HTTP_Res** — receives the HTTP response from the TCP class and interprets it.
  * `add_response()` — adds new data to be processed.
  * `check_response()` — checks whether the response has a 2XX success status code or an error code (any non-success code).
  * `get_data()` — extracts the body from the response.
  * `get_error()` — extracts the error message.
  * `get_json()` — returns the JSON data from the body.
* **Client** — manages user interaction by reading commands and executing functions in accordance with the server API and the commands available to the client.

## JSON Integration
When it came to working with JSON structures, I decided to use an existing library: `nlohmann/json` (available here: https://github.com/nlohmann/json). The choice to use this library comes down to a simple fact: without it, I would have had to manually implement a JSON parsing class, which would have doubled the development time. It is used both when extracting data from user input to build the request, and when parsing the response from the server.

## Additional Notes
* User input is always validated, ensuring both the correctness of the entered data and that a valid command was provided.
