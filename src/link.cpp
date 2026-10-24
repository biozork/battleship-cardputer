#include "link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Both devices must use the same WiFi channel. No router is involved.
static const uint8_t CHANNEL = 1;
// First bytes of every packet, so we ignore other ESP-NOW gadgets nearby.
static const uint8_t MAGIC = 0xB5, VERSION = 1;

static const uint32_t HELLO_EVERY = 300;        // ms between HELLO broadcasts while searching
static const uint32_t HEARTBEAT_EVERY = 1000;   // ms between heartbeats once connected
static const uint32_t RESEND_EVERY = 120;       // ms to wait for an ack before resending
static const uint32_t PAIRING_TIMEOUT = 2000;   // give up on a half-made pairing after this
static const uint32_t LOST_TIMEOUT = 3500;      // partner silent this long = partner lost

// What actually goes over the air.
struct __attribute__((packed)) Packet {
  uint8_t magic, version, type;
  uint16_t seq;  // reliable messages: their number. ACK: the number being acknowledged.
  uint8_t len;
  uint8_t data[LINK_MAX_DATA];
};
static const int HEADER = sizeof(Packet) - LINK_MAX_DATA;

struct __attribute__((packed)) Hello {
  char name[16];
  uint8_t want[6];  // MAC of the partner the sender is pairing with, or all zero
};

// A packet handed from the radio to loop().
struct Incoming {
  uint8_t mac[6];
  int len;
  uint8_t bytes[sizeof(Packet)];
};

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t NOBODY[6] = {0, 0, 0, 0, 0, 0};

// Searching: shouting HELLO to anyone. Pairing: picked a partner, waiting for
// it to pick us back. Connected: both sides agree.
enum class State { Searching, Pairing, Connected };
static State state = State::Searching;

static QueueHandle_t rxQueue;
static uint8_t myMac[6], peer[6];
static char myNameBuf[16], peerNameBuf[16];
static uint32_t lastHeard, lastHello, lastHeartbeat;
static LinkStats st;

// Reliable sending: a small queue, one message in the air at a time.
static const int QUEUE = 8;
static Message outbox[QUEUE], inbox[QUEUE];
static int outHead, outCount, inHead, inCount;
static bool inFlight;
static uint16_t nextSeq, inFlightSeq, lastSeqIn;
static uint32_t inFlightSince, lastTry;

// Runs in the WiFi driver's task, not in loop(), so it only copies the
// packet into a queue for update() to deal with.
static void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
  if (len < HEADER || len > (int)sizeof(Packet) || data[0] != MAGIC || data[1] != VERSION) return;
  Incoming in;
  memcpy(in.mac, mac, 6);
  in.len = len;
  memcpy(in.bytes, data, len);
  xQueueSend(rxQueue, &in, 0);
}

static void rawSend(const uint8_t* to, uint8_t type, uint16_t seq, const void* data, uint8_t len) {
  Packet p;
  p.magic = MAGIC;
  p.version = VERSION;
  p.type = type;
  p.seq = seq;
  p.len = len;
  if (len) memcpy(p.data, data, len);
  esp_now_send(to, (const uint8_t*)&p, HEADER + len);
}

static void addPeer(const uint8_t* mac) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t info = {};
  memcpy(info.peer_addr, mac, 6);
  info.channel = CHANNEL;
  info.ifidx = WIFI_IF_STA;
  info.encrypt = false;
  esp_now_add_peer(&info);
}

static void sendHello() {
  Hello h = {};
  strncpy(h.name, myNameBuf, sizeof h.name - 1);
  memcpy(h.want, state == State::Searching ? NOBODY : peer, 6);
  rawSend(BROADCAST, MSG_HELLO, 0, &h, sizeof h);
  lastHello = millis();
}

// Forget all queued messages and sequence numbers: every connection starts fresh.
static void resetSession() {
  outHead = outCount = inHead = inCount = 0;
  inFlight = false;
  nextSeq = 1;
  lastSeqIn = 0;
}

static void dropPeer() {
  if (state != State::Searching) esp_now_del_peer(peer);
  state = State::Searching;
  memset(peer, 0, 6);
  peerNameBuf[0] = 0;
  resetSession();
}

static void pickPeer(const uint8_t* mac, const char* name, uint32_t now) {
  memcpy(peer, mac, 6);
  strncpy(peerNameBuf, name, sizeof peerNameBuf - 1);
  addPeer(peer);
  state = State::Pairing;
  lastHeard = now;
  sendHello();  // tell them right away that we picked them
}

static void becomeConnected(uint32_t now) {
  state = State::Connected;
  resetSession();
  st = {};
  lastHeard = now;
  lastHeartbeat = now - HEARTBEAT_EVERY;  // send the first heartbeat immediately
}

