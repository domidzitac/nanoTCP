# TCP-Jia-Komiljon
A open-source TCP/IP implementation in C. Simulated using mininet

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
