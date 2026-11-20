# Servicetastatur ArduinoInterface

Dieser Sketch kann mit einer Profitech 3000 kommunizieren und stellt in der seriellen Konsole eine virtuelle Tastatur dar.
Alternativ kann ein 1602 LCD + Keypad Shield als Display verwendet werden.

### Uno/Nano: Verdrahtung

Am Automaten
(von oben nach unten)
| Pin | Signal | Arduino |
|---|---|---|
| 6 | GND | GND |
| 5 | OutST | D12 |
| 4 | EnST | D2 |
| 3 | nTaktST | D13 |
| 2 | DataST | D11 |
| 1 | 5V | VIN |

D10 muss am Displayshield abgetrennt werden und auf dem Arduino low sein.

Die Stromversorgung nur anschließen falls der Ardunio nicht per USB angeschlossen ist.
Und dann bitte an VIN und nicht direkt 5V.
GND befindet sich zwischen 5V und VIN am Shield falls es nicht beschriftet ist.

## Arduino IDE
Im Bibliotheksverwalter installieren:

- LCD1602: **LiquidCrystal**
