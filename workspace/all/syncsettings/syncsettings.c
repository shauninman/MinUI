/**
 * syncsettings.c - Hardware settings synchronization utility
 *
 * Ensures hardware settings (volume and brightness) are properly applied
 * after system startup or wake from sleep. This is necessary on some
 * platforms where hardware state doesn't persist across power state changes.
 *
 * The 1-second delay allows the hardware initialization to complete before
 * applying settings, preventing race conditions with device driver startup.
 */

#include <msettings.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Synchronizes volume and brightness settings with hardware.
 *
 * This utility is called during system initialization to ensure the
 * hardware matches the saved settings values. The sleep delay prevents
 * race conditions with device driver initialization.
 *
 * @param argc Argument count (unused)
 * @param argv Argument vector (unused)
 * @return 0 on success
 */
int main(int argc, char* argv[]) {
	InitSettings();

	// Wait for hardware initialization to complete
	sleep(1);

	// Re-apply saved settings to hardware
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());

	return 0;
}
