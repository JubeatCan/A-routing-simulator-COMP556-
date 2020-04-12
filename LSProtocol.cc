#ifndef DVPROTOCOL_CC
#define DVPROTOCOL_CC

#include <arpa/inet.h>
#include <cstring>
#include "LSProtocol.h"
#include "Node.h"

struct edge {
    uint16_t prev_id;
    uint16_t cost;
};

LSProtocol::LSProtocol(uint16_t router_id, uint16_t num_ports, Node* sys, 
                std::unordered_map<uint16_t, port_table_entry>* port_table){
    this->router_id = router_id;
    this->num_ports = num_ports;
    this->sys = sys;
    this->port_table = port_table;

    seq_num = 0;
}

// send LSP to all its neighbors
void LSProtocol::sendLSPackets() {
    uint16_t size = port_table->size() * 4 + 3 * 4;
    
    // for all avaliable ports
    // if(router_id == 1){
    // std::cout << "this router id: " <<  router_id << std::endl;
    // for (auto it : (*port_table)){
    //     std::cout << "neigbhor id: " << it.first << std::endl;
    // }
    for (uint16_t i = 0; i < num_ports; i++){
        bool isAvaliable = false;
        for (auto &it : (*port_table)){
            if(it.second.port == i){
                isAvaliable = true;
                break;
            }
        }
        if(isAvaliable){
            // construct LSP
            char* sendBuffer = (char*)malloc(size);
            *sendBuffer = LS;
            *(uint16_t *)(sendBuffer + 2) = htons(size);
            *(uint16_t *)(sendBuffer + 4) = htons(router_id);
            *(uint32_t *)(sendBuffer + 8) = htonl(seq_num);
            seq_num++;

            int offset = 0;
            for (auto &it : *port_table){
                *(uint16_t *)(sendBuffer + 12 + offset * 4) = htons(it.first);
                *(uint16_t *)(sendBuffer + 14 + offset * 4) = htons(it.second.cost);
                offset++;
            }
            // std::cout << "send to router: " << id << std::endl;
            sys->send(i, sendBuffer, size);
        }
    }
}

// When recv pong packet, update the graph if link cost changed/new link
void LSProtocol::handlePongPacket(uint16_t neighbor_id, uint16_t prev, uint16_t cost, uint16_t port){
    // no change
    if(prev == cost){
        // TODO: need to change TTL for PONG packet?
        return;
    }
    
    // New neighbor
    if(prev == INFINITY_COST){
        // if not in the graph
        // if(ls_table.find(neighbor_id) == ls_table.end()){
        //     // insert this neighbor to graph
        //     ls_table[router_id][neighbor_id] = {port, cost, LS_TTL};
        //     ls_table[neighbor_id][router_id] = {port, cost, LS_TTL};
        // }
        // insert this neighbor to the graph
        ls_table[router_id][neighbor_id] = {cost, LS_TTL};
        ls_table[neighbor_id][router_id] = {cost, LS_TTL};

        destinations.push_back(neighbor_id);
    }
    else{
        // in graph, new link cost: prev != cost
        // update the graph
        ls_table[router_id][neighbor_id] = {cost, LS_TTL};
        ls_table[neighbor_id][router_id] = {cost, LS_TTL};
    }

    // run Dijkstra algorithm, compute the forwarding table
    dijkstra();
    sendLSPackets();
}

void LSProtocol::handleLSPacket(uint16_t port, char * packet, uint16_t size){
    uint16_t source_id = ntohs(*(uint16_t *)(packet + 4));
    uint32_t source_seqNum = ntohl(*(uint32_t *)(packet + 8));

    // check if this source_seqNum is the latest
    if(seqNum_map.count(source_seqNum) && seqNum_map[source_id] >= source_seqNum){
        return;
    }

    // update the seqNum for source_id
    seqNum_map[source_id] = source_seqNum;

    uint32_t num_entries = (size - 12) / 4;
    std::unordered_map<uint16_t, uint16_t> mapEntries;
    int offset = 0;
    for (uint32_t i = 0; i < num_entries; i++){
        uint16_t entry_id = ntohs(*(uint16_t *)(packet + 12 + offset * 4));
        uint16_t entry_cost = ntohs(*(uint16_t *)(packet + 14 + offset * 4));
        offset++;
        mapEntries[entry_id] = entry_cost;

        // update the graph
        ls_table[source_id][entry_id] = {entry_cost, LS_TTL};
        ls_table[entry_id][source_id] = {entry_cost, LS_TTL};
    }

    // delete dying links
    for (auto it : ls_table[source_id]){
        if(mapEntries.find(it.first) == mapEntries.end()){
            ls_table[source_id].erase(it.first);
            ls_table[it.first].erase(source_id);
        }
    }

    dijkstra();

    // now dispatch this packet to all other neighbors except where it comes from
    for (uint16_t i = 0; i < num_ports; i++){
        bool isAvaliable = false;
        for (auto &it : *port_table){
            if(it.second.port == i && it.second.port != port){
                isAvaliable = true;
                break;
            }
        }
        if(isAvaliable){
            // construct LSP
            char* sendBuffer = (char*)malloc(size);
            memcpy(sendBuffer, packet, size);
            sys->send(i, sendBuffer, size);
        }
    }
    free(packet);
}

