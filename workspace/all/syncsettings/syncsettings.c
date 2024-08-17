#include <stdio.h>
#include <unistd.h>
#include <msettings.h>

int main (int argc, char *argv[]) {
	InitSettings();
	
	sleep(1);
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	return 0;
}
