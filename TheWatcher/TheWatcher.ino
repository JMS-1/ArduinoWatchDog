#include <SPI.h>
#include <MFRC522.h>
#include <IRremote.h>
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

// Konfiguration des Bewegungsmelders.
#define MOVE_PIN        7

// Konfiguration der Helligkeitssteuerung.
#define POTI_PIN        A0

// Konfiguration der Fernbedienung.
#define REMOTE_PIN      6

#define REMOTE_1        0xFF30CF
#define REMOTE_2        0xFF18E7
#define REMOTE_3        0xFF7A85
#define REMOTE_4        0xFF10EF
#define REMOTE_5        0xFF38C7
#define REMOTE_6        0xFF5AA5

IRrecv irrecv(REMOTE_PIN);

/*
    LED Anzeige.
*/

typedef enum { LED_OFF, LED_START_ON, LED_START_OFF, LED_HOT, LED_WARN, LED_DETECTED } ledDisplayMode;

ledDisplayMode ledMode = LED_OFF;

#define START_ON    500         // Startwarnung: Zeit für LEDs an
#define START_OFF   200         // Startwarnung: Zeit für LEDs aus
#define START_DELAY 5000        // Dauer der Startwarnung
#define AUTH_ON     2500        // Dauer des Feedbacks für eine unbekannte RFID
#define WARN_ON     10000       // Zeitraum zum Abschalten des Alarms (ok, hier nicht wirklich spannend, danach könnte es aber losheulen!)
#define LATENCY     5000        // Solange müssen wir nach dem Abschalten warten, bis wir wieder scharf schalten können

long ledHot = -1;               // Ende der Startwarnung
long badAuth = -1;              // Ende des Feedbacks zu einer falschen RFID              
auto lastPoti = -1;             // Letzte Einstellung der Helligkeitsregelung
long ledStart = -1;             // Letzter Umschaltzeitpunkt der LED Anzeige
auto lastDynamic = -1;          // Referenz für veränderliche Anzeigen (Rotation, Glühen)
auto hasBadAuth = false;        // Gesetzt, wenn eine falsche RFID erkannt wurde

// Einfarbige Anzeige.
void singleColor(uint8_t r, uint8_t g, uint8_t b) {
    auto color = leds.Color(r, g, b);

    // Ring in der gewünschten Farbe vorbereiten.
    for (auto i = 0; i < LED_COUNT; i++)
        leds.setPixelColor(i, color);

    // LED Ring anzeigen.
    leds.show();
}

// Anzeige ändern.
void setDisplay(ledDisplayMode mode, bool force = false);
void setDisplay(ledDisplayMode mode, bool force = false) {
    // Keine Änderung.
    if ((mode == ledMode) && !force)
        return;

    // Einige Anzeigen verändern sich mit der Zeit, wir mekren uns daher den Umschaltzeitpunkt als Referenz.
    ledStart = millis();

    // Änderung merken und umsetzen.
    switch (ledMode = mode)
    {
    case LED_OFF:
        lastPoti = -1;
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
    case LED_WARN:
    case LED_DETECTED:
        lastDynamic = -1;
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

// Zeigt einen Einbruch an (nein, heult nicht los).
void rotate() {
    // Das sind wir nicht.
    if (ledMode != LED_DETECTED)
        return;

    // Relative Position ermitteln.
    auto now = millis();
    auto done = now - ledStart;
    long center = (done / 75) % LED_COUNT;

    // Ein bißchen optimieren.
    if (center == lastDynamic)
        return;

    lastDynamic = center;

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

// Zeigt einen Einruch an, erlaubt aber noch das Abschalten des Alarms.
void glow() {
    // Das sind wir nicht
    if (ledMode != LED_WARN)
        return;

    // Relative Position ermitteln.
    auto now = millis();
    auto done = now - ledStart;

    // Anzeigen - wir machen uns hier nicht die Mühe einer Optimierung.
    if (done >= WARN_ON)
        setDisplay(LED_DETECTED);
    else
        singleColor((done / 6) % 128, 0, 0);
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

// Prüft auf eine RFID Anmeldung.
void processTag() {
    // Neu einlesen.
    auto tagInfo = readTag();

    if (tagInfo == lastTag)
        return;

    // Ergebnis auswerten.
    auto now = millis();

    switch (lastTag = tagInfo)
    {
    case RFID_GOODTAG:
        switch (ledMode)
        {
        case LED_OFF:
            // Das war zu schnell.   
            if ((now - ledStart) < LATENCY)
                return;

            // Startwarnung anwerfen.
            ledHot = now + START_DELAY;
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
        badAuth = now + AUTH_ON;
        break;
    }
}

/*
    Helligkeit im Schlafmodus.
*/

// Viel können wir mit der Fernbedienung nicht machen, aber die Idee kommt sicher rüber.
auto sleep_red = 0;
auto sleep_blue = 0;
auto sleep_green = 1;

// Anzeige im Schlafzustand - sowohl die Helligkeitsregelung als auch die Fernbedienung haben hier einen Einfluss.
void sleep() {
    // Nicht unsere Aufgabe.
    if (ledMode != LED_OFF)
        return;

    // Auslesen.
    auto intens = 4 * (analogRead(POTI_PIN) / 16);

    // Fernbedienung auslesen.
    decode_results results;

    // Keine Änderung.
    if (irrecv.decode(&results))
    {
        // Fernbedienung auswerten.
        switch (results.value)
        {
        case REMOTE_1:
            sleep_red = 1;
            sleep_blue = 0;
            sleep_green = 0;
            break;
        case REMOTE_2:
            sleep_red = 0;
            sleep_blue = 1;
            sleep_green = 0;
            break;
        case REMOTE_3:
            sleep_red = 0;
            sleep_blue = 0;
            sleep_green = 1;
            break;
        case REMOTE_4:
            sleep_red = 0;
            sleep_blue = 1;
            sleep_green = 1;
            break;
        case REMOTE_5:
            sleep_red = 1;
            sleep_blue = 0;
            sleep_green = 1;
            break;
        case REMOTE_6:
            sleep_red = 1;
            sleep_blue = 1;
            sleep_green = 0;
            break;
        }

        // Weiter machen.
        irrecv.resume();
    }
    else if (intens == lastPoti)
        return;

    // Merken.
    lastPoti = intens;

    // Anzeigen.
    singleColor(sleep_red * lastPoti, sleep_green * lastPoti, sleep_blue * lastPoti);
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

    // Bewegungsmelder initialisieren.
    pinMode(MOVE_PIN, INPUT);

    // Fernbedienung initialisieren.
    irrecv.enableIRIn();

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

    // Wir sind scharf.
    if (ledMode == LED_HOT)
        if (digitalRead(MOVE_PIN) == HIGH)
            setDisplay(LED_WARN);

    // Wechselnde Anzeigen darstellen.
    sleep();
    blinkStart();
    glow();
    rotate();
}

