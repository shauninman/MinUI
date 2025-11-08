/**
 * msettings.h - macOS platform settings API
 *
 * Provides platform-specific settings management for the macOS development
 * environment. This is a stub implementation that allows MinUI to be built
 * and tested on macOS without requiring actual hardware access.
 *
 * @note This is a development-only header for macOS builds. Real hardware
 *       platforms implement these functions with actual device control.
 */

#ifndef __msettings_h__
#define __msettings_h__

/**
 * Initializes the settings subsystem.
 *
 * Prepares the platform for reading and writing brightness, volume,
 * and audio output settings.
 */
void InitSettings(void);

/**
 * Shuts down the settings subsystem.
 *
 * Releases any resources allocated by InitSettings().
 */
void QuitSettings(void);

/**
 * Gets the current brightness level.
 *
 * @return Brightness level in normalized range (0-10)
 */
int GetBrightness(void);

/**
 * Gets the current volume level.
 *
 * @return Volume level in normalized range (0-20)
 */
int GetVolume(void);

/**
 * Sets the raw hardware brightness value.
 *
 * @param value Hardware-specific brightness (0-255)
 * @note Direct hardware value, not normalized. Use SetBrightness() for normalized values.
 */
void SetRawBrightness(int value);

/**
 * Sets the raw hardware volume value.
 *
 * @param value Hardware-specific volume (0-160)
 * @note Direct hardware value, not normalized. Use SetVolume() for normalized values.
 */
void SetRawVolume(int value);

/**
 * Sets the brightness level.
 *
 * @param value Normalized brightness level (0-10, where 0 is minimum and 10 is maximum)
 */
void SetBrightness(int value);

/**
 * Sets the volume level.
 *
 * @param value Normalized volume level (0-20, where 0 is muted and 20 is maximum)
 */
void SetVolume(int value);

/**
 * Gets the headphone jack status.
 *
 * @return 1 if headphones are connected, 0 otherwise
 */
int GetJack(void);

/**
 * Sets the headphone jack mode.
 *
 * @param value 1 to enable headphone output, 0 to disable
 * @note On real hardware, this may switch between internal speaker and headphone jack
 */
void SetJack(int value);

/**
 * Gets the HDMI output status.
 *
 * @return 1 if HDMI output is enabled, 0 otherwise
 */
int GetHDMI(void);

/**
 * Sets the HDMI output mode.
 *
 * @param value 1 to enable HDMI output, 0 to disable
 * @note On real hardware, this may switch between LCD and HDMI display
 */
void SetHDMI(int value);

/**
 * Gets the mute status.
 *
 * @return 1 if audio is muted, 0 otherwise
 */
int GetMute(void);

#endif  // __msettings_h__
