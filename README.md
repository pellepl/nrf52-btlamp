# nrf52-btlamp
This is yet another smart lamp using a WS2812B rgb led ring.

The lamp is controlled using the wonderful little NRF52832 from Nordic Semiconductors, a Cortex M4 with BLE capabilities.

## Building and flashing
First time off, we need Nordics SDK and their softdevice binary - the BLE stack. The makefile will download and unpack this for you:

`# make setup`

Ignore the dependencies error, this is because the dependencies cannot be fulfilled without having the sdk, which we're about to download. I apparantly need help from a gnumake ninja here...

Having the sdk and the softdevice, we need to flash the softdevice once. Supposing you're connected with openocd, this is done using:

`# make install-softdev`

Also only needed once, unless the flash is wrecked.

To build the thing, issue 

`# make`

and finally to install the app, do

`# make install`

For flashing this thing I used a pirated ST-LINK V2 (yes, yes, I am a horrible person - the expensive one is at work) and [openocd4all](https://github.com/fredrikhederstierna/openocd4all).

Apart from the official SDK from Nordic, I stole some code from these repositories too: [embedded crap](https://github.com/pellepl/generic_embedded) and [bitmanio](https://github.com/pellepl/bitmanio). The author is a nice fella and won't mind.

## Hardware

I bought the PTR5628 board from aliexpress. It is tiny, smaller than the nail of my little finger. Soooo cute! Soldered wires to Vcc, GND, SWDIO, SWDCLK, reset, UART and an extra pin for SPI controlling the led ring. Behold my soldering skills!

![prototyping](https://raw.githubusercontent.com/pellepl/nrf52-btlamp/master/doc/prototyping.jpg)

After finished tinkering, I soldered together a more clean, minimalistic designed, rock-solid, industrial version of the ratnest above.

![front](https://raw.githubusercontent.com/pellepl/nrf52-btlamp/master/doc/front.jpg)

![back](https://raw.githubusercontent.com/pellepl/nrf52-btlamp/master/doc/back.jpg)

Do I need to mention my soldering skills again? Guessed not...! I might add that I hotglued the crap together after taking the pictures.

The connectors on the backside are the debug port and the uart. The pins sticking out are Vcc and GND. The front contains a MIC5239 fixed 3.3V LDO. The lamp is powered by a 5V 1A phone charger, so I needed to convert those 5V to 3.3V for the nrf52.

When all was finished and installed in the actual lamp, it turned out flickering and behaving badly. After some head scratching it turned out the 5V charger I used was crap, so after switching it to a proper one all became dandy.

I control the lamp using [this android app](https://github.com/NordicSemiconductor/Android-nRF-UART). Someday I'll do a proper app I gather.




