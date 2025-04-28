#ifndef __msettings_h__
#define __msettings_h__

#define SETTINGS_DEFAULT_BRIGHTNESS 2
#define SETTINGS_DEFAULT_COLORTEMP 20
#define SETTINGS_DEFAULT_CONTRAST 0
#define SETTINGS_DEFAULT_SATURATION 0
#define SETTINGS_DEFAULT_EXPOSURE 0
#define SETTINGS_DEFAULT_VOLUME 8
#define SETTINGS_DEFAULT_HEADPHONE_VOLUME 4

#define SETTINGS_DEFAULT_MUTE_NO_CHANGE -69

void InitSettings(void);
void QuitSettings(void);
int InitializedSettings(void);

int GetBrightness(void);
int GetColortemp(void);
int GetContrast(void);
int GetSaturation(void);
int GetExposure(void);
int GetVolume(void);

void SetRawBrightness(int value); // 0-255
void SetRawColortemp(int value); // 0-255
void SetRawContrast(int value); // 0-100
void SetRawSaturation(int value); // 0-100
void SetRawExposure(int value); // 0-100
void SetRawVolume(int value); // 0-100

void SetBrightness(int value); // 0-10
void SetColortemp(int value); // 0-40
void SetContrast(int value); // -4-5
void SetSaturation(int value); // -5-5
void SetExposure(int value); // -4-5
void SetVolume(int value); // 0-20

int GetJack(void);
void SetJack(int value); // 0-1

int GetHDMI(void);
void SetHDMI(int value); // 0-1

int GetMute(void);
void SetMute(int value); // 0-1

// custom mute mode persistence layer

int  GetMutedBrightness(void);
int  GetMutedColortemp(void);
int  GetMutedContrast(void);
int  GetMutedSaturation(void);
int  GetMutedExposure(void);
int  GetMutedVolume(void);

void SetMutedBrightness(int);
void SetMutedColortemp(int);
void SetMutedContrast(int);
void SetMutedSaturation(int);
void SetMutedExposure(int);
void SetMutedVolume(int);

#endif  // __msettings_h__
