#ifndef LSPROTOCOL_H
#define LSPROTOCOL_H

#include <unordered_map>
#include <list>
#include "commons.h"
// #include "RoutingProtocolImpl.h"

#define LS_TTL 45

class Node;

class LSProtocol {

public:
    LSProtocol(uint16_t router_id, uint16_t num_ports, Node* sys, 
                std::unordered_map<uint16_t, port_table_entry> *port_table);

    LSProtocol() {};

    // send LSP to all its neighbors
    void sendLSPackets();

    // When recv pong packet, update the graph if link cost changed/new link
    void handlePongPacket(uint16_t neighbor_id, uint16_t prev, uint16_t cost, uint16_t port);
    
    // When recv LSP, update the graph, etc.
    void handleLSPacket(uint16_t port, char * packet, uint16_t size);

    // dijkstra algorithm, compute the shortest path to all destinations
    void dijkstra();

    // check dying links/nodes in the graph
    void checkEntriesTTL();

    bool update_LS_TTL();

    void update_dests(uint16_t erase_id);

    void print_table();

private:
    friend class RoutingProtocolImpl;
    // this router id
    uint16_t router_id;

    // number of ports
    uint16_t num_ports;

    // sequence number;
    uint32_t seq_num;

    Node* sys;

    // sequence number of other nodes <dest_id, seq_num>
    std::unordered_map<uint16_t, uint32_t> seqNum_map;

    // port status table 
    std::unordered_map<uint16_t, port_table_entry>* port_table;

    // LS graph table: source_id -> map<dest_id, entry>
    std::unordered_map<uint16_t, std::unordered_map<uint16_t, ls_table_entry>> ls_table;

    // forwarding table: key = router_id/dest_id, value = next_hop_id
    std::unordered_map<unsigned short, unsigned short> forwarding_table;
    
    // all known destinations
    std::list<uint16_t> destinations;
};

#endif