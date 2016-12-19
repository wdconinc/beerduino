## BeerDuino: Arduino-controlled (Particle photon) home-brewing automation

* Temperature using Vishay 10k PTC
* Relay control of brew-belt
* Bubble counting using IR LED and photo-diode
* Publishing of information to ThingSpeak using the Particle API and webhook

## Compilation

Compilation using:
`particle compile photon .`

Installation using (assuming beerduino is the assigned name of the device to be flashed):
`particle flash beerduino .`
