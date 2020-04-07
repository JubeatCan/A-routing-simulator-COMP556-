#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "DVProtocol.h"
#include <unordered_map>

#define PORT_ENTRY_LIVE 15  // seconds
#define DV_ENTRY_LIVE 45    // seconds
#define LS_ENTRY_LIVE 45    // seconds

#define CHECK_ENTRY_FREQ 1000
#define PING_PONG_FREQ 10000
#define UPDATE_FREQ 30000

#define PING_SIZE 12

typedef unsigned short u_short;


enum eAlarmType {
    ALARM_CHECK_ENTRY,
    ALARM_PING_PONG_EXCHANGE,
    ALARM_DV_UPDATE,
    ALARM_LS_UPDATE
};

struct port_table_entry {
    unsigned short port;    // port num to connect
    unsigned short cost;    // RTT btw two routers
    unsigned short TTL;     // time to live
};


class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces 
    
    unsigned short router_id;
    unsigned short num_ports;
    eProtocolType protocol_type;

    // right now I put all entires check into one alarm, can be different events, will see how impl goes
    // eAlarmType alarm_port_entry_check;
    // eAlarmType alarm_dv_entry_check;
    // eAlarmType alarm_ls_entry_check;
    eAlarmType check_entry;
    eAlarmType ping_pong_exchange;
    eAlarmType dv_update;
    eAlarmType ls_update;

    DVProtocol dv;
    // TODO: add LS protocol later

    // port status table(track neighbor status): key = neighborID, value = entry
    std::unordered_map <unsigned short, port_table_entry> port_table;

    // DV table: key = router_id, value = entry
    std::unordered_map <unsigned short, DV_table_entry> DV_table;

    // forwarding table: key = router_id/dest_id, value = port_id
    // move into DVProtocol or LSProtocol, since unpdate in DV_Table will affect forwarding table immediately

    /* helper functions to deal with different alarm_type and packet_type */

    // forward data to best neighbor using forwarding table
    void forwardData(u_short port, char * packet, u_short size);
    
    // get sourceID, generate PONG packet and send out, using the same port
    void recvPingPacket(u_short port, char * packet, u_short size);

    // get sourceID, calculate RTT, update port status table
    void recvPongPacket(u_short port, char * packet, u_short size);

    // update DV table
    void recvDVPacket(u_short port, char * packet, u_short size);

    // update LS table
    void recvLSPacket(u_short port, char * packet, u_short size);

    // check whether entries in port table and DV/LS table expire 
    void checkTableEntries();

    // send PING packet to all ports 
    void sendPingToAllPorts();

    // send all DV entries to all neighbors
    void sendDVEntriesToNeighbors();

    // send all LS entries to all neighbors
    void sendLSEntriesToNeighbors();

};

#endif

