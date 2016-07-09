# tinytot
Homebrew 2FA OTP generator for the Nintendo 3DS.

Uses liboath from [oath-toolkit](http://www.nongnu.org/oath-toolkit/) by Simon Joseffson, et al., licensed under the LGPL.

Instructions for building liboath: (in the oath-toolkit/liboath directory):
	
	export PORTLIBS="${DEVKITPRO}/portlibs/armv6k"
	export PATH="${DEVKITARM}/bin:${PATH}"
	export CFLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -O3 -mword-relocations -fomit-frame-pointer -ffast-math"
	./configure --prefix=${DEVKITPRO}/portlibs/armv6k --host=arm-none-eabi --disable-shared --enable-static 
