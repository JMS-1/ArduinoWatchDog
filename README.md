# Arduino meets 007
Wie üblich brauche ich mal wieder was zum Anfassen beim Lernen neuer Dinge. Darum auch dieses kleine Projekt für einen Arduino Uno R3.

Für die erste Ausbaustufe geht es erst einmal nur um die Software für so eine Art Alarmanlage - wer erinnert sich nicht an James Bond,
der immer ein Haar an der geschlossenen Türe angebracht hat um zu sehen, ob diese in seiner Abwesenheit geöffnet hat? Naja, ganz so
doll wird das nicht aber ich möchte folgende Idee umsetzen: mit einem RFID Tag kann die Anlage scharf geschaltet werden. In diesem
Zustand meldet ein Bewegungsmelder alle Aktivitäten. Die Visualierung (aus, scharf, Bewegung erkannt) erfolgt über einen Adafruit
NeoPixel Ring. Im ausgeschalteten Zustand dient die Anlage als Nachlicht - ausschalten geht wieder über ein RFID Tag. Bei einem 
falschen Tag wird kurz eine Warnung angezeigt - zum Beispiel wird der Ring ganz gelb. Ok, die "richtige" Kennung wird fest in den
Code kommen, aber darauf kommt es für das Beispiel nicht an.

In der zweiten Ausbaustufe kann man mit einem Poti dann die Helligkeit des Nachlichts regeln. Zulätzlich erlaubt eine IR Fernbedienung
verschiedene Varianten für die Anzeige des Nachlichts zu wählen: feste Farbe, Regenbogen, vielleicht auch Helligkeitsschwankungen.

Wenn das wirklich alles so funktioniert, dann soll das Ganze auch noch in ein kleines Kästchen eingebaut werden, damit man es
wirklich nutzen kann. Muss aber nicht: ist ja zum Lernen!
