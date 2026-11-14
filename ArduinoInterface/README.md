# Servicetastatur ArduinoInterface

Dieser Sketch kann mit einer Profitech 3000 kommunizieren und stellt in der seriellen Konsole eine virtuelle Tastatur dar.
Alternativ kann ein 1602 LCD + Keypad Shield auf einem Arduino Uno als Display verwendet werden.

## Arduino IDE

`ArduinoInterface.ino` öffnen, unter **Werkzeuge > Board > Arduino AVR Boards > Arduino Uno**
den Uno und anschließend den angeschlossenen Port auswählen. Das LCD1602-Keypad-Shield
ist standardmäßig aktiviert; Änderungen am Sketch sind dafür nicht nötig.
Die Board- und Portauswahl erfolgt in der IDE.

Für das RM68130-TFT/Touch-Shield in `Hardware.h` den Standardwert von
`SERVICE_KEYBOARD_TFT` auf `1` setzen. PlatformIO wählt den Displaytyp über die
jeweilige Umgebung; die Standardumgebung ist `uno_lcd1602`.

Verkabelung:
| Header | Signal |
|---|---|
| A1 | OutST |
| A2 | Enable |
| A3 | ClockIn |
| A4 | InST |
