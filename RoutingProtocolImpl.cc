#include "RoutingProtocolImpl.h"
#include "Node.h"
#include <arpa/inet.h>


RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
    sys = n;
    // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
    // remember configures
    this->num_ports = num_ports;
    this->router_id = router_id;
    this->protocol_type = protocol_type;
    
    check_entry = ALARM_CHECK_ENTRY;
    ping_pong_exchange = ALARM_PING_PONG_EXCHANGE;
    dv_update = ALARM_DV_UPDATE;
    ls_update = ALARM_LS_UPDATE;

    print_tables = PRINT;
    sendPingToAllPorts();

    sys->set_alarm(this, CHECK_ENTRY_FREQ, &check_entry);
    sys->set_alarm(this, PING_PONG_FREQ, &ping_pong_exchange);
    
    if (protocol_type == P_DV) {
        dv = DVProtocol(router_id);
        sys->set_alarm(this, UPDATE_FREQ, &dv_update);
    }

    else if (protocol_type == P_LS) {
        // TODO: add LS protocol later
    }

    sys->set_alarm(this, 10000, &print_tables);

}

void RoutingProtocolImpl::handle_alarm(void *data) {

    eAlarmType alarm_type = *((eAlarmType *) data);
    switch (alarm_type)
    {
        case ALARM_CHECK_ENTRY:
            /* check whether entries in port table and DV/LS table expire */
            checkTableEntries();
            sys->set_alarm(this, CHECK_ENTRY_FREQ, data);
            break;
        
        case ALARM_PING_PONG_EXCHANGE:
            /* send PING packets to all ports */
            sendPingToAllPorts();
            sys->set_alarm(this, PING_PONG_FREQ, data);
            break;
        
        case ALARM_DV_UPDATE:
            /* send all DV entries to all neighbors */
            sendDVEntriesToNeighbors();
            sys->set_alarm(this, UPDATE_FREQ, data);
            break;
        
        case ALARM_LS_UPDATE:
            /* send all LS entries to all neighbors */
            sendLSEntriesToNeighbors();
            sys->set_alarm(this, UPDATE_FREQ, data);
            break;
        
        case PRINT:
            printTables();
            sys->set_alarm(this, 10000, data);
            break;
            
        default:
            break;
    }

}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {

    ePacketType packet_type = (ePacketType)(* (char *) packet);
    switch (packet_type)
    {
        case DATA:
            forwardData(port, (char *) packet, size);
            break;

        case PING:
            recvPingPacket(port, (char *) packet, size);
            break;
        
        case PONG:
            recvPongPacket(port, (char *) packet, size);
            break;
        
        case DV:
            recvDVPacket(port, (char *) packet, size);
            break;
        
        case LS:
            recvLSPacket(port, (char *) packet, size);
            break;

        default:
            free(packet);
            break;
    }

}

// add more of your own code

void RoutingProtocolImpl::forwardData(u_short port, char * packet, u_short size) {
    u_short dest = ntohs(*(u_short *)(packet + 6));

    // if itself is the dest, the packet memory should be freed
    if (dest == router_id) {
        free(packet);
        return;
    }

    if (protocol_type == P_DV) {
        // if find an entry for the dest in forwarding table, send out use the nextHop
        if (dv.forwarding_table.find(dest) != dv.forwarding_table.end()) {
            u_short nextHop = dv.forwarding_table[dest];
            sys->send(port_table[nextHop].port, packet, size);
        }
        // if no entry found, free the packet
        else {
            free(packet);
        }
    }
    
    else if (protocol_type == P_LS) {
        // TODO: add LS handler

    }

}

void RoutingProtocolImpl::recvPingPacket(u_short port, char * packet, u_short size) {
    *packet = PONG;                                         // update packet type
    *(u_short *)(packet + 6) = *(u_short *)(packet + 4);    // copy srcID to destID
    *(u_short *)(packet + 4) = htons(router_id);            // update srcID to itself
    sys->send(port, packet, size);

}

void RoutingProtocolImpl::recvPongPacket(u_short port, char * packet, u_short size) {
    // get the router id this PONG came from
    u_short neighbor_id = ntohs(*(u_short *)(packet + 4));

    uint32_t currentTime = sys->time();
    uint32_t sentTime = ntohl(*(uint32_t *)(packet + 8));
    u_short cost = currentTime - sentTime;
    
    u_short prev = (port_table.find(neighbor_id) == port_table.end()) ? INFINITY_COST : port_table[neighbor_id].cost;
    
    // update the port status map
    // TODO: if duration exceed short limit?
    port_table[neighbor_id].port = port;
    port_table[neighbor_id].cost = cost;
    port_table[neighbor_id].TTL = PORT_ENTRY_LIVE;

    // TODO: update DV/LS structure if necessary
    if (protocol_type == P_DV) {
        if (dv.update_DV_table_new_neighborcost(neighbor_id, prev, cost)) {
            sendDVEntriesToNeighbors();
        }
    } else if (protocol_type == P_LS) {
        // TODO: LS
    }
}

