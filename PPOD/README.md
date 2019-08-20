# PPOD_FlightComputer
Flight computer for the PPOD

Status indicator LEDs:
Blue: GPS, flashes every 10 seconds if fix, every second if not
Red: SMART, flashes every 10 seconds if not released, every second after release
Yellow: SD card, flashes every log cycle if logging, stays on if not logging

XBee commands:
Time: Sends minutes remaining until cut time
+[minutes]: Adds given minutes to cut time
-[minutes]: Subtracts given minutes from cut time
Alt: Sends altitude remaining until cut alt
A+[meters]: Adds given meters to cut alt
A-[meters]: Subtracts given meters from cut alt
Cut: Releases PPOD via command, or indicates it has already been released
Data: Sends data string from GPS and sensors
Marco: Sends "Polo" for ping