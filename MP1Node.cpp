/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include "Log.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address)
{
    for (int i = 0; i < 6; i++)
    {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop()
{
    if (memberNode->bFailed)
    {
        return false;
    }
    else
    {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size)
{
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport)
{
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if (initThisNode(&joinaddr) == -1)
    {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if (!introduceSelfToGroup(&joinaddr))
    {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr)
{
    /*
	 * This function is partially implemented and may require changes
	 */
    //int id = *(int *)(&memberNode->addr.addr);
    //int port = *(short *)(&memberNode->addr.addr[4]);

    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    // node is up!
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr)
{
    MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if (0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr)))
    {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        MemberListEntry temp;
        int t_id;
        int t_port;
        memcpy(&t_id, &(this->memberNode->addr.addr[0]), sizeof(int));
        memcpy(&t_port, &(this->memberNode->addr.addr[4]), sizeof(short));
        memberNode->memberList.push_back(MemberListEntry(t_id, t_port, this->memberNode->heartbeat, this->memberNode->timeOutCounter)); //darmora
        cout << "Starting up group" << endl;
        memberNode->inGroup = true;
    }
    else
    { //darmora check how JOINREQ message is being sent
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *)malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg + 1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);
        cout << "Sending JOINREQ" << memberNode->heartbeat << endl;
        printAddress(&memberNode->addr);
        printAddress(joinaddr);
        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode()
{


    //inited(false), inGroup(false), bFailed(false), nnb(0), heartbeat(0), pingCounter(0), timeOutCounter(0)
    this->memberNode->inited = false;
    this->memberNode->inGroup = false;
    this->memberNode->nnb = 0;
    this->memberNode->heartbeat = 0;
    this->memberNode->timeOutCounter = 0;
    this->memberNode->pingCounter = TFAIL;
    initMemberListTable(this->memberNode);
    //darmora
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop()
{
    if (memberNode->bFailed)
    {
        return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if (!memberNode->inGroup)
    {
        return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages()
{
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while (!memberNode->mp1q.empty())
    {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */

//checks if present in membership table of this node and returns the index
int checkifpresentinmembershiptable(Member *memberNode, int input_id)
{
    for (int i = 0; i < memberNode->memberList.size(); i++)
    {
        if (memberNode->memberList[i].id == input_id)
        {
            return i;
        }
    }
    return -1;
}

bool MP1Node::recvCallBack(void *env, char *data, int size)
{
    /*
	 * Your code goes here
	 */
    if (data == NULL)
    {
        return 0;
    }
    //darmora
    //cout<<*data<<endl;
    MessageHdr *inmessage = reinterpret_cast<MessageHdr *>(data);
    //char* inbuffer =(char *)malloc(size * sizeof(char));

    string inputmessage(data);
    cout << inputmessage;
    if (inmessage->msgType == JOINREQ)
    {

        /*msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
        
#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif
        */

        cout << "JOIN REQ received" << endl;
        printAddress(&(this->memberNode->addr));

        Address inaddr;

        int id;
        short port;
        long heartbeat;
        memcpy(&inaddr.addr, data + sizeof(MessageHdr), sizeof(inaddr.addr));

        memcpy(&heartbeat, data + sizeof(MessageHdr) + 1 + sizeof(int) + sizeof(short), sizeof(long));

        memcpy(&id, &inaddr.addr[0], sizeof(int));
        memcpy(&port, &inaddr.addr[4], sizeof(short));

        printAddress(&inaddr);

        int ispresent = checkifpresentinmembershiptable(this->memberNode, id);

        if (ispresent == -1)
        {
            //add this to membership list

            int sizeofoutmessage = sizeof(MessageHdr) + sizeof(int) + sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
            MessageHdr *outmessage = (MessageHdr *)malloc(sizeofoutmessage * sizeof(char));
            int membershiplistsize = 1;
            outmessage->msgType = JOINREP;

            memcpy((char *)(outmessage + 1), &membershiplistsize, sizeof(int));
            memcpy((char *)(outmessage + 1) + sizeof(int), &id, sizeof(int));
            memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int), &port, sizeof(short));
            memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int) + sizeof(short), &heartbeat, sizeof(long));
            memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int) + sizeof(short) + sizeof(long), &(this->memberNode->timeOutCounter), sizeof(long));

            for (int i = 0; i < this->memberNode->memberList.size(); i++)
            {
                Address outaddr;
                memcpy(&outaddr.addr[0], &this->memberNode->memberList[i].id, sizeof(int));
                memcpy(&outaddr.addr[4], &this->memberNode->memberList[i].port, sizeof(short));
                emulNet->ENsend(&(this->memberNode->addr), &outaddr, (char *)outmessage, sizeofoutmessage);
            }
            free(outmessage);

            MemberListEntry newmember = MemberListEntry(id, port, heartbeat, this->memberNode->timeOutCounter);
            this->memberNode->memberList.push_back(newmember);
            log->logNodeAdd(&(this->memberNode->addr), &inaddr);
            cout << "Node added";
            printAddress(&(this->memberNode->addr));
            printAddress(&inaddr);
        }

        //now prepare a JOINREP message and reply with this list
        int membershiplistsize = (int)this->memberNode->memberList.size();
        int membershipentrysize = sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
        int sizeofoutmessage = sizeof(MessageHdr) + sizeof(int) + (membershiplistsize * membershipentrysize) + 1;
        MessageHdr *outmessage = (MessageHdr *)malloc(sizeofoutmessage * sizeof(char));

        outmessage->msgType = JOINREP;

        memcpy((char *)(outmessage + 1), &membershiplistsize, sizeof(int));

        for (int i = 0; i < membershiplistsize; i++)
        {
            int offset = sizeof(int) + i * membershipentrysize;
            memcpy((char *)(outmessage + 1) + offset, &(this->memberNode->memberList[i].id), sizeof(int));
            memcpy((char *)(outmessage + 1) + offset + sizeof(int), &(this->memberNode->memberList[i].port), sizeof(short));
            memcpy((char *)(outmessage + 1) + offset + sizeof(int) + sizeof(short), &(this->memberNode->memberList[i].heartbeat), sizeof(long));
            memcpy((char *)(outmessage + 1) + offset + sizeof(int) + sizeof(short) + sizeof(long), &(this->memberNode->memberList[i].timestamp), sizeof(long));
        }
        if (!isNullAddress(&inaddr))
        {
            emulNet->ENsend(&(this->memberNode->addr), &inaddr, (char *)outmessage, sizeofoutmessage); //send message fromw which we receive JOINREQ
        }
        free(outmessage);

        //JOINREP
    }

    else if (inmessage->msgType == JOINREP)
    {

        this->memberNode->inGroup = true;
        cout << "Received JOIN REP Message";
        printAddress(&(this->memberNode->addr));

        int size;
        memcpy(&size, data + sizeof(MessageHdr), sizeof(int));
        int id;
        short port;
        long heartbeat;
        long timestamp;
        MemberListEntry temp;
        int membershipentrysize = sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
        for (int i = 0; i < size; i++)
        {
            int offset = sizeof(MessageHdr) + sizeof(int) + i * membershipentrysize;
            Address inaddr;

            memcpy(&id, data + offset, sizeof(int));
            memcpy(&port, data + offset + sizeof(int), sizeof(short));
            memcpy(&heartbeat, data + offset + sizeof(int) + sizeof(short), sizeof(long));
            memcpy(&timestamp, data + offset + sizeof(int) + sizeof(short) + sizeof(long), sizeof(long));
            int isPresent = checkifpresentinmembershiptable(this->memberNode, id);
            if (isPresent == -1)
            {
                memcpy(&inaddr.addr[0], &id, sizeof(int));
                memcpy(&inaddr.addr[4], &port, sizeof(short));

                MemberListEntry temp(id, port, heartbeat, timestamp);
                this->memberNode->memberList.push_back(temp);
                cout << "Added to membershiplist " << endl;
                printAddress(&this->memberNode->addr);
                printAddress(&inaddr);
                log->logNodeAdd(&(this->memberNode->addr), &inaddr);
            }
        }
    }

    else if (inmessage->msgType == HEARTBEAT)
    {
        int size;
        memcpy(&size, data + sizeof(MessageHdr), sizeof(int));
        int id;
        short port;
        long heartbeat;
        long timestamp;
        MemberListEntry temp;
        int membershipentrysize = sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
        for (int i = 0; i < size; i++)
        {
            int offset = sizeof(MessageHdr) + sizeof(int) + i * membershipentrysize;
            Address inaddr;

            memcpy(&id, data + offset, sizeof(int));
            memcpy(&port, data + offset + sizeof(int), sizeof(short));
            memcpy(&heartbeat, data + offset + sizeof(int) + sizeof(short), sizeof(long));
            memcpy(&timestamp, data + offset + sizeof(int) + sizeof(short) + sizeof(long), sizeof(long));
            int isPresent = checkifpresentinmembershiptable(this->memberNode, id);
            if (isPresent == -1)
            {
                MemberListEntry temp(id, port, heartbeat, timestamp);
                memcpy(&inaddr.addr[0], &id, sizeof(int));
                memcpy(&inaddr.addr[4], &port, sizeof(short));
                this->memberNode->memberList.push_back(temp);
                cout << "Added to membershiplist " << endl;
                printAddress(&this->memberNode->addr);
                printAddress(&inaddr);
                log->logNodeAdd(&(this->memberNode->addr), &inaddr);
            }
            else
            { //update hearbeat and timeoutcounter
                this->memberNode->memberList[isPresent].heartbeat = heartbeat;
                this->memberNode->memberList[isPresent].timestamp = this->memberNode->timeOutCounter;
            }
        }
    }

    return 0;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps()
{

    /*
	 * Your code goes here
	 */

    //check for the latest heartbeats received to this Node

    int cur_id;
    int cur_port;

    this->memberNode->heartbeat++;
    memcpy(&cur_id, &this->memberNode->addr.addr[0], sizeof(int));
    memcpy(&cur_port, &this->memberNode->addr.addr[4], sizeof(short));

    for (int i = 0; i < this->memberNode->memberList.size();)
    {

        if (this->memberNode->timeOutCounter - this->memberNode->memberList[i].timestamp > TREMOVE && cur_id != this->memberNode->memberList[i].id)
        {
            Address removeaddr;
            memcpy(&removeaddr.addr[0], &this->memberNode->memberList[i].id, sizeof(int));
            memcpy(&removeaddr.addr[4], &this->memberNode->memberList[i].port, sizeof(short));
            log->logNodeRemove(&this->memberNode->addr, &removeaddr);
            this->memberNode->memberList.erase(this->memberNode->memberList.begin() + i);
        }
        else
        {
            i++;
        }
    }

    if (this->memberNode->pingCounter == 0)
    {
        int sizeofoutmessage = sizeof(MessageHdr) + sizeof(int) + sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);
        MessageHdr *outmessage = (MessageHdr *)malloc(sizeofoutmessage * sizeof(char));
        int membershiplistsize = 1;
        outmessage->msgType = HEARTBEAT;

        memcpy((char *)(outmessage + 1), &membershiplistsize, sizeof(int));
        memcpy((char *)(outmessage + 1) + sizeof(int), &cur_id, sizeof(int));
        memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int), &cur_port, sizeof(short));
        memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int) + sizeof(short), &this->memberNode->heartbeat, sizeof(long));
        memcpy((char *)(outmessage + 1) + sizeof(int) + sizeof(int) + sizeof(short) + sizeof(long), &(this->memberNode->timeOutCounter), sizeof(long));

        for (int i = 0; i < this->memberNode->memberList.size(); i++)
        {
            Address outaddr;
            memcpy(&outaddr.addr[0], &this->memberNode->memberList[i].id, sizeof(int));
            memcpy(&outaddr.addr[4], &this->memberNode->memberList[i].port, sizeof(short));
            emulNet->ENsend(&(this->memberNode->addr), &outaddr, (char *)outmessage, sizeofoutmessage);
        }
        free(outmessage);
        this->memberNode->pingCounter = TFAIL;
    }

    else
    {
        this->memberNode->pingCounter--;
    }

    this->memberNode->timeOutCounter++;
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr)
{
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress()
{
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode)
{
    memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n", addr->addr[0], addr->addr[1], addr->addr[2],
           addr->addr[3], *(short *)&addr->addr[4]);
}