void LSProtocol::checkEntriesTTL(){
    bool hasNabrChanged = false;
    std::unordered_map <unsigned short, port_table_entry>::iterator it = port_table->begin();
    // for (auto it : (*port_table)){
    //     std::cout << "neighbor id: " << it.first << " TTL: " << it.second.TTL << std::endl;
    // }
    while(it != port_table->end()){
        it->second.TTL--;

        if(it->second.TTL == 0){
            hasNabrChanged = true;
            uint16_t erase_id = it->first;
            it = port_table->erase(it);

            // update the graph
            ls_table[router_id].erase(erase_id);
            ls_table[erase_id].erase(router_id);

            // if none of nodes can reach erase_id node for now
            if(ls_table[erase_id].size() == 0){
                // erase this dest in destinations
                update_dests(erase_id);
            }
        }
        else {
            ++it;
        }
    }

    // check timeout for ls_table
    bool hasLSChanged = update_LS_TTL();

    // recompute shortest path
    if(hasNabrChanged || hasLSChanged){
        dijkstra();
        sendLSPackets();
    }
}

void LSProtocol::update_dests(uint16_t erase_id){
    // erase this dest in destinations
    auto list_iter = destinations.begin();
    while(list_iter != destinations.end()){
        if(*list_iter == erase_id){
            destinations.erase(list_iter);
            break;
        }
        ++list_iter;
    }
}

bool LSProtocol::update_LS_TTL(){
    bool hasChanged = false;

    std::unordered_map<uint16_t, std::unordered_map<uint16_t, ls_table_entry>>::iterator source;
    source = ls_table.begin();
    while(source != ls_table.end()){
        uint16_t source_id = source->first;
        std::unordered_map<uint16_t, ls_table_entry>::iterator dest;
        dest = ls_table[source_id].begin();
        while(dest != ls_table[source_id].end()){
            uint16_t dest_id = dest->first;
            ls_table[source_id][dest_id].TTL--;

            if(ls_table[source_id][dest_id].TTL == 0){
                // erase this edge
                dest = ls_table[source_id].erase(dest);
                ls_table[dest_id].erase(source_id);
                hasChanged = true;

                // if none of nodes can reach erase_id node for now
                if(ls_table[source_id].size() == 0){
                    update_dests(source_id);
                }
                if(ls_table[dest_id].size() == 0){
                    update_dests(dest_id);
                }
            }
            else {
                ++dest;
            }
        }
        ++source;
    }
    return hasChanged;
}

void LSProtocol::dijkstra(){
    forwarding_table.clear();

    if(port_table->empty()){
        return;
    }
    
    // pair: <dest_id, cost>
    auto comparator = [](pair<uint16_t, uint16_t> lhs, pair<uint16_t, uint16_t> rhs) {
        return lhs.second > rhs.second;
    };

    // <dest_id, cost>
    std::priority_queue< pair<uint16_t, uint16_t>, std::vector<pair<uint16_t, uint16_t>>, decltype(comparator)> pqueue(comparator);
    // <dest_id, edge>
    std::unordered_map<uint16_t, edge> resultMap;

    std::unordered_map<uint16_t, bool> hasVisited;

    // std::cout << "destinations" << std::endl;
    // for (auto e : destinations){
    //     std::cout << e << " ";
    // }
    // std::cout << std::endl;

    // initialize resultMap
    for (auto dest_id : destinations){
        resultMap[dest_id] = {router_id, INFINITY_COST};
    }

    pqueue.push(make_pair(router_id, 0));
    resultMap[router_id] = {router_id, 0};

    while(!pqueue.empty()){
        auto curNode = pqueue.top();
        pqueue.pop();

        uint16_t curNode_id = curNode.first;

        if(hasVisited.find(curNode_id) != hasVisited.end()){
            continue;
        }
        hasVisited[curNode_id] = true;
        
        // for all adjacent nodes of curNode
        std::unordered_map<uint16_t, ls_table_entry> &adjNodes = ls_table[curNode_id];
        for (auto const &adj : adjNodes){
            uint16_t adjNode_id = adj.first;
            uint16_t direct_cost = ls_table[curNode_id][adjNode_id].cost;

            if(resultMap[adjNode_id].cost > resultMap[curNode_id].cost + direct_cost){
                resultMap[adjNode_id].cost = resultMap[curNode_id].cost + direct_cost;
                pqueue.push(make_pair(adjNode_id, resultMap[adjNode_id].cost));
            }
        }
    }

    // update forwarding table
    for (uint16_t dest_id : destinations){
        uint16_t temp_id = dest_id;
        while(resultMap[temp_id].prev_id != router_id){
            temp_id = resultMap[temp_id].prev_id;
        }
        
        forwarding_table[dest_id] = temp_id;
    }
}

void LSProtocol::print_table(){
    std::cout << "forwarding table: dest -> nextHop" << std::endl;
    for (auto it : forwarding_table){
        std::cout << it.first << " -> " << it.second << std::endl;
    }

    std::cout << "ls_table: " << std::endl;
    for (auto it : ls_table){
        for (auto e : ls_table[it.first]){
            std::cout << it.first << " -> " << e.first << " TTL: " <<  e.second.TTL << std::endl;
        }
    }
}

#endif