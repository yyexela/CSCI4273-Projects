# Programming Assignment 2

This folder contains the files for programming assignment 2 from CSCI 4273: Network Systems. The goal was to create an HTTP web server in C. Directions for running the server are below, in `Notes`. Unfortunately I wasn't able to save the project PDF.

### Directory Structure:

```text
.
|-- README.md
|   * This file
|-- Skeleton
|   |   * The initial files given for the programming assignment
|   |-- httpechosrv-basic.c
|   `-- www
|-- makefile
|   * Used to compile the server code
|-- server.c
|   * The server C code
|-- server.h
|   * The header file for server.c
|-- test_code
|   |   * Directory used to test various functionalities before adding them to server.c
|   |-- makefile
|   |-- packet_parse.c
|   |-- packet_parse.exe
|   |-- timer.c
|   `-- timer.exe
`-- www
    |   * The files the server hands off to the clients
    |-- css
    |-- fancybox
    |-- favicon.ico
    |-- files
    |-- graphics
    |-- images
    |-- index.html
    `-- jquery-1.4.3.min.js
```

### Compilation/Running:
* To compile the program, go to the root directory and run `make clean` followed by `make`. To run the program, run `./server.exe <PORT NUMBER>`. You can then connect to the server locally by browsing `localhost:<PORT>`.

### Notes:

* We attempted both extra credits (pipelining and POST requests).
* The timeout for the pipelining is set to 10 seconds.
* If a POST request's POST data contains a a newline which is immediately followed by "GET" or "POST" the program incorrectly runs.
* The server returns HTTP status code 200 when the request is fulfilled, 404 if the file requested for does not exist, and 500 for all other errors.
* Testing the POST requests with `telnet` did not work because `telnet` wasn't sending the entire request at once to the server, so when the server called `read` it wasn't getting the entire request and was thus breaking. Please use `nc` when testing all requests through the command line. This implementation assumes that for each call to `read` there are an integer number of whole HTTP requests.
* We exclusively used Firefox and the default Ubuntu Linux web browser to test everything. Please use either of these browsers; however, we have no reason to suspect this code would fail in other browsers.
* The file provided called `welcome.html~` was changed to `welcome.html` because we were told we could do that by Prof. Rozner in office hours.
* Various debugging print flags are available in `server.h` if you'd like to see more information. Our final commit includes the flags we found most useful; however, if you'd like more or less information they can be changed.