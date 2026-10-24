// The radio link to the other Cardputer, over ESP-NOW.
// Finds a partner, keeps checking that it's still there (heartbeat), and
// delivers game messages reliably: each one is numbered, acknowledged, and
// resent until the acknowledgement comes back.
#pragma once
#include <Arduino.h>

enum MsgType : uint8_t {
  // used inside the link itself
  MSG_HELLO = 1,
  MSG_HEARTBEAT,
  MSG_ACK,
  // game messages, delivered reliably and in order
  MSG_PING,
  MSG_READY,
  MSG_FIRE,
  MSG_RESULT,
  MSG_REMATCH,
};

const uint8_t LINK_MAX_DATA = 48;  // bytes of payload per message

struct Message {
  uint8_t type;
  uint8_t len;
  uint8_t data[LINK_MAX_DATA];
};

enum class LinkEvent { None, Connected, Lost };

struct LinkStats {
  uint32_t heartbeatsIn;  // heartbeats received from the partner
  uint32_t sent;          // reliable messages sent
  uint32_t acked;         // ...and acknowledged by the partner
  uint32_t resends;       // extra tries because a packet or its ack got lost
  uint32_t duplicates;    // repeats we received and ignored
  uint32_t lastRttMs;     // round trip (send until ack) of the last message
};

namespace Link {
void begin();
LinkEvent update();  // call every loop; reports when a partner is found or lost
bool connected();
const char* myName();
const char* peerName();
// True if our MAC address sorts below the partner's. Used as a fair tie-breaker
// that both devices agree on without talking.
bool iAmLower();
// Queue a reliable message for the partner. Returns false if not connected or the queue is full.
bool send(uint8_t type, const void* data = nullptr, uint8_t len = 0);
// Take the next message that arrived from the partner. Returns false if there is none.
bool receive(Message& out);
const LinkStats& stats();
}  // namespace Link
