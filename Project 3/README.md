# Programming Assignment 3

The goal of this assignment was to create an HTTP web proxy. To compile the program, run `make` in this directory. The execution arguments are below in `Usage`. Once running, you can configure your browser to use this web proxy and browse an HTTP website like `http://neverssl.com/`. The project PDF is in `CSCI4273_PA3.pdf`.

### Usage
* `./webproxy <port> <cache_timeout (int seconds)>`

### Functionality
* Only `HTTP` requests are supported by the proxy. Functionality is undefined for `HTTPS` requests.
* All `GET` requests are forwarded from the proxy to the HTTP server. All other requests are not supported and return an error. The HTTP response is cached locally and then forwarded to the client. The cached responses expire after `<cache_timeout>` seconds and are re-retreived when expired.
* All DNS query results are cached locally. Before performing a DNS query, the local cache is referenced to see if the hostname has already been resolved.
* `blacklist.txt` contains hostnames and IP addresses separated by newlines. If an HTTP request references a blacklisted hostname or IP address the query does not go through and an error is returned. Hostnames are resolved to IP addresses when referencing the blacklist as well in cases where two different hostnames resolve to the same IP address.
* This implementation assumes that for each call to `read` there are an integer number of whole HTTP requests.
* We didn't attempt the link prefetching extra credit, however, we are turning in this assignment on April 13, 2021.
* We used Firefox for all testing because setting up the proxy on Firefox is much easier than on Chrome; however, we don't see why this proxy would fail in other browsers.
* We expect that every HTTP result contains a `Content-Length:` field, otherwise, we assume that there is no content for the HTTP result.
* Various debugging print flags are available in `server.h`.

### Error codes
* `400`: DNS lookup couldn't resolve the hostname.
* `403`: The hostname is blacklisted.
* `405`: Method not supported (The method is not `GET`).
