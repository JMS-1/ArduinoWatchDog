#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

// Konfiguration des LED Rings.
#define LED_CONTROL     8
#define LED_COUNT       16

Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, LED_CONTROL, NEO_GRB + NEO_KHZ800);

// Konfiguration des RFID Empfängers.
#define RFID_SDA        10
#define RFID_RST        9

MFRC522 mfrc522(RFID_SDA, RFID_RST);

/*
    LED Anzeige.
*/

typedef enum { LED_OFF, LED_START_ON, LED_START_OFF, LED_HOT, LED_DETECTED } ledDisplayMode;

ledDisplayMode ledMode = LED_OFF;

#define START_ON    500
#define START_OFF   200
#define START_DELAY 5000
#define AUTH_ON     2500

long ledHot = -1;
long badAuth = -1;
long ledStart = -1;
auto lastCenter = -1;
auto hasBadAuth = false;

// Einfarbige Anzeige.
void singleColor(uint8_t r, uint8_t g, uint8_t b) {
    // Ring in der gewünschten Farbe vorbereiten.
    for (auto i = 0; i < LED_COUNT; i++)
        leds.setPixelColor(i, leds.Color(r, g, b));

    // LED Ring anzeigen.
    leds.show();
}

// Anzeige ändern.
void setDisplay(ledDisplayMode mode, bool force = false);
void setDisplay(ledDisplayMode mode, bool force = false) {
    // Keine Änderung.
    if ((mode == ledMode) && !force)
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
    case LED_HOT:
        singleColor(16, 0, 0);
        break;
    case LED_DETECTED:
        lastCenter = -1;
        break;
    }
}

// Bearbeitet die Startwarnung.
void blinkStart() {
    auto now = millis();

    // Startwarnung geben und nach Ablauf scharf schalten.
    switch (ledMode)
    {
    case LED_START_ON:
        if (now >= ledHot)
            setDisplay(LED_HOT);
        else if (now >= (ledStart + START_ON))
            setDisplay(LED_START_OFF);
        break;
    case LED_START_OFF:
        if (now >= ledHot)
            setDisplay(LED_HOT);
        else if (now >= (ledStart + START_OFF))
            setDisplay(LED_START_ON);
        break;
    }
}

// Bearbeitet die Anzeige der Meldung.

void rotate() {
    // Das sind wir nicht
    if (ledMode != LED_DETECTED)
        return;

    // Relative Position ermitteln.
    auto now = millis();
    auto done = now - ledStart;
    long center = (done / 75) % LED_COUNT;

    // Ein bißchen optimieren.
    if (center == lastCenter)
        return;

    lastCenter = center;

    // Anzeige setzen.
    for (long i = 0; i < LED_COUNT; i++)
    {
        // Distanz vom Zentrum.
        auto dist = abs(i - center);

        if (dist > LED_COUNT / 2)
            dist = LED_COUNT - dist;

        // Anzeige erstellen.
        switch (dist)
        {
        case 0:
            leds.setPixelColor(i, 128, 0, 0);
            break;
        case 1:
            leds.setPixelColor(i, 32, 0, 0);
            break;
        case 2:
            leds.setPixelColor(i, 8, 0, 0);
            break;
        case 3:
            leds.setPixelColor(i, 4, 0, 0);
            break;
        case 4:
            leds.setPixelColor(i, 2, 0, 0);
            break;
        default:
            leds.setPixelColor(i, 1, 0, 0);
            break;
        }
    }

    // Anzeigen.
    leds.show();
}

/*
    RFID Empfänger.
*/

#define READ_TAG_DELAY  500

typedef enum { RFID_BADTAG = -1, RFID_NOTAG, RFID_GOODTAG } rfidTagResult;

auto lastTag = RFID_NOTAG;
auto firstReadTag = true;
long nextReadTag = -1;

// Prüft of ein RFID Tag in der Nähe ist.
rfidTagResult readTag();
rfidTagResult readTag() {
    // Auslesen initialisieren.
    if (firstReadTag) {
        firstReadTag = false;
        nextReadTag = millis();
    }

    // Glitches vermeiden.
    auto now = millis();

    if (now < nextReadTag)
        return RFID_NOTAG;

    // Auf Karte prüfen.
    if (!mfrc522.PICC_IsNewCardPresent())
        return RFID_NOTAG;

    if (!mfrc522.PICC_ReadCardSerial())
        return RFID_NOTAG;

    // Nächste Prüfung erst wieder nach einer gewissen Zeit erlauben.
    nextReadTag = now + READ_TAG_DELAY;

    // RFID Kennung (UID) prüfen.
    if (mfrc522.uid.size != 4)
        return RFID_BADTAG;
    if (mfrc522.uid.uidByte[0] != 84)
        return RFID_BADTAG;
    if (mfrc522.uid.uidByte[1] != 189)
        return RFID_BADTAG;
    if (mfrc522.uid.uidByte[2] != 158)
        return RFID_BADTAG;
    if (mfrc522.uid.uidByte[3] != 187)
        return RFID_BADTAG;

    return RFID_GOODTAG;
}

void processTag() {
    // Neu einlesen.
    auto tagInfo = readTag();

    if (tagInfo == lastTag)
        return;

    // Ergebnis auswerten.
    switch (lastTag = tagInfo)
    {
    case RFID_GOODTAG:
        switch (ledMode)
        {
        case LED_OFF:
            // Startwarnung anwerfen.
            ledHot = millis() + START_DELAY;
            setDisplay(LED_START_ON);
            break;
        case LED_START_ON:
        case LED_START_OFF:
            break;
        default:
            // In den Ruhezustand wechseln.
            setDisplay(LED_OFF);
            break;
        }
        break;
    case RFID_BADTAG:
        // Falsches Tag melden.
        hasBadAuth = true;
        badAuth = millis() + AUTH_ON;
        break;
    }
}

/*
    Steuerung.
*/

void setup() {
    // LED Ring initialisieren.
    leds.begin();

    // SPI initialisieren.
    SPI.begin();

    // RFID Empfänger initialisieren.
    mfrc522.PCD_Init();

    // Initiale Anzeige setzen.
    setDisplay(LED_OFF, true);
}

void loop() {
    // RFID einlesen und entsprechend reagieren.
    processTag();

    // RFID Warnung hat Vorrang.
    if (hasBadAuth) {
        if (millis() > badAuth) {
            hasBadAuth = false;

            // Aktuellen Zustand wieder herstellen.
            setDisplay(ledMode, true);
        }
        else {
            // Wir setzen einfach immer die Farbe - eigentlich könnten wir uns das auch merken aber für die paar Sekunden...
            singleColor(128, 128, 0);

            return;
        }
    }

    // Wechselnde Anzeigen darstellen.
    blinkStart();
    rotate();
}

