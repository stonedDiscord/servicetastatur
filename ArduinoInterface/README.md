# Servicetastatur ArduinoInterface

Dieser Sketch kann mit einer Profitech 3000 kommunizieren und stellt in der seriellen Konsole eine virtuelle Tastatur dar.
Alternativ kann ein 1602 LCD + Keypad Shield auf einem Arduino Uno als Display verwendet werden.

# Verkabelung:
Am Arduino/Shield
| Header | Signal |
|---|---|
| A1 | OutST |
| A2 | EnST |
| A3 | nTaktST |
| A4 | DataST |

Am Automaten
(von oben nach unten)
| Pin | Signal |
|---|---|
| 6 | GND |
| 5 | OutST |
| 4 | EnST |
| 3 | nTaktST |
| 2 | DataST |
| 1 | 5V |

![Kabeldiagramm](https://raw.githubusercontent.com/stonedDiscord/servicetastatur/schema_bb.png)

Die Stromversorgung nur anschließen falls der Ardunio nicht per USB angeschlossen ist.
Und dann bitte an VIN und nicht direkt 5V.
GND befindet sich zwischen 5V und VIN am Shield falls es nicht beschriftet ist.

## Arduino IDE
Für das RM68130-TFT/Touch-Shield in `Hardware.h` den Standardwert von
`SERVICE_KEYBOARD_TFT` auf `1` setzen.

Im Bibliotheksverwalter installieren:

- LCD1602: **LiquidCrystal**
- TFT: **GFX Library for Arduino** von Moon On Our Nation