static LinkEvent handle(const Incoming& in, uint32_t now) {
  const Packet& p = *(const Packet*)in.bytes;
  bool fromPeer = state != State::Searching && memcmp(in.mac, peer, 6) == 0;

  if (p.type == MSG_HELLO) {
    if (p.len < sizeof(Hello)) return LinkEvent::None;
    Hello h;
    memcpy(&h, p.data, sizeof h);
    h.name[sizeof h.name - 1] = 0;
    bool wantsMe = memcmp(h.want, myMac, 6) == 0;
    bool wantsNobody = memcmp(h.want, NOBODY, 6) == 0;

    if (state == State::Searching) {
      // Only pair with someone who is free, or who already picked us.
      if (wantsMe || wantsNobody) {
        pickPeer(in.mac, h.name, now);
        if (wantsMe) {
          becomeConnected(now);
          return LinkEvent::Connected;
        }
      }
    } else if (fromPeer) {
      lastHeard = now;
      if (state == State::Pairing) {
        if (wantsMe) {
          becomeConnected(now);
          return LinkEvent::Connected;
        }
        if (!wantsNobody) dropPeer();  // they paired with someone else
      } else if (wantsMe) {
        lastHeartbeat = now - HEARTBEAT_EVERY;  // they haven't heard us yet: answer now
      } else {
        dropPeer();  // they restarted or gave up on us
        return LinkEvent::Lost;
      }
    }
    return LinkEvent::None;
  }

  if (!fromPeer) return LinkEvent::None;  // not our partner: ignore
  lastHeard = now;

  if (p.type == MSG_HEARTBEAT) {
    st.heartbeatsIn++;
    if (state == State::Pairing) {  // a heartbeat means they consider us connected
      becomeConnected(now);
      st.heartbeatsIn = 1;
      return LinkEvent::Connected;
    }
    return LinkEvent::None;
  }
  if (state != State::Connected) return LinkEvent::None;

  if (p.type == MSG_ACK) {
    if (inFlight && p.seq == inFlightSeq) {
      inFlight = false;
      st.acked++;
      st.lastRttMs = now - inFlightSince;
      outHead = (outHead + 1) % QUEUE;
      outCount--;
    }
    return LinkEvent::None;
  }

  // A game message. Always acknowledge, even a repeat: our earlier ack may have been lost.
  rawSend(peer, MSG_ACK, p.seq, nullptr, 0);
  if (p.seq <= lastSeqIn) {
    st.duplicates++;
    return LinkEvent::None;
  }
  lastSeqIn = p.seq;
  if (inCount < QUEUE) {
    Message& m = inbox[(inHead + inCount) % QUEUE];
    m.type = p.type;
    m.len = min(p.len, LINK_MAX_DATA);
    memcpy(m.data, p.data, m.len);
    inCount++;
  }
  return LinkEvent::None;
}

namespace Link {

void begin() {
  WiFi.mode(WIFI_STA);  // turns the radio on; we never join a network
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);  // no radio power saving: lower delay
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  WiFi.macAddress(myMac);
  snprintf(myNameBuf, sizeof myNameBuf, "Cardputer %02X%02X", myMac[4], myMac[5]);

  rxQueue = xQueueCreate(16, sizeof(Incoming));
  if (esp_now_init() != ESP_OK) Serial.println("ESP-NOW failed to start!");
  esp_now_register_recv_cb(onReceive);
  addPeer(BROADCAST);
}

LinkEvent update() {
  uint32_t now = millis();
  LinkEvent ev = LinkEvent::None;
  Incoming in;
  while (xQueueReceive(rxQueue, &in, 0) == pdTRUE) {
    LinkEvent e = handle(in, now);
    if (e != LinkEvent::None) ev = e;
  }

  switch (state) {
    case State::Searching:
      if (now - lastHello >= HELLO_EVERY) sendHello();
      break;

    case State::Pairing:
      if (now - lastHeard > PAIRING_TIMEOUT) dropPeer();
      else if (now - lastHello >= HELLO_EVERY) sendHello();
      break;

    case State::Connected:
      if (now - lastHeard > LOST_TIMEOUT) {
        dropPeer();
        return LinkEvent::Lost;
      }
      if (now - lastHeartbeat >= HEARTBEAT_EVERY) {
        rawSend(peer, MSG_HEARTBEAT, 0, nullptr, 0);
        lastHeartbeat = now;
      }
      if (outCount > 0) {
        Message& m = outbox[outHead];
        if (!inFlight) {
          inFlight = true;
          inFlightSeq = nextSeq++;
          inFlightSince = lastTry = now;
          st.sent++;
          rawSend(peer, m.type, inFlightSeq, m.data, m.len);
        } else if (now - lastTry >= RESEND_EVERY) {
          lastTry = now;
          st.resends++;
          rawSend(peer, m.type, inFlightSeq, m.data, m.len);
        }
      }
      break;
  }
  return ev;
}

bool connected() { return state == State::Connected; }
const char* myName() { return myNameBuf; }
const char* peerName() { return peerNameBuf; }
bool iAmLower() { return memcmp(myMac, peer, 6) < 0; }
const LinkStats& stats() { return st; }

bool send(uint8_t type, const void* data, uint8_t len) {
  if (state != State::Connected || outCount >= QUEUE || len > LINK_MAX_DATA) return false;
  Message& m = outbox[(outHead + outCount) % QUEUE];
  m.type = type;
  m.len = len;
  if (len) memcpy(m.data, data, len);
  outCount++;
  return true;
}

bool receive(Message& out) {
  if (inCount == 0) return false;
  out = inbox[inHead];
  inHead = (inHead + 1) % QUEUE;
  inCount--;
  return true;
}

}  // namespace Link
