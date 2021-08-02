# GossipBasedMemberShipProtocol
Implement a heartbeat(gossip) based membership protocol to detect every node join , failure and exist in a distributed system.

**Goals of the protocol**
* Satisify completeness all the time i.e. every non-faulty process detects every node join,failure and leave.
* Satisfy accuracy of failure detection when there are no message losses and message delays are small.
* Satisfy completeness when there are message losses.
* Optimal accuracy under message losses.

**Layers in the system**
* Application Layer 
* P2P (peer to peer) layer
* EmulNet (Emulated Network Layer)

The protocol runs on P2P layer.
*Each Node/Peer uses ENinit to initialise its own address
*Each Node uses ENsend and ENrecv to send and receive messages.

These functions are provided so that later it can be mapped to implementation that uses TCP sockets.


**Building the application**

* MakeFile is already provided and just need to run it using make
* To run the simulation, ./Application testcases/<test_name>.conf
* There are three test cases (1)Single Node Failure (2) Multiple Node failure (3) Single Node failure under a lossy network.

