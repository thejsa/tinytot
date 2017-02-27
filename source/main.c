#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <liboath/oath.h>

char const * TEST_ENCODED_SECRET = "HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ"; /* secret (base32) */

// TODO: How to account for daylight savings time?
// TODO: LibCtrU includes an internal function that appears to use a timezone:
//       int __libctru_gtod(struct _reent *ptr, struct timeval *tp, struct timezone *tz);
//       Is this exported anywhere?
//       If not, perhaps modify LibCtrU to include this functionality?

// See libctru\include\3ds\result.h for definitions
Result const R_TINYTOT_SUCCESS  = MAKERESULT(RL_SUCCESS,RS_SUCCESS,RM_APPLICATION,RD_SUCCESS);
Result const R_TINYTOT_OVERFLOW = MAKERESULT(RL_FATAL,RS_INVALIDARG,RM_APPLICATION,RD_TOO_LARGE);
Result const R_TINYTOT_OUTOFMEMORY = MAKERESULT(RL_FATAL,RS_OUTOFRESOURCE,RM_APPLICATION,RD_OUT_OF_MEMORY);
unsigned long const INVALID_DECODED_SECRET = 0;
unsigned long const REQUESTED_OTP_DIGITS = 6;

signed long const INVALID_CLOCK_OFFSET  = 0x1FFFF; // illegal value... see InitializeClockOffset()
signed long g_SystemClockUtcOffset = INVALID_CLOCK_OFFSET; // NULL for non-pointer isn't correct
bool IsValidTimeOffset(u32 timeOffset)
{
	// timeOffset valid values are +/- 12 hours (+/- 43200 seconds)
	u32 x = 43200;
	u32 y = 0 - x;
	return ((timeOffset <= x) || (timeOffset >= y));
}

Result InitializeClockOffset() {
	/* Compares the system clock with current UTC time from timeapi.com */
	/* ESSENTIAL for correct OTP generation, unless system clock is UTC */
	/* Returns difference, in seconds. */
	/* Add the result of this function to the 3DS time to get UTC. */
	
	if (IsValidTimeOffset(g_SystemClockUtcOffset)) return true;
	
	Result ret = 0;
	unsigned long statusCode = 0;
	unsigned long contentSize = 0;
	unsigned char *buffer = NULL;

	httpcContext context; // NOTE: Uninitialized memory
	httpcInit(0);
	
	char * url = "http://jsa.labs.projectkaeru.xyz/atoolyoucanputonthewall.php";
	/* put this line into a blank PHP file:

	<?php date_default_timezone_set("UTC"); echo time(); exit; ?>

	*/

	/* URL returning current time in UTC */
	
	if (R_SUCCESS(ret)) {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
	}

	if (R_SUCCESS(ret)) {
		ret = httpcBeginRequest(&context);
	}
	
	if (R_SUCCESS(ret)) {
		ret = httpcGetResponseStatusCode(&context, &statusCode);
		if (R_SUCCESS(ret) && statusCode != 200) {
			printf("WARNING: HTTP status code returned was %d\n", statusCode);
		}
	}
	if (R_SUCCESS(ret)) {
		ret = httpcGetDownloadSizeState(&context, NULL, &contentSize);
	}
	if (R_SUCCESS(ret)) {
		if(contentSize+1 < contentSize) {
			ret =  R_TINYTOT_OVERFLOW; // overflow -- do not allow
		}
	}
	if (R_SUCCESS(ret)) {
		buffer = (unsigned char*)malloc(contentSize+1);
		if(buffer == NULL) {
			ret = R_TINYTOT_OUTOFMEMORY;
		}
	}
	if (R_SUCCESS(ret)) {
		memset(buffer, 0, contentSize+1); // zero that last byte also
		ret = httpcDownloadData(&context, buffer, contentSize, NULL);
	}
	if (R_SUCCESS(ret)) {
		time_t utcTime = (time_t)atol(buffer);
		time_t systemTime = time(NULL);
		signed long timeDifference = systemTime - utcTime; /* time(NULL) + timeDifference = UTC */
		g_SystemClockUtcOffset = timeDifference;
	}
	if (NULL != buffer) {
		free(buffer);
	}
	return ret;
}

// TODO: Change to accept a pointer, return Result type, so can call Initialize() directly here?
unsigned long currentTimeUTC() {
	unsigned long systemTime = (unsigned long)time(NULL);
	return (unsigned long)(systemTime - g_SystemClockUtcOffset);
}
	
unsigned long generateTOTP(unsigned char const * secret, size_t const * secretLength) {
	if(*secretLength < 1) {
		printf("Secret is zero-length, cannot generate TOTP\n");
		return INVALID_DECODED_SECRET;
	}
	
	if(!R_SUCCESS(InitializeClockOffset)) {
		printf("Failed to initializ clock offset, cannot generate TOTP\n");
		return INVALID_DECODED_SECRET;
	}
	
	unsigned long timerightnow = currentTimeUTC();
	unsigned char otp[REQUESTED_OTP_DIGITS+1]; /* must allocate for trailing NULL */
	
	int ret = oath_totp_generate(secret, (size_t)*secretLength, timerightnow, OATH_TOTP_DEFAULT_TIME_STEP_SIZE, OATH_TOTP_DEFAULT_START_TIME, REQUESTED_OTP_DIGITS, (char*)&otp);
	if(ret != OATH_OK) {
		printf("Error generating TOTP: %s\n", oath_strerror(ret));
		return INVALID_DECODED_SECRET;
	}
	
	return atol(otp);
}