void RoutingProtocolImpl::recvDVPacket(u_short port, char * packet, u_short size) {
    // TODO
    bool flag = false;
    u_short fromId = ntohs(*(u_short *)(packet + 4));
    if (dv.DV_table.find(fromId) == dv.DV_table.end()) {
        return;
    }
    unordered_map<u_short, u_short> destCostPair;
    for (int i = 0; i < (size-8)/4; i++) {
        destCostPair[(ntohs(*(u_short *)(packet + 8 + i*4)))] =  
                        (ntohs(*(u_short *)(packet + 8 + i*4 + 2)));
    }

    

    for (auto& p : destCostPair) {
        if (p.first == router_id) {
            continue;
        }

        if (dv.DV_table.find(p.first) == dv.DV_table.end()) {
            flag = (dv.update_DV_table_pack(p.first, fromId, dv.DV_table[fromId].cost + p.second));
        } else {
            if (dv.DV_table[p.first].cost > dv.DV_table[fromId].cost + p.second) {
                flag = dv.update_DV_table_pack(p.first, fromId, dv.DV_table[fromId].cost + p.second);
            }
        }
    }
    
    auto it = dv.DV_table.begin();
    while (it != dv.DV_table.end()) {
        if (it->second.next_hop == fromId && destCostPair.find(it->first) == destCostPair.end() && it->first != fromId) {
            dv.forwarding_table.erase(it->first);
            it = dv.DV_table.erase(it);
            flag = true;
        } else {
            ++it;
        }
    }

    auto it2 = dv.DV_table.begin();
    while (it2 != dv.DV_table.end()) {
        it2->second.TTL = DV_TTL;
        cout << it2->first << "DV: " << it2->second.cost << " " << it2->second.next_hop << endl;
        ++it2;
    }



    if (flag) {
        sendDVEntriesToNeighbors();
    }
}

void RoutingProtocolImpl::recvLSPacket(u_short port, char * packet, u_short size) {
    // TODO
}

void RoutingProtocolImpl::sendPingToAllPorts() {

    for (u_short i = 0; i < num_ports; i++) {
        char* sendBuffer = (char*)malloc(PING_SIZE);
        *sendBuffer = PING;
        *(u_short *)(sendBuffer + 2) = htons(PING_SIZE);
        *(u_short *)(sendBuffer + 4) = htons(router_id);
        *(uint32_t *)(sendBuffer + 8) = htonl(sys->time());
        sys->send(i, sendBuffer, PING_SIZE);
    }

}

void RoutingProtocolImpl::checkTableEntries() {
    // TODO: decrease TTL for each entry by 1, if TTL == 0, remove that entry
    // TODO: unpdate DV/LS table if necessary
    bool flag = false;
    std::unordered_map <unsigned short, port_table_entry>::iterator it = port_table.begin();
    while (it != port_table.end()) {
        it->second.TTL--;

        // if TTL is 0 now.
        if (it->second.TTL == 0) {
            // Update our DV first.
            // flag = dv.update_DV_table_new_neighborcost(it->first, it->second.cost, INFINITY_COST);
            if (flag) {
                dv.update_DV_table_new_neighborcost(it->first, it->second.cost, INFINITY_COST);
            } else {
                flag = dv.update_DV_table_new_neighborcost(it->first, it->second.cost, INFINITY_COST);
            }
            // Then remove this entry.
            it = port_table.erase(it);
        } else {
            ++it;
        }
    }

    // DV TTL Update
    // flag = (dv.update_DV_ttl() || flag);
    if (flag) {
        dv.update_DV_ttl();
    } else {
        flag = dv.update_DV_ttl();
    }

    if (flag) {
        sendDVEntriesToNeighbors();
    }

}

void RoutingProtocolImpl::sendDVEntriesToNeighbors() {
    u_short size = dv.DV_table.size() * 4 + 8;

    for (auto const &p: port_table) {
        u_short neighbor = p.first;
        
        char * packet = (char *)(malloc)(size);
        *packet = DV;
        *(u_short *)(packet + 2) = htons(size);
        *(u_short *)(packet + 4) = htons(router_id);
        *(u_short *)(packet + 6) = htons(neighbor);

        u_short offset = 8;
        
        // add all entries in DV_table to packet, be careful to deal with posion reverse
        for (auto const &d: dv.DV_table) {
            u_short dest = d.first;
            u_short nextHop = d.second.next_hop;

            *(u_short *)(packet + offset) = htons(dest);
            
            // posion reverse: if nextHop is the router we are going to send DV packet to, put infinity in the cost
            if (dest != neighbor && nextHop == neighbor) {
                *(u_short *)(packet + offset + 2) = htons(INFINITY_COST);
            }
            else {
                *(u_short *)(packet + offset + 2) = htons(d.second.cost);
            }

            offset += 4;
        }

        sys->send(p.second.port, packet, size);
    }

}


void RoutingProtocolImpl::sendLSEntriesToNeighbors() {
    // TODO
}

void RoutingProtocolImpl::printTables() {
    for(auto & it: port_table) {
        std::cout<< "PT: " << it.first << ": " << it.second.cost << std::endl;
    }

    for (auto & it: dv.DV_table) {
        std::cout<< "DV: " << it.first << ": " << it.second.next_hop << ", "<< it.second.cost << std::endl;
        
    }

}
