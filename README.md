# thermostat
An Arduino thermostat with LCD, heat, cool, keyboard and non-volatile temperature store.

Uses a 100K EPCOS thermistor and 4K7 resistor as a voltage divide on the analogue pin just like a RAMPS 1.4 board.

If temperature is below "Min" value, it turns the heater on. If the temperature is above the "Max" value it turns the cooler on. There is 0.5 degree C of hysteresis to avoid chatter.

A standard LCD/keyboard module can be fitted to allow in-flight control, and the settings can be saved to EEPROM with the "SELECT" button.

If the thermistor shorts or goes open-circuit, the heat & cool outputs are turned off and the program halts.
