#include <msettings.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
	InitSettings();

	sleep(1);
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	return 0;
}
