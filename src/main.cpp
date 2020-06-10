#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <lmic.h>
#include <hal/hal.h>
#include <LowPower.h>
#include <Adafruit_NeoPixel.h>

#define BUTTON 3
#define LED 4
#define RED strip.Color(255, 0, 0)
#define GREEN strip.Color(0, 255, 0)
#define TIMEOUT 5000

int activeLED;
int i = 0;
unsigned long lastAlarm;
boolean checked = true;

int ma[] = {0, 13, 14, 27};
int ti[] = {1, 12, 15, 26};
int ke[] = {2, 11, 16, 25};
int to[] = {3, 10, 17, 24};
int pe[] = {4, 9, 18, 23};
int la[] = {5, 8, 19, 22};
int su[] = {6, 7, 20, 21};

RTC_DS1307 RTC;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, LED, NEO_GRB + NEO_KHZ800);

void interruptFunction();

/**
* LoRa
*/
const unsigned TX_INTERVAL = 60;

// Näitä käytetään ainoastaan OTAA-lähetykseen, joten tässä tyhjiä.
void os_getArtEui(u1_t *buf) {}
void os_getDevEui(u1_t *buf) {}
void os_getDevKey(u1_t *buf) {}

// LMIC-kirjaston lähetysfunktiota varten
static osjob_t sendjob;

// Draginon pinnikartta
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};

static const PROGMEM u1_t NWKSKEY[16] = {0x55, 0xDE, 0xCC, 0x35, 0x95, 0x47, 0xF8, 0xF4, 0x25, 0xAC, 0x6C, 0xBF, 0xA9, 0x65, 0x3E, 0x31};
static const u1_t PROGMEM APPSKEY[16] = {0x4D, 0x5A, 0x16, 0x8F, 0xD3, 0x63, 0x8F, 0xD1, 0x0B, 0xF1, 0xE4, 0x58, 0xAF, 0x08, 0x4C, 0xBE};
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
    uint8_t payload[value.length() + 1];
    value.getBytes(payload, value.length());
    Serial.println("Sending LoRa message.");
    LMIC_setTxData2(1, payload, sizeof(payload) - 1, 0);
    Serial.println(F("Packet queued"));
  }
}

void wakeup()
{
  Serial.println("Awake!");
  detachInterrupt(BUTTON);
  attachInterrupt(digitalPinToInterrupt(BUTTON), interruptFunction, FALLING);
}

/*
* Set color to led.
*/
void changeLEDColor(int led, uint32_t color)
{
  Serial.print("Changing color of LED ");
  Serial.print(led);
  Serial.println('.');
  strip.setPixelColor(led, color);
  strip.show();
}

void interruptFunction()
{
  Serial.println("Button pressed.");
  changeLEDColor(activeLED, 0);
  checked = true;
  detachInterrupt(digitalPinToInterrupt(BUTTON));
  delay(100);
  //LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void logButtonNotPressed(){
  Serial.println("Button not pressed in time. Sending LoRa-message.");
  changeLEDColor(activeLED,RED);
  detachInterrupt(digitalPinToInterrupt(BUTTON));
  checked = true;
  //do_send(&sendjob);
  //os_runloop_once(); // Tämä lähettää jonossa olevat viestit.
}

int getLEDindex(uint8_t hour)
{
  if(hour == 9) return 0;
  if(hour == 12) return 1;
  if(hour == 15) return 2;
  if(hour == 21) return 3;
  return -1;
}

void turnOnLED(DateTime date)
{
  uint8_t day = date.dayOfTheWeek();
  uint8_t hour = date.hour();
  uint8_t index = getLEDindex(hour);
  int x;
  if (index >= 0)
  {
    switch (day)
    {
    case 1:
      x = ma[index];
      break;
    case 2:
      x = ti[index];
      break;
    case 3:
      x = ke[index];
      break;
    case 4:
      x = to[index];
      break;
    case 5:
      x = pe[index];
      break;
    case 6:
      x = la[index];
      break;
    case 7:
      x = su[index];
      break;
    default:
      break;
    }

    if (x != activeLED)
    {
      activeLED = x;
      changeLEDColor(activeLED, GREEN);
      attachInterrupt(digitalPinToInterrupt(BUTTON), interruptFunction, FALLING);
      lastAlarm = millis();
      checked = false;
      Serial.print("It's ");
      Serial.print(hour);
      Serial.print(" o'clock now. Time to turn on LED ");
      Serial.print(x);
      Serial.println('.');
    }
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("Setup complete. Welcome!");

  pinMode(BUTTON, INPUT);

  /**
   * RTC-Setup
   */
  Wire.begin();
  RTC.begin();
  if (!RTC.isrunning())
  {
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

  /*
  * LED strip setup
  */
  strip.begin();
  strip.setBrightness(10);
  strip.show(); //Initialize pixels to 'off'
}

boolean print = true;
void loop()
{
  DateTime now = RTC.now();
  turnOnLED(now);

  unsigned long timeNow = millis();
  if(timeNow - lastAlarm >= TIMEOUT && !checked){
    logButtonNotPressed();
    checked = true;
  }
}