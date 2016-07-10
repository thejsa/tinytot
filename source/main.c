#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <liboath/oath.h>

int ret;
signed long clockOffset = -2147483647; /* will not reach this value ever on the 3DS */

signed long sysClockOffset() {
	/* Compares the system clock with the current UTC time from my web server,
	 * returning the difference in seconds.
	 * Add the return value of this function to the system clock time to get the current time in UTC. */
	
	if(clockOffset != -2147483647) return clockOffset;
	
	Result ret = 0;
	unsigned long statusCode = 0;
	unsigned long contentSize = 0;
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
	
	time_t utcTime = (time_t)atol(buffer);
	
	time_t systemTime = time(NULL);
	
	clockOffset = systemTime - utcTime; /* time(NULL) + timeDifference = UTC */
	free(buffer);
	return clockOffset;
	}

unsigned long currentTimeUTC() {
	unsigned long systemTime = (unsigned long)time(NULL);
	signed long difference = sysClockOffset();
	return (unsigned long)(systemTime - difference);
	}
	
unsigned long generateTOTP(unsigned char *secret, signed short *secretLength) {
	if(*secretLength < 1) {
		printf("Secret is zero-length, cannot generate TOTP\n");
		return 0;
	}
	
	unsigned long timerightnow = currentTimeUTC();
	unsigned char otp[7]; /* 6 digits + NULL */
	
	ret = oath_totp_generate(secret, (size_t)*secretLength, timerightnow, OATH_TOTP_DEFAULT_TIME_STEP_SIZE, OATH_TOTP_DEFAULT_START_TIME, 6, (char*)&otp);
	if(ret != OATH_OK) {
		printf("Error generating TOTP: %s\n", oath_strerror(ret));
		return 0;
	}
	
	return atol(otp);
	}

int main() {
	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);
	
	printf("%s %s by %s\n", APP_TITLE, APP_VERSION, APP_AUTHOR);
	printf("Build date: %s %s\n\n", __DATE__, __TIME__);
	//printf("Current time: %d\n\n", (int)time(NULL));
	
	printf("Calculating time difference from UTC... make sure you are connected to the Internet. ");
	
	sysClockOffset();
	printf("OK\n");
	
	ret = oath_init();
	if(ret != OATH_OK) {
		printf("Error initializing liboath: %s\n", oath_strerror(ret));
		exit(1);
	}
	
	FILE *secretFile;
	if((secretFile = fopen("secret.txt", "r")) == NULL) {
		printf("Secret.txt not found in application directory - this file is required to generate an OTP. Press A to exit.");
		while(aptMainLoop()) {
			gspWaitForVBlank();
			hidScanInput();
			
			unsigned long kDown = hidKeysDown();
			if(kDown & KEY_A) break;
			gfxFlushBuffers();
			gfxSwapBuffers();
		}
		return 0;
	}
	
	printf("Opened secret.txt\n");
	
	//free(enc_secret);
	
	char encoded_secret[1024];
	fscanf(secretFile, "%[^\n]", encoded_secret);
	fclose(secretFile);
	
	if(strlen(encoded_secret) < 1){
		printf("Secret is zero length.\n");
		return 1;
	}
	
	printf("Encoded secret: %s\n", encoded_secret);
	
	char *secret;
	signed short secretLength;
	
	ret = oath_base32_decode(&encoded_secret, strlen(&encoded_secret), &secret, &secretLength);
	if(ret != OATH_OK) {
		printf("Error decoding secret: %s\n", oath_strerror(ret));
		exit(1);
	}
	
	printf("Press A to generate a one-time password.\n\n");
	
	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		unsigned long kDown = hidKeysDown();
		if (kDown & KEY_A) {
			unsigned long otp = generateTOTP(secret, &secretLength);
			if(otp == 0) {
				printf("\nOTP generation failed.\n");
				exit(1);
			}
			printf("OTP: %06lu\n\n");
		}
		
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	gfxExit();
	return 0;
}
