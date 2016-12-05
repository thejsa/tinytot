# tinytot
## Two-factor authentication app for the 3DS

Currently only supports the TOTP algorithm.
The secret to use should be stored in secret.txt in the same directory as the 3DSX, encoded in base 32.

This is not at all related to the OTP files required for A9LH; sorry if I got your hopes up.
This is used for generating One-Time Passwords for logging into websites, similar to Google Authenticator.

## License
TinyTot is licensed under the Apache License v2.0. (see LICENSE.txt)
Requires liboath from [oath-toolkit](http://www.nongnu.org/oath-toolkit/) by Simon Joseffson, et al., licensed under the LGPL.

## Compiling liboath
Instructions for downloading & building liboath as a portlib:
	
	wget http://download.savannah.gnu.org/releases/oath-toolkit/oath-toolkit-2.6.1.tar.gz
	tar zxvf oath-toolkit-*.tar.gz
	cd oath-toolkit-*/liboath
	export PORTLIBS="${DEVKITPRO}/portlibs/armv6k"
	export PATH="${DEVKITARM}/bin:${PATH}"
	export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -O3 -mword-relocations -fomit-frame-pointer -ffast-math"
	./configure --prefix=${DEVKITPRO}/portlibs/armv6k --host=arm-none-eabi --disable-shared --enable-static 
