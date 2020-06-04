#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <lmic.h>
#include <hal/hal.h>

#define BUTTON 3
#define LED 4

int i = 1;

RTC_DS1307 RTC;

/**
* LoRa
*/ 
const unsigned TX_INTERVAL = 60;

// Näitä käytetään ainoastaan OTAA-lähetykseen, joten tässä tyhjiä.
void os_getArtEui(u1_t *buf){}
void os_getDevEui(u1_t *buf){}
void os_getDevKey(u1_t *buf){}

// LMIC-kirjaston lähetysfunktiota varten
static osjob_t sendjob;

 // Draginon pinnikartta
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};

static const PROGMEM u1_t NWKSKEY[16] = {0x55,0xDE,0xCC,0x35,0x95,0x47,0xF8,0xF4,0x25,0xAC,0x6C,0xBF,0xA9,0x65,0x3E,0x31};
static const u1_t PROGMEM APPSKEY[16] = {0x4D,0x5A,0x16,0x8F,0xD3,0x63,0x8F,0xD1,0x0B,0xF1,0xE4,0x58,0xAF,0x08,0x4C,0xBE};
static const u4_t DEVADDR = 0x2601186D; // Laiteosoite

void onEvent(ev_t ev)
{
  Serial.print(os_getTime());
  Serial.println(": ");
  switch (ev)
  {
  case EV_SCAN_TIMEOUT:
    Serial.println(F("EV_SCAN_TIMEOUT"));
    break;
  case EV_BEACON_FOUND:
    Serial.println(F("EV_BEACON_FOUND"));
    break;
  case EV_BEACON_MISSED:
    Serial.println(F("EV_BEACON_MISSED"));
    break;
  case EV_BEACON_TRACKED:
    Serial.println(F("EV_BEACON_TRACKED"));
    break;
  case EV_JOINING:
    Serial.println(F("EV_JOINING"));
    break;
  case EV_JOINED:
    Serial.println(F("EV_JOINED"));
    break;
  case EV_RFU1:
    Serial.println(F("EV_RFU1"));
    break;
  case EV_JOIN_FAILED:
    Serial.println(F("EV_JOIN_FAILED"));
    break;
  case EV_REJOIN_FAILED:
    Serial.println(F("EV_REJOIN_FAILED"));
    break;
  case EV_TXCOMPLETE:
    Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
    os_radio(RADIO_RST);
    // After successful send, set sending=0 and sleep.
    Serial.println("Sending message completed.");
    break;
  case EV_LOST_TSYNC:
    Serial.println(F("EV_LOST_TSYNC"));
    break;
  case EV_RESET:
    Serial.println(F("EV_RESET"));
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
    Serial.println(F("EV_RXCOMPLETE"));
    break;
  case EV_LINK_DEAD:
    Serial.println(F("EV_LINK_DEAD"));
    break;
  case EV_LINK_ALIVE:
    Serial.println(F("EV_LINK_ALIVE"));
    break;
  default:
    Serial.println(F("Unknown event"));
    break;
  }
}

void do_send(osjob_t *j)
{
  // Tarkistetaan, onko TX/RX-työ käynnissä. Jos on, ajastetaan seuraava lähetys.
  if (LMIC.opmode & OP_TXRXPEND)
  {
    Serial.println(F("OP_TXRXPEND, not sending"));
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
  }
  else
  {
    String value = "payload";
    uint8_t payload[value.length()+1];
    value.getBytes(payload, value.length());
    Serial.println("Sending LoRa message.");
    LMIC_setTxData2(1, payload, sizeof(payload) - 1, 0);
    Serial.println(F("Packet queued"));
  }
}

void interruptFunction(){
  Serial.println("Button pressed. Sending LoRaWAN message.");
  detachInterrupt(digitalPinToInterrupt(BUTTON));
  do_send(&sendjob);
}

void setup(){
  Serial.begin(9600);
  Serial.println("Setup complete. Welcome!"); 

  pinMode(BUTTON, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), interruptFunction, FALLING);

  /**
   * RTC-Setup
   */
  Wire.begin();
  RTC.begin();
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
  }
  // following line sets the RTC to the date & time this sketch was compiled
  RTC.adjust(DateTime(__DATE__, __TIME__)); //TODO: Uncomment after battery is inserted

  /**
   * LoRa setup
   */
  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession(0x1, DEVADDR, nwkskey, appskey);

  //Kytketään linkin tarkitus pois päältä.
  LMIC_setLinkCheckMode(0);

  // The Things Network käyttää SF9 RX2 ikkunnassa
  LMIC.dn2Dr = DR_SF9;

  //Asetetaan lähetyksen vahvuus (SF)
  LMIC_setDrTxpow(DR_SF7, 14);
}

void loop(){

  os_runloop_once(); // Tämä lähettää jonossa olevat viestit.
  delay(10000);
}