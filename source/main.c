#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <liboath/oath.h>

int ret;
u32 timeOffset = NULL;
char* enc_secret = "HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ"; /* secret (base32) */

u32 sysClockOffset() {
	/* Compares the system clock with current UTC time from timeapi.com */
	/* ESSENTIAL for correct OTP generation, unless system clock is UTC */
	/* Returns difference, in seconds. */
	/* Add the result of this function to the 3DS time to get UTC. */
	
	if(timeOffset != NULL) return timeOffset; /* a Very Low Number never reached in normal use. */
	
	Result ret = 0;
	u32 statusCode = 0;
	u32 contentSize = 0;
	unsigned char *buffer;

	httpcContext context;
	httpcInit(0);
	
	char *url = "http://flipnote.nonm.co.uk/time.php";
	/* URL returning current time in UTC */
	ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
	if(ret != 0) return ret;
	
	ret = httpcBeginRequest(&context);
	if(ret != 0) return ret;
	
	ret = httpcGetResponseStatusCode(&context, &statusCode, 0);
	if(ret != 0) return ret;
	if(statusCode != 200) printf("WARNING: Status code returned was %d\n", statusCode);
	
	ret = httpcGetDownloadSizeState(&context, NULL, &contentSize);
	if(ret != 0) return ret;
	
	buffer = (unsigned char*)malloc(contentSize+1);
	if(buffer == NULL) return -1;
	memset(buffer, 0, contentSize);
	
	ret = httpcDownloadData(&context, buffer, contentSize, NULL);
	if(ret != 0) {
		free(buffer);
		return ret;
	}
	
	time_t utcTime = (time_t)strtol(buffer, NULL, 10);
	
	time_t systemTime = time(NULL);
	
	u32 timeDifference = systemTime - utcTime; /* time(NULL) + timeDifference = UTC */

	free(buffer);
	return timeDifference;
	}

u32 currentTimeUTC() {
	u32 timeNow = (u32)time(NULL);
	u32 timeDifference = sysClockOffset();
	
	timeOffset = timeNow - timeDifference;
	return timeOffset;
	}

int main() {
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	
	printf("%s %s by %s\n", APP_TITLE, APP_VERSION, APP_AUTHOR);
	printf("Build date: %s %s\n\n", __DATE__, __TIME__);
	//printf("Current time: %d\n\n", (int)time(NULL));
	
	printf("Calculating time ... make sure you are connected to the Internet. ");
	
	u32 rightnow = currentTimeUTC(); printf("OK\n");
	
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
	if(strlen(enc_secret) < 1){
		printf("Zero length!\n");
	}
	
	ret = oath_base32_decode(enc_secret, strlen(enc_secret), &secret, NULL);
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
