#ifndef DVPROTOCOL_C
#define DVPROTOCOL_C

#include <arpa/inet.h>
#include "DVProtocol.h"
#include <iostream>

bool DVProtocol::update_DV_table_new_neighborcost(u_short neighbor_id, u_short prev, u_short cost) {
    u_short infcost = 0xffff;
    bool flag = false;
    if (DV_table.find(neighbor_id) == DV_table.end()) {
        if (cost == infcost) {
            forwarding_table.erase(neighbor_id);
            flag = true;
        } else {
            DV_table.insert({neighbor_id, {cost, neighbor_id, DV_TTL}});
            forwarding_table.insert({neighbor_id, neighbor_id});
            flag = true;
        }
    } else if (DV_table.find(neighbor_id) != DV_table.end() && (prev != cost)) {
        if (cost == infcost) {
            auto it = DV_table.begin();
            while (it != DV_table.end()) {
                if (it->second.next_hop == neighbor_id) {
                    forwarding_table.erase(it->first);
                    it = DV_table.erase(it);
                    flag = true;
                } else {
                    ++it;
                }
            }
        } else {

            // Previously existed and cost updated, update the DV table
            for (auto& it : DV_table) {
                if (it.second.next_hop == neighbor_id) {
                    it.second.cost += (cost - prev);
                    it.second.TTL = DV_TTL;
                    flag = true;
                }
            }
        }
    }
    
    for (auto& it:forwarding_table) {
        std::cout << it.first << " " << it.second << std::endl;
    }
    std::cout << std::endl;

    return flag;
}

bool DVProtocol::update_DV_ttl() {
    bool flag = false;
    auto it = DV_table.begin();
    while (it != DV_table.end()) {
        it->second.TTL--;
        if (it->second.TTL == 0) {
            // erase forward table first
            forwarding_table.erase(it->first);
            it = DV_table.erase(it);
            flag = true;
        } else {
            ++it;
        }
    }

    return flag;
}

bool DVProtocol::update_DV_table_pack(u_short dest, u_short next, u_short cost) {
    DV_table[dest].cost = cost;
    DV_table[dest].next_hop = next;
    DV_table[dest].TTL = DV_TTL;

    forwarding_table[dest] = next;

    return true;
}

#endif