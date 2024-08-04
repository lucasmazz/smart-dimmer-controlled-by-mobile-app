# Smart Dimmer Controlled by Mobile App

This project involves the development of a smart dimmer using an ESP32 microcontroller, which can be controlled via a mobile app. The smart dimmer allows users to adjust the brightness of a connected light remotely through WiFi communication.

![Prototype Working on Breadboard](http://workabotic.com/public/images/smart-dimmer-controlled-by-mobile-app/complete%20prototype_working_on_the_breadboard.gif?raw=true)

### Requirements
- [ESP IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)
- [Android Studio](https://developer.android.com/studio/)
- [Volley Library](https://github.com/google/volley)

### Materials

The list of components used in this project are listed below:

- 1x ESP32 microcontroller
- 1x TRIAC
- 1x 4N25 optocoupler
- 1x MOC3021 optocoupler
- 1x transformer
- 4x diodes
- 2x 100 Ω resistors
- 1x 330 Ω resistor
- 1x 10K Ω resistor

### Overview

Key components and their functions include:

- A transformer reduces power voltage to 12 V AC, with a bridge rectifier converting it to 6 V DC.
- The 4N25 optocoupler provides zero-crossing detection and electrical isolation between the microcontroller and low voltage DC side.
- A 10k pull-up resistor ensures stable logic levels, while other resistors limit current and protect components.
- The MOC3021 optocoupler interfaces with the TRIAC, enabling the ESP32 to trigger the high voltage TRIAC safely.

In summary, the ESP32 receives zero-crossing signals from the 4N25 optocoupler, using this reference to calculate the trigger timing for the TRIAC via the MOC3021, thereby controlling dimming operations. The complete circuit schematic is shown below.

![Complete Circuit Schematic](http://workabotic.com/public/images/smart-dimmer-controlled-by-mobile-app/complete_circuit_schematic.webp)


### Documentation

For detailed documentation, including the schematic, firmware code, and mobile app source, refer to the project website: [Smart Dimmer Controlled by Mobile App](http://workabotic.com/2024/smart-dimmer-controlled-by-mobile-app/).

### Contributing

Contributions are welcome! Please fork the repository and submit pull requests for any improvements or bug fixes.