# PPOD_FlightComputer
Flight computer for the PPOD, configured for Teensy 3.5/3.6.

**Layout**

This PPOD can hold 4U worth of CubeSats. It runs on a Teensy 3.6, with a SMART release mechanism, a Ublox GPS, XBee radio communication via any Comms unit, and additional sensors: a Dallas temperature sensor and a Honeywell pressure sensor.

The board on which the controls are mounted can be detached by opening the front access panel and pushing up on it from underneath (be sure to separate the SMART, LED, and battery/switch wires first). To reattach the board, line up the arrow in one corner of the board with the corresponding arrow on the corner of the shelf inside the PPOD.

**Release checker setup**

The release checker must be pinned in place each time the SMART pin is reset. Take a very small breadboard wire and bend it around the large loop of cinching string (NOT the loop that goes around the SMART pin). Connect the ends of the wire to digital pins 9 and 10, so the loop of string is held in place. This wire will pull out when the SMART unit releases, confirming the release with the computer.

**Status indicator LEDs:**

Blue: GPS, flashes every 10 seconds if fix, every second if not

Red: SMART, flashes every 10 seconds if not released, every second after release

Yellow: SD card, flashes every log cycle if logging, stays on if not logging

**XBee commands:**

Time: Sends minutes remaining until cut time

+[minutes]: Adds given minutes to cut time

-[minutes]: Subtracts given minutes from cut time

Alt: Sends altitude remaining until cut alt

A+[meters]: Adds given meters to cut alt

A-[meters]: Subtracts given meters from cut alt

Cut: Releases PPOD via command, or indicates it has already been released

Data: Sends data string from GPS and sensors

Marco: Sends "Polo" for ping