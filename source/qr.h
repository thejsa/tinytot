#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <quirc.h>

/* These files are borrowed from ksanislo/QRWebLoader */
#include "common.h"
#include "graphics.h"
#include "camera.h"

u32 __stacksize__ = 0x40000;

int initialiseQRScanner();
int scanQRCode(char *outBuffer);
