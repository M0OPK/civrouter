# CIVRouter
CIV Router is a program to allow a single Icom radio to be used my multiple applications by passing providing a NAT like interface so each application is convinced they are talking to the radio. But are in fact talking to this application which passes on their requests to the radio.

## Requirements
A loopback interface. This is untested on windows but should in theory be fine on a properly implemented serial port loop back. On linux the author uses the tty0tty kernel module. This creates virtual device pairs. /dev/tnt0 <--> /dev/tnt1 for example.

## Use Examples
A single Icom radio, Logging software, WSJT and an SDR running a panadapter. They would all like to talk to the radio. But only one can. In this case, civrouter would be setup to communicate to the radio (perhaps on COM1 or /dev/ttyUSB0) and then connect to ports COM11 or /dev/tnt1, COM13 or /dev/tnt3, COM15 or /dev/tnt5. Then the logging software could connect to COM10 or /dev/tnt0, WSJT on COM12 or /dev/tnt2 and the SDR software on COM14 or /dev/tnt4.

## Features
The configuration file allows for specifying whether a port receives transceive[1] messages. This is because some programs need these messages, and some get a little upset when they are sent. TODO: Prevent on a port by port basis, programs from enabling/disabling transceive mode.

It's possible to specify fixed translation addresses. Software for Icom radios usually use 0xE0 as the controller address. CIVRouter uses "fake" controller IDs 0xE1+ to send to the radio. Most radios accept these and will answer back to them. In this way it's easy to interleave commands and still know who to route responses back to. On the port connected to the software, the ID is translated back to 0xE0 so the software is none the wiser that their ID was changed. Normally these IDs are created automatically. They can be specified manually.

Allow forwarding changes to Frequency or mode from other programs to Transceive commands to those ports accepting Transceive commands. [2]

*[1] When Transceive mode is enabled on an Icom radio, frequency and mode changes are sent in real time. In my case, my logging software and my CIV client module for SDR++ make good use of this.

*[2] When a program sends a command to change the frequency or mode, this is forwarded as normal to the radio, but in addition a transceive CIV frame is also forwarded to other controller interfaces that accept info messages, to ensure they can see the action event and react to it as if the user tuned the radio directly.

## Coming soon
The move from Rust to C++ is in order to provide a framework for the project to also work on ESP32 boards. A hardware project is planned in tandem where it would be possible to build a simple board connected to an ESP32 S3 board, to connect to the CIV port on almost all ICOM radios, and have the USB port on the ESP32 simulate multiple serial ports such that multiple applications can connect to the single radio.

