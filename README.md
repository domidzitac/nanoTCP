# TCP-Jia-Komiljon
A open-source TCP/IP implementation in C. Simulated using mininet

Current functionality:
  * Sending packets based on a fixed sending window size (WND size 10)
  * Reciever sending cumulative acks to server.
  * Moving the sending window and transmitting new packets on the sender
  * Buffering out of order packets on the reciever end.
  * Timeout mechanism on the server to re-send packets

---
How to run mininet.

```
sudo mn --link tc,bw=10,delay=10ms,loss=2
mininet> xterm h1 h2
```

Terminal node h1

`./rdt2.0/obj/rdt_receiver 60001 FILE_RCVD`

Terminal node h2

`./rdt2.0/obj/rdt_sender 10.0.0.1 60001 small_file.bin`

Verifying checksum of the two files

`cksum FILE_RCVD small_file.bin`

---

TODO: Congestion control and flow control.