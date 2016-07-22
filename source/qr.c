#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <quirc.h>
#include <time.h>

/* These files are borrowed from ksanislo/QRWebLoader */
#include "common.h"
#include "graphics.h"
#include "camera.h"

//u32 __stacksize__ = 0x40000;
u16 *camBuf;
struct quirc *qr;

void delay(double dly) {
	const time_t start = time(NULL);
	time_t current;
	do {
		time(&current);
	} while(difftime(current, start) < dly);
}

inline void clearScreen(void) {
	u8 *frame = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	memset(frame, 0, 400 * 240 * 3);
	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
	frame = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	memset(frame, 0, 400 * 240 * 3);
	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
	}

int initialiseQRScanner() {
	camInit();
	
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSet3D(false);
	
	configCamera();
	
	camBuf = (u16*)malloc(WIDTH * HEIGHT * 2);
	if(!camBuf) {
		printf("Failed to allocate memory.");
		return 1;
	}
	
	qr = quirc_new();
	if(!qr) {
		printf("Failed to allocate memory for quirc.");
		return 1;
	}
	
	if(quirc_resize(qr, WIDTH, HEIGHT) < 0) {
		printf("Failed to allocate video memory.");
		return 1;
	}
	
	return 0;
}

int scanQRCode(uint8_t *outBuffer) {
	while (aptMainLoop())
	{
		hidScanInput();

		unsigned long kDown = hidKeysDown();
		
		if (kDown & KEY_B) {
			printf("Cancelled QR scanning.\n");
			clearScreen();
			gfxFlushBuffers();
			gspWaitForVBlank();
			gfxSwapBuffers();
			return 0;
		}

		takePicture(camBuf);
		writePictureToFramebufferRGB565(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), camBuf, 0, 0, WIDTH, HEIGHT);
		
		int w = WIDTH, h = HEIGHT;
		u8 *image = (u8*)quirc_begin(qr, &w, &h);
		writePictureToIntensityMap(image, camBuf, WIDTH, HEIGHT);
		quirc_end(qr);
		
		int num_codes = quirc_count(qr);
		
		for(int i = 0; i < num_codes; i++) {
			struct quirc_code code;
			struct quirc_data data;
			quirc_decode_error_t err;
			
			quirc_extract(qr, i, &code);
			
			writePictureToFramebufferRGB565(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), camBuf, 0, 0, WIDTH, HEIGHT);
			for (int j = 0; j < 4; j++) {
				struct quirc_point *a = &code.corners[j];
				struct quirc_point *b = &code.corners[(j + 1) % 4];
				bhm_line(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), a->x, a->y, b->x, b->y, 0x0077FF77);
			}
			
			err = quirc_decode(&code, &data);
			if(!err) {
				CAMU_PlayShutterSound(SHUTTER_SOUND_TYPE_NORMAL);
				printf("Data found. Length: %d; payload: %s\n", data.payload_len, data.payload);
				outBuffer = (uint8_t*)malloc(data.payload_len);
				if(NULL == outBuffer) {
					printf("Could not allocate memory for QR payload.\n");
					return -1;
				}
				printf("Pointer allocated: %p\n", outBuffer);
				*outBuffer = data.payload;
				// outBuffer = strdup(data.payload);
				delay(3);
				
				clearScreen();
				gfxFlushBuffers();
				gspWaitForVBlank();
				gfxSwapBuffers();
				return 0;
			}
		}
	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
	}

	return 0;
}
