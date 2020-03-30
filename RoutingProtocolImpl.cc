#include "RoutingProtocolImpl.h"
#include "Node.h"
#include <arpa/inet.h>

const unsigned short PING_SIZE = 96;
const int PORT_STATUS_MONITOR = 1;

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  // remember configures
  this->num_ports = num_ports;
  this->router_id = router_id;
  this->protocol_type = protocol_type;

  // init port status vector
  port_status.resize(num_ports);
  for (auto &it : port_status) {
    it.first = INFINITY_COST;
    it.second = INFINITY_COST;
  }

  // send PING packet
  for (uint16_t i = 0; i < num_ports; i++) {
    char* sendBuffer = (char*)malloc(PING_SIZE);
    *sendBuffer = PING;
    *(uint16_t *)(sendBuffer + 2) = htons(PING_SIZE);
    *(uint16_t *)(sendBuffer + 4) = htons(router_id);
    *(uint32_t *)(sendBuffer + 8) = htonl(sys->time());
    sys->send(i, sendBuffer, PING_SIZE);
  }

  // fire alarm event
  sys->set_alarm(this, 10000, (void*)&PORT_STATUS_MONITOR);
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
  int type = *(int*)data;
  switch (type)
  {
  case PORT_STATUS_MONITOR: {
    // periodicly check neighbors, send PINGs
    for (uint16_t i = 0; i < num_ports; i++) {
      char* sendBuffer = (char*)malloc(PING_SIZE);
      *sendBuffer = PING;
      *(uint16_t *)(sendBuffer + 2) = htons(PING_SIZE);
      *(uint16_t *)(sendBuffer + 4) = htons(router_id);
      *(uint32_t *)(sendBuffer + 8) = htonl(sys->time());
      sys->send(i, sendBuffer, PING_SIZE);
    }
    
    sys->set_alarm(this, 10000, (void*)&PORT_STATUS_MONITOR);
    break;
  }
  
  default:
    break;
  }
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  char* recvBuffer = (char*)packet;
  int type = (int)(*recvBuffer);

  switch (type)
  {
  case PING:{
    // read where the PING message comes from
    uint16_t dest = ntohs(*(uint16_t*)(recvBuffer + 2));

    *recvBuffer = PONG;
    *(uint16_t *)(recvBuffer + 4) = htons(this->router_id);
    *(uint16_t *)(recvBuffer + 6) = htons(dest);

    // send the PONG message
    sys->send(port, recvBuffer, PING_SIZE);
    break;
  }
  
  case PONG:{
    uint32_t currentTime = sys->time();
    uint32_t timestamp = ntohl(*(uint32_t*)(recvBuffer + 8));
    // get the router id this PONG came from
    uint16_t dest_router = ntohs(*(uint16_t*)(recvBuffer + 4));
    
    // update the port status map
    // TODO: if duration exceed short limit?
    port_status[port] = make_pair(dest_router, timestamp - currentTime);

    // TODO: update DV/LS structure if necessary
    break;
  }
  
  default:
    break;
  }
}

// add more of your own code