static SwkbdCallbackResult swkbdCallbackThing(void* user, const char** ppMessage, const char* text, size_t textlen)
{
	char *secret;
	signed short secretLength;

	ret = oath_base32_decode(text, strlen(text), &secret, &secretLength);

	if(ret != OATH_OK) {
		printf("Error decoding secret: %s\n", oath_strerror(ret));
		sprintf(*ppMessage, "Error decoding: %s", oath_strerror(ret));
		return SWKBD_CALLBACK_CONTINUE;
	}else{
		unsigned long otp = generateTOTP(secret, &secretLength);
		if(otp == 0) {
			*ppMessage = "OTP generation failed.";
			printf("\nOTP generation failed.\n");
			return SWKBD_CALLBACK_CONTINUE;
		}else{
			sprintf(*ppMessage, "OTP: %06lu", otp)
			printf("*** OTP: %06lu ***\n\n", otp);
			return SWKBD_CALLBACK_CONTINUE;
		}
	}

	return SWKBD_CALLBACK_OK;
}

int main() {
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);
	
	printf("%s %s by %s\n", APP_TITLE, APP_VERSION, APP_AUTHOR);
	printf("Build date: %s %s\n\n", __DATE__, __TIME__);
	//printf("Current time: %d\n\n", (int)time(NULL));

	printf("Calculating time difference from UTC... make sure you are connected to the Internet. ");
	
	Result InitClockOffsetResult = InitializeClockOffset();
	if (!R_SUCCESS(InitClockOffsetResult)) {
		printf("Error initializing time offset: %08x", InitClockOffsetResult);
		return 1;
	}
	printf("OK\n");
	
	ret = oath_init();
	if(ret != OATH_OK) {
		printf("Error initializing liboath: %s\n", oath_strerror(ret));
		return 1;
	}

	char encoded_secret[1024];
	FILE *secretFile;
	signed short secretTxtDecLength;
	if((secretFile = fopen("secret.txt", "r")) == NULL) {
		printf("warning: Secret.txt not found in application directory (SD root if installed as CIA)");
		/*while(aptMainLoop()) {
			gspWaitForVBlank();
			hidScanInput();
			
			unsigned long kDown = hidKeysDown();
			if(kDown & KEY_A) break;
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
		return 1;*/
	}else{
		printf("Opened secret.txt\n");
		fscanf(secretFile, "%[^\n]", encoded_secret);
		fclose(secretFile);
		if(strlen(encoded_secret) < 1){
			printf("warning: secret.txt exists but is empty.\n");
		}else{
			printf("Read secret.txt: %s\n", encoded_secret);
	
			ret = oath_base32_decode(&encoded_secret, strlen(&encoded_secret), NULL, &secretTxtDecLength);
			if(ret != OATH_OK) {
				printf("Error decoding secret.txt: %s\n", oath_strerror(ret));
				memset(&encoded_secret[0], 0, sizeof(encoded_secret)); // wipe the copy we have in memory, to avoid prefilling the swkbd with undecodable stuff
			}else{
				printf("Read secret.txt successfully.");
			}
		}
	}

	char inputSecret[1024];

	bool quit = false;
	bool ask = false;
	
	printf("Press A to begin, or start to exit.\n\n");
	
	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		unsigned long kDown = hidKeysDown();
		if (kDown & KEY_A) ask = true;

		if(ask && !quit) {
			static SwkbdState swkbd;
			static SwkbdStatusData swkbdStatus;
			SwkbdButton button = SWKBD_BUTTON_NONE;

			swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, 512);
			swkbdSetHintText(&swkbd, "Enter your TOTP secret.");
			swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Quit", false);
			swkbdSetButton(&swkbd, SWKBD_BUTTON_MIDDLE, "Load txt", true);
			swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Go", true);
			swkbdSetInitialText(&swkbd, inputSecret);
			swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
			swkbdSetFilterCallback(&swkbd, swkbdCallbackThing, NULL);

			swkbdInputText(&swkbd, inputSecret, sizeof(inputSecret));

			switch(button) {
				case SWKBD_BUTTON_LEFT: // quit
					quit = true;
					break;
				case SWKBD_BUTTON_MIDDLE: // read secret.txt
					strcpy(inputSecret, encoded_secret);
					break;
				case SWKBD_BUTTON_RIGHT: // go (this is handled in filter callback)
					break;
				default:
					break;
			}

			if(quit) break; // quit to HBL
		}
		
		if (kDown & KEY_START) {
			break; // break in order to return to hbmenu
		}

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	gfxExit();
	return 0;
}
