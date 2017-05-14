#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Konfiguration des LED Rings
#define LED_CONTROL     8
#define LED_COUNT       16

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_CONTROL, NEO_GRB + NEO_KHZ800);

/*
    LED Anzeige.
*/

typedef enum { LED_INIT = -1, LED_OFF, LED_START_ON, LED_START_OFF } ledDisplayMode;

ledDisplayMode ledMode = LED_INIT;

#define START_ON    500
#define START_OFF   200
#define AUTH_ON     2500

bool authOn = false;
int ledStart = -1;
int badAuth = -1;

// Einfarbige Anzeige.
void singleColor(uint8_t r, uint8_t g, uint8_t b) {
    // Ring in der gewünschten Farbe vorbereiten.
    for (int i = 0; i < LED_COUNT; i++)
        pixels.setPixelColor(i, pixels.Color(r, g, b));

    // LED Ring anzeigen.
    pixels.show();
}

// Anzeige ändern.
void setDisplay(ledDisplayMode mode);
void setDisplay(ledDisplayMode mode) {
    // Keine Änderung.
    if (mode == ledMode)
        return;

    // Einige Anzeigen blinken.
    ledStart = millis();

    // Änderung merken und umsetzen.
    switch (ledMode = mode)
    {
    case LED_OFF:
        singleColor(0, 4, 0);
        break;
    case LED_START_ON:
        singleColor(0, 128, 0);
        break;
    case LED_START_OFF:
        singleColor(0, 16, 0);
        break;
    }
}

// Bearbeitet die Startwarnung.
void blinkStart() {
    int now = millis();

    switch (ledMode) {
    case LED_START_ON:
        if (now >= (ledStart + START_ON))
            setDisplay(LED_START_OFF);
        break;
    case LED_START_OFF:
        if (now >= (ledStart + START_OFF))
            setDisplay(LED_START_ON);
        break;
    }
}

/*
    Steuerung.
*/

void setup() {
    // LED Ring initialisieren.
    pixels.begin();
}

void loop() {
    if (ledStart == LED_INIT)
        setDisplay(LED_START_ON);

    // RFID Warnung hat Vorrang.
    if (millis() <= badAuth) {
        // Wir setzen einfach immer die Farbe - eigentlich könnten wir uns das auch merken aber für die paar Sekunden.
        singleColor(255, 255, 0);

        return;
    }

    // Wechselnde Anzeigen darstellen.
    blinkStart();
}

