# README.txt

This folder contains the files for programming assignment 1 for CSCI 4273: Network Systems. This project involved creating a UDP server and a client in C where the client could `get`, `put`, `delete`, and `ls` files from the server. The directions for running the server are below, in `To run:`. The project description is contained in `CSCI4273_PA1.pdf`.

## Directory Structure


```text
.
|-- PA1_udp_example.tar : File provided with the assignment
|-- README.md : This file
|-- README.txt : The same README but in markdown
|-- TCPExample : Sample code I worked on to understand and 
|   |            create a TCP connection (we will be working on UDP for this project)
|   |-- uftp_client.c : C file for the client (TCP)
|   `-- server.c : C file for the server (TCP)
|-- client : Code for the client
|   |-- client.c : C file for the client (UDP)
|   |-- client.h : C header file for the client (UDP)
|   |-- foo1 : foo1 - foo3 will eventually be sent over UDP to the server
|   |-- foo2
|   |-- foo3
|   `-- makefile : 'make' to compile 'client.c', 'make clean' to remove all the junk
|-- shared
|   |-- crc.c : Main crc logic found here (NOT MINE)
|   |-- crc.h : Header file for crc (can choose different types of crc) (NOT MINE)
|   |-- crc_main.c : Test code for the crc files (NOT MINE)
|   |-- funcs.c : Contains functions shared betweens client and server
|   |-- funcs.h : Contains function prototypes for funcs.c
|   `-- defines.h : Where we put our #define statements for both client and server
|-- server
|   |-- makefile : 'make' to compile 'server.c', 'make clean' to remove all the junk
|   |-- uftp_server.h : C header file for the server (UDP)
|   `-- server.c  : C file for the server (UDP)
|-- skeleton : The result of extracting the tar file 'PA1_udp_example', containing
|   |          a template for PA1 and the files we're gonna send
|   |-- foo1
|   |-- foo2
|   |-- foo3
|   |-- udp_client.c
|   `-- udp_server.c
`-- testcode : A directory I made to test shit as I was working on the project
    |-- ProtoNum.c : C file for printing the protocol number of a protocol from string
    |                (NOTE: this didn't end up being useful information)
    |-- crc.c : Main crc logic found here (NOT MINE)
    |-- crc.h : Header file for crc (can choose different types of crc) (NOT MINE)
    |-- crc_main.c : Test code for the crc files (NOT MINE)
    `-- makefile : Compiles everything in this directory (currently only one thing)
```

## uftp_server.c

* Binds to a socket provided on the command line
* Performs operations (get/put/ls/delete) given from the client

## uftp_client.c

* Gets the socket for the given IP and Port
* Asks user for input for various operations (get/put/ls/delete) to send to the server

## crc_main.c, crc.c, and crc.h

* This code was found online through the CRC Wikipedia, it is free to download here:
* https://sourceforge.net/projects/crccalculator/
* It is written by Francisco Javier Lana Romero
* It is used in this assignment to calculate the CRC32 of each packet on the client and server

## To run:

* You only need to have the `client`, `server`, and `shared` directories locally
* Run `make clean` and then `make` in both the `client` and `server` directories
* In the `server` directory, run `./server.exe [SERVER PORT]`
* In the `client` directory, run `./client.exe [SERVER IP] [SERVER PORT]`

## Design Decisions:

* We tried our very best to make the code work universally. Given the nature of the assignment, we couldn't cover every edge case of networking; however, we did learn A LOT about network systems :).
* We implemented send and wait as our base reliability protocol.
* On top of send-and-wait we implemented CRC32, to ensure no bit corruption occurred.
* Furthermore, we have extensive error checking in our code to guard against erroneous input.
