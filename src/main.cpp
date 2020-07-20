#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <lmic.h>
#include <hal/hal.h>
#include <LowPower.h>
#include <Adafruit_NeoPixel.h>
#include <keys.h>

#define BUTTON 3
#define LED 4
#define RED strip.Color(255, 0, 0)
#define GREEN strip.Color(0, 255, 0)
#define TIMEOUT 5000

//Periferals
RTC_DS1307 RTC;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, LED, NEO_GRB + NEO_KHZ800);

//Global parameters
/**
* LoRa
*/
const unsigned TX_INTERVAL = 60;

//Used only for OTAA, left empty here.
void os_getArtEui(u1_t *buf) {}
void os_getDevEui(u1_t *buf) {}
void os_getDevKey(u1_t *buf) {}

// LMIC-library
static osjob_t sendjob;

// Dragin pinmap
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};

//Prototypes
void interruptFunction();
void changeLEDColor();
void logEvent();
void do_send();
void beginAlarm();

//Main
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

void loop()
{
}

//Other functions
void do_send(osjob_t *j)
{
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

/*
* Function called when interrupt is evoked.
*/
void interruptFunction()
{
    Serial.println("Interrupt!");
}

/*
* Changes LED color on strip.
*/
void changeLEDColor(int led, uint32_t color)
{
    Serial.print("Changing color of LED ");
    Serial.print(led);
    Serial.println('.');
    strip.setPixelColor(led, color);
    strip.show();
}

/*
* Logs events.
*/
void logEvent(String info)
{
    Serial.print("Event occured. Logging info: ");
    Serial.println(info);
}

/*
* Turns on alarm.
*/
void beginAlarm()
{
    Serial.print("Alarm Time!");
}