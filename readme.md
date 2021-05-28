# Estlcam Handrad

Estlcam Handrad Umsetzung mit Raspberry Pi Pico. Es wurde das von Estlcam veröffentlichte Protokoll umgesetzt. Das Handrad läuft mit der offiziellen Hardware von Estlcam (LPT Adapter und Klemmenadapter) .

Da der Pi Pico nur 3 nutzbare ADC Kanäle hat, wurden *Feed* und *Spindle* über Drehencoder realisiert.

Buttons für *OK*, *SPINDLE* und *PROG* wurden umgesetzt.

Die Knöpfe *YELLOW*, *RED* und *FEED* sind noch unbelegt.


## Protokoll:

https://www.estlcam.de/DIN_Detail.php


## Pinout:

![Pinout](doc/pi_pico_pinout.png)


## Bilder


![Pinout](doc/images/handrad001.png)

![Pinout](doc/images/handrad002.png)

## Stückliste:

[Siehe hier](doc/bom.md)


## Download

[Aktuelle Version](https://github.com/hdo/pico-estlcam-handrad/releases/tag/v0.0.1)

## Gehäuse

https://www.thingiverse.com/thing:4871639

## Links:

Estlcam LPT / Parallelport Adapter:

https://www.estlcam.de/lpt_adapter.php

Estlcam Klemmenadapter:

https://www.estlcam.de/Klemmenadapter.php
 

Vorlage für Gehäuse:

https://blog.altholtmann.com/diy-cnc-handrad-mit-joystick/
