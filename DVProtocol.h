#ifndef DVPROTOCOL_H
#define DVPROTOCOL_H

#include <unordered_map>

struct DV_table_entry {
  unsigned short cost;      // minimun cost to reach that router
  unsigned short next_hop;  // next hop used to reach that router
  unsigned short TTL;       // time to live
};


class DVProtocol {
public:

    DVProtocol(unsigned short router_id_in) : router_id(router_id_in) {}
    
    DVProtocol() {}
    ~DVProtocol() {}


    bool update_DV_table();

    // store the router_id for itself
    unsigned short router_id;

    // DV table: key = router_id, value = entry
    std::unordered_map<unsigned short, DV_table_entry> DV_table;

    // forwarding table: key = router_id/dest_id, value = next_hop_id
    std::unordered_map<unsigned short, unsigned short> forwarding_table;


};

#endif