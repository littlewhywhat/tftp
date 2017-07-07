Simplified Trivial File Transfer Protocol
===============

C implementation of client-server file transfer
using BSD sockets through UDP protocol on IPv4/v6 and inspired by TFTP protocol 
for packet management.

Implementation is simplified because it doesn't include write/read request message from client to server. 

Here client always writes and server reads. One file can be sent during "session" and so it's necessary to run client, server again to send another file.

## Behaviour

Client and server agrees on maximal size of data in packet.

### Client

Client opens communication with server by specified host and service (port). It opens file to send and copies first chunk of it with max possible size that is not bigger than agreed maximal size of data in packet to the buffer. 

It assigns id value of it to own counter (initialised with 0 at first) and sends packet with id and file chunk from buffer to server. 

It waits then to receive an acknowledgement from server. If timeout expires, client exits with error. Otherwise, it checks id of received acknowledgement. If this id is not equal to counter value it sends this packet again. If ids are equal it increments the counter and proceeds to next chunk of file repeating previous steps.

If client reaches end of file it exits with no error.

Once server receives final package it will send acknowledgement only once and exit. It's possible then that client would miss it and end with error (Two Generals' Problem).

### Server

Server bounds to the specified port, opens file to write to and waits for a packet with id equal to its counter to come in (counter is initialised with 0 at first). 

If timeout expires server exits with error. On first successful receive server saves host and service (port) values of client to compare and verify in future on next receive. If these values don't match with received ones server will exit with error.

If received packet has different id from server's counter then this id is sent as an acknowledgement again to server (the id will always be less than server's counter value because of the way the client behaves and therefore correspond to already received packet). 

If id is equal then server copies chunk from packet to buffer, sends acknowledgement to client and increments its counter. If received chunk has maximal agreed size of data in packet then server waits for another packet repeating previous steps. If not, it means that there will be no more chunks and server ends with no error.

Here again if client sends final package that accidently has maximum agreed size then it will exit after it receives acknowledgement. However, server will proceed waiting for another package and end with error when timeout expires (Two Generals' Problem).

## Build

`make`

or

`make compile`

## Usage

`./bin/main [host] port file_path_to_recv_or_send`

