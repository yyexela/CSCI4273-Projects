# Programming Assignment 4

This project involved creating a distributed filesystem. The ideas is that there are four servers running which store redundant data when the client uploads a file. For example, out of a file split into four segments, server 1 stores parts `1` and `2`, server 2 stores parts `2` and `3`, server 3 stores parts `2` and `3` and server 4 stores parts `4` and `1`. Any other client can then download this file as long as there are enough servers running containing all four parts. So, servers 1 and 3 can go offline and the entire file can still be reconstructed. To compile the server and client, run `make` in the respective directories. The usage is below. Unforunately the project PDF is lost.

### Client Usage
* `dfc dfc.conf`

### Server Usage
* `dfs <server name> <port>`
* Example of the four servers running:
* `dfs DFS1 10001`
* `dfs DFS2 10002`
* `dfs DFS3 10003`
* `dfs DFS4 10004`

## Design Notes
 * We expect exactly 4 entries in `dfc.conf` where each server's entry is separated by newlines and has values delimeted by a single space (exactly as shown in the project PDF, except we use one line per entry). The number of the line represents the number of the server (ex. line 2 is used for server 2's information, the first line is line 1). The values for each server in `dfc.conf` represent the following: `Server <relative cache directory> <IP address>:<Port number>`. We assume the directories `DFS1`, `DFS2`, `DFS3`, and `DFS4` will not be deleted while the servers are running. The directories must be passed in like `DFS1`, not `DFS1/` (exactly as is shown in the project PDF).
 * If a server restarts, we don't delete its stored files. The server can be re-booted and serve the stored files.
 * If the user doesn't provide any input, the program exits (ie, 'return' is pressed without any other input).
 * The metadata pre-prended to each part of a file sent during a `put` request by the client is: `put <FILENAME> <FILESIZE> <PART NUMBER> `.
 * The packet sent by the client during a `list` request is: `list`. The packet returned by the server is a series of lines (separated by newlines) signifying which files are present (`<FILENAME> 14 <DIRNAME>` indicates files `1` and `4` for file `<FILENAME>` exist from directory `<DIRNAME>`).
 * All commands are supported even when some servers are down. `list` receives the data it can, and tells the user which files are complete or not. `put` will distribute pieces to servers that are alive. `get` will retrieve all pieces it can from live servers and attempt to reconstruct the file, notifing the user if it can't.
 * Every operation opens up a new TCP connection, makes the code nicer and easier to understand.
 * We take the last 64 bytes of the md5 value when calculating modulo 4, (theoretically we would only need the last byte anyways for this operation due to the way modular arithmetic works).
 * filenames with spaces are not supported.
 * `make clean` does NOT remove stored files from server directories, this must be done manually through the OS file system if desired.
 * There are scripts/code in the `bash_scripts` and `shared` directories that are just as important to operation as the `client` and `server` directories, but the make file and other code handles these connections, and the user doesn't have to worry about it. Just don't move stuff around.
 * This code was tested on our machines as well as LOCALLY on an ELRA machine. The project description stipulates that the config file should use local hosts IP, so that is the scope of our testing, although we have no reason to expect it to fail otherwise.
