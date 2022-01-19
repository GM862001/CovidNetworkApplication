# CovidNetworkApplication
## Peer-to-Peer Network Application implementing Covid-19 Data Sharing
### About
This is a university project (Computer Networks Course).  
Tasks of the project:
* Design of a peer-to-peer network architecture;
* Implementation of the network in C using the socket interface.

The design of the network is widely described in the documentation.  
A generic peer of the network is implemented in the file peer.c, while the Discovery Server is implemented in the file ds.c.  
The whole project, including the specifications and the documentation, is written in Italian.

#### How to Run the Projct
The project is designed to be run on a Debian 8 machine.  
To run the project is enough to move into the project directory and run the command `sh exec.sh` from the terminal. This command will boot the Discovery Server on port 4242 and five peers on ports 5001-5005.
