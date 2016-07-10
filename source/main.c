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
u32 const R_TINYTOT_SUCCESS  = MAKERESULT(RL_SUCCESS,RS_SUCCESS,RM_APPLICATION,RD_SUCCESS);
u32 const R_TINYTOT_OVERFLOW = MAKERESULT(RL_FATAL,RS_INVALIDARG,RM_APPLICATION,RD_TOO_LARGE);
u32 const R_TINYTOT_OUTOFMEMORY = MAKERESULT(RL_FATAL,RS_OUTOFRESOURCE,RM_APPLICATION,RD_OUT_OF_MEMORY);



u32 INVALID_CLOCK_OFFSET  = 0x1FFFF; // illegal value
u32 g_SystemClockUtcOffset = INVALID_CLOCK_OFFSET; // NULL for non-pointer isn't correct

bool IsValidTimeOffset(u32 timeOffset)
{
	// timeOffset valid values are +/- 12 hours ~= +/- 43200...
	// because time programming is really odd, only validate +/- 45,000
	u32 x = 43200;
	u32 y = 0 - x;
	return ((timeOffset <= x) || (timeOffset >= y))
}

Result InitializeClockOffset() {
	/* Compares the system clock with current UTC time from timeapi.com */
	/* ESSENTIAL for correct OTP generation, unless system clock is UTC */
	/* Returns difference, in seconds. */
	/* Add the result of this function to the 3DS time to get UTC. */
	
	if (IsValidTimeOffset(g_SystemClockUtcOffset)) return true;
	
	Result ret = 0;
	u32 statusCode = 0;
	u32 contentSize = 0;
	unsigned char *buffer = NULL;

	httpcContext context;
	httpcInit(0);
	
	char * url = "http://flipnote.nonm.co.uk/time.php";

	/* URL returning current time in UTC */
	
	if (R_SUCCESS(ret)) {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
	}
	if (R_SUCCESS(ret)) {
		ret = httpcBeginRequest(&context);
	}
	if (R_SUCCESS(ret)) {
		ret = httpcGetResponseStatusCode(&context, &statusCode, 0);
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
		time_t utcTime = (time_t)strtol(buffer, NULL, 10);
		time_t systemTime = time(NULL);
		u32 timeDifference = systemTime - utcTime; /* time(NULL) + timeDifference = UTC */
		g_SystemClockUtcOffset = timeDifference;
	}
	if (NULL != buffer) {
		free(buffer);
	}
	return ret;
}

u32 currentTimeUTC() {
	u32 timeNow = (u32)time(NULL);
	u32 timeDifference = g_SystemClockUtcOffset;	
	timeOffset = timeNow - timeDifference;
	return timeOffset;
	}

int main() {
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	
	printf("%s %s by %s\n", APP_TITLE, APP_VERSION, APP_AUTHOR);
	printf("Build date: %s %s\n\n", __DATE__, __TIME__);
	//printf("Current time: %d\n\n", (int)time(NULL));
	
	printf("Calculating time offset ... make sure you are connected to the Internet. ");
	
	Result InitClockOffsetResult = InitializeClockOffset();
	if (!R_SUCCESS(InitClockOffsetResult)) {
		printf("Error initializing time offset: %08x", InitClockOffsetResult);
		return 1;
	}

	u32 rightnow = currentTimeUTC();
	printf("OK\n");	
	printf("Current time (Unix timestamp): %d\n\n", rightnow);
	
	ret = oath_init();
	if(ret != OATH_OK) {
		printf("Error initializing liboath: %s\n", oath_strerror(ret));
		return 1;
	}
	
	char *secret;
	
	FILE *secretFile;
	if((secretFile = fopen("secret.txt", "r")) == NULL) {
		printf("Secret.txt not found in application directory - this file is required to generate an OTP. Press A to exit.");
		while(aptMainLoop()) {
			gspWaitForVBlank();
			hidScanInput();
			
			u32 kDown = hidKeysDown();
			if(kDown & KEY_A) break;
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
		exit(1);
	}
	
	printf("Opened secret.txt\n");
	
	//free(enc_secret);
	
	char encoded_secret[1024];
	fscanf(secretFile, "%[^\n]", encoded_secret);
	printf("Secret read from secret.txt: %s\n", encoded_secret);
	fclose(secretFile);
	if(strlen(encoded_secret) < 1){
		printf("Zero length!\n");
	}
	
	ret = oath_base32_decode(&encoded_secret, strlen(encoded_secret), &secret, NULL);
	if(ret != OATH_OK) {
		printf("Error decoding secret: %s\n", oath_strerror(ret));
		return 1;
	}
	
	/*int hex_secret_size = (2*strlen(secret) + 1);
	char hex_secret[hex_secret_size];
	
	oath_bin2hex(secret, strlen(secret), hex_secret);
	
	printf("Secret: %s\n", hex_secret); */
	
	char otp[7]; /* 6 digits + NULL */
	
	ret = oath_totp_generate(secret, strlen(secret), rightnow, OATH_TOTP_DEFAULT_TIME_STEP_SIZE, OATH_TOTP_DEFAULT_START_TIME, 6, (char*)&otp);
	if(ret != OATH_OK) {
		printf("Error generating TOTP: %s\n", oath_strerror(ret));
		return 1;
	}
	printf("\n\nOTP is: %s\n", otp);
	
	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	gfxExit();
	return 0;
}
