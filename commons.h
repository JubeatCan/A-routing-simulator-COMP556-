#ifndef COMMONS_H
#define COMMONS_H

struct port_table_entry {
    unsigned short port;    // port num to connect
    unsigned short cost;    // RTT btw two routers
    unsigned short TTL;     // time to live
};

struct DV_table_entry {
  unsigned short cost;      // minimun cost to reach that router
  unsigned short next_hop;  // next hop used to reach that router
  unsigned short TTL;       // time to live
};

struct ls_table_entry {
    unsigned short cost;      // minimun cost to reach that router
    unsigned short TTL;       // time to live
};

#endif