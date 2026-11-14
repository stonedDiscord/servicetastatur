# Servicetastatur ArduinoInterface

Dieser Sketch kann mit einer Profitech 3000 kommunizieren und stellt in der seriellen Konsole eine virtuelle Tastatur dar.
Alternativ kann ein 1602 LCD + Keypad Shield auf einem Arduino Uno als Display verwendet werden.

# Verkabelung:
| Header | Signal |
|---|---|
| A1 | OutST |
| A2 | Enable |
| A3 | ClockIn |
| A4 | InST |

## Arduino IDE
Für das RM68130-TFT/Touch-Shield in `Hardware.h` den Standardwert von
`SERVICE_KEYBOARD_TFT` auf `1` setzen.
