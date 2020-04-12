#ifndef DVPROTOCOL_H
#define DVPROTOCOL_H

#include <unordered_map>
#include "commons.h"

#define DV_TTL 45

class DVProtocol {
public:

    DVProtocol(unsigned short router_id_in) : router_id(router_id_in) {}
    
    DVProtocol() {}
    ~DVProtocol() {}


    bool update_DV_table_new_neighborcost(u_short neighbor_id, u_short prev, u_short cost, std::unordered_map <unsigned short, port_table_entry>& port);
    bool update_DV_ttl();
    bool update_DV_table_pack(u_short dest, u_short next, u_short cost);

    // store the router_id for itself
    unsigned short router_id;

    // DV table: key = router_id, value = entry
    std::unordered_map<unsigned short, DV_table_entry> DV_table;

    // forwarding table: key = router_id/dest_id, value = next_hop_id
    std::unordered_map<unsigned short, unsigned short> forwarding_table;


};

#endif