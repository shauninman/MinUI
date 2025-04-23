// macos
#include <stdio.h>
#include <stdlib.h>
// #include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "msettings.h"

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"
#include "opengl.h"

///////////////////////////////////////

// shader stuff
typedef struct Shader {
	GLuint shader_p;
	int scale;
	int filter;
	char *filename;
} Shader;

GLuint g_shader_default = 0;
GLuint g_shader_color = 0;
GLuint g_shader_overlay = 0;

Shader* shaders[3] = {
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR }, 
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR }, 
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR }
};

int nrofshaders = 3; // choose between 1 and 3 pipelines, > pipelines = more cpu usage, but more shader options and shader upscaling stuff

// Legacy MinUI settings
typedef struct SettingsV3 {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack;
} SettingsV3;

// First NextUI settings format
typedef struct SettingsV4 {
	int version; // future proofing
	int brightness;
	int colortemperature; // 0-20
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack; 
} SettingsV4;

// Current NextUI settings format
typedef struct SettingsV5 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV5;


// Third NextUI settings format
typedef struct SettingsV6 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV6;

typedef struct SettingsV7 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int mutedbrightness;
	int mutedcolortemperature;
	int mutedcontrast;
	int mutedsaturation;
	int mutedexposure;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV7;

typedef struct SettingsV8 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int toggled_brightness;
	int toggled_colortemperature;
	int toggled_contrast;
	int toggled_saturation;
	int toggled_exposure;
	int toggled_volume;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV8;

// When incrementing SETTINGS_VERSION, update the Settings typedef and add
// backwards compatibility to InitSettings!
#define SETTINGS_VERSION 8
typedef SettingsV8 Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = SETTINGS_DEFAULT_BRIGHTNESS,
	.colortemperature = SETTINGS_DEFAULT_COLORTEMP,
	.headphones = SETTINGS_DEFAULT_HEADPHONE_VOLUME,
	.speaker = SETTINGS_DEFAULT_VOLUME,
	.mute = 0,
	.contrast = SETTINGS_DEFAULT_CONTRAST,
	.saturation = SETTINGS_DEFAULT_SATURATION,
	.exposure = SETTINGS_DEFAULT_EXPOSURE,
	.toggled_brightness = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_colortemperature = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_contrast = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_saturation = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_exposure = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_volume = 0, // mute is default
	.jack = 0,
};
static Settings* msettings;

static char SettingsPath[256];

///////////////////////////////////////

int peekVersion(const char *filename) {
	int version = 0;
	FILE *file = fopen(filename, "r");
	if (file) {
		fread(&version, sizeof(int), 1, file);
		fclose(file);
	}
	return version;
}

void InitSettings(void){
	// We are not really using them, but we should be able to debug them
	sprintf(SettingsPath, "%s/msettings.bin", SDCARD_PATH "/.userdata");
	msettings = (Settings*)malloc(sizeof(Settings));
	
	int version = peekVersion(SettingsPath);
	if(version > 0) {
		// fopen file pointer
		int fd = open(SettingsPath, O_RDONLY);
		if(fd) {
			if (version == SETTINGS_VERSION) {
				read(fd, msettings, sizeof(Settings));
			}
			else if(version==7) {
				SettingsV7 old;
				read(fd, &old, sizeof(SettingsV7));
				// default muted
				msettings->toggled_volume = 0;
				// muted* -> toggled*
				msettings->toggled_brightness = old.mutedbrightness;
				msettings->toggled_colortemperature = old.mutedcolortemperature;
				msettings->toggled_contrast = old.mutedcontrast;
				msettings->toggled_exposure = old.mutedexposure;
				msettings->toggled_saturation = old.mutedsaturation;
				// copy the rest
				msettings->saturation = old.saturation;
				msettings->contrast = old.contrast;
				msettings->exposure = old.exposure;
				msettings->colortemperature = old.colortemperature;
				msettings->brightness = old.brightness;
				msettings->headphones = old.headphones;
				msettings->speaker = old.speaker;
				msettings->mute = old.mute;
				msettings->jack = old.jack;
			}
			else if(version==6) {
				SettingsV6 old;
				read(fd, &old, sizeof(SettingsV6));
				// no muted* settings yet, default values used.
				msettings->toggled_brightness = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
				msettings->toggled_colortemperature = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
				msettings->toggled_contrast = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
				msettings->toggled_exposure = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
				msettings->toggled_saturation = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
				// copy the rest
				msettings->saturation = old.saturation;
				msettings->contrast = old.contrast;
				msettings->exposure = old.exposure;
				msettings->colortemperature = old.colortemperature;
				msettings->brightness = old.brightness;
				msettings->headphones = old.headphones;
				msettings->speaker = old.speaker;
				msettings->mute = old.mute;
				msettings->jack = old.jack;
			}
			else if(version==5) {
				SettingsV5 old;
				read(fd, &old, sizeof(SettingsV5));
				// no display settings yet, default values used. 
				msettings->saturation = 0;
				msettings->contrast = 0;
				msettings->exposure = 0;
				// copy the rest
				msettings->colortemperature = old.colortemperature;
				msettings->brightness = old.brightness;
				msettings->headphones = old.headphones;
				msettings->speaker = old.speaker;
				msettings->mute = old.mute;
				msettings->jack = old.jack;
			}
			else if(version==4) {
				printf("Found settings v4.\n");
				SettingsV4 old;
				// read old settings from fd
				read(fd, &old, sizeof(SettingsV4));
				// colortemp was 0-20 here
				msettings->colortemperature = old.colortemperature * 2;
			}
			else if(version==3) {
				printf("Found settings v3.\n");
				SettingsV3 old;
				read(fd, &old, sizeof(SettingsV3));
				// no colortemp setting yet, default value used. 
				// copy the rest
				msettings->brightness = old.brightness;
				msettings->headphones = old.headphones;
				msettings->speaker = old.speaker;
				msettings->mute = old.mute;
				msettings->jack = old.jack;
				msettings->colortemperature = 20;
			}
			else {
				printf("Found unsupported settings version: %i.\n", version);
				// load defaults
				memcpy(msettings, &DefaultSettings, sizeof(Settings));
			}

			close(fd);
		}
		else {
			printf("Unable to read settings, using defaults\n");
			// load defaults
			memcpy(msettings, &DefaultSettings, sizeof(Settings));
		}
	}
	else {
		printf("No settings found, using defaults\n");
		// load defaults
		memcpy(msettings, &DefaultSettings, sizeof(Settings));
	}
}
void QuitSettings(void){
	// dealloc settings
	free(msettings);
	msettings = NULL;
}

int GetBrightness(void) { return 0; }
int GetColortemp(void) { return 0; }
int GetContrast(void) { return 0; }
int GetSaturation(void) { return 0; }
int GetExposure(void) { return 0; }
int GetVolume(void) { return 0; }

int GetMutedBrightness(void) { return 0; }
int GetMutedColortemp(void) { return 0; }
int GetMutedContrast(void) { return 0; }
int GetMutedSaturation(void) { return 0; }
int GetMutedExposure(void) { return 0; }
int GetMutedVolume(void) { return 0; }

void SetMutedBrightness(int value){}
void SetMutedColortemp(int value){}
void SetMutedContrast(int value){}
void SetMutedSaturation(int value){}
void SetMutedExposure(int value){}
void SetMutedVolume(int value){}

void SetRawBrightness(int value) {}
void SetRawVolume(int value){}

void SetBrightness(int value) {}
void SetColortemp(int value) {}
void SetContrast(int value) {}
void SetSaturation(int value) {}
void SetExposure(int value) {}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }

///////////////////////////////

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* target_layer1;
	SDL_Texture* target_layer2;
	SDL_Texture* stream_layer1;
	SDL_Texture* target_layer3;
	SDL_Texture* target_layer4;
	SDL_Texture* target;
	SDL_Texture* effect;
	SDL_Texture* overlay;
	SDL_Surface* screen;
	SDL_GLContext gl_context;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
} vid;

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;
static uint32_t SDL_transparentBlack = 0;
#define OVERLAYS_FOLDER SDCARD_PATH "/Overlays"

static char* overlay_path = NULL;


GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        char* log = (char*)malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        printf("Program link error: %s\n", log);
        free(log);
    }
	LOG_info("program linked\n");
    return program;
}

char* load_shader_source(const char* filename) {
	char filepath[256];
	snprintf(filepath, sizeof(filepath), "%s", filename);
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", filepath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* source = (char*)malloc(length + 1);
    if (!source) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);
    return source;
}

GLuint load_shader_from_file(GLenum type, const char* filename, const char* path) {
	char filepath[256];
	snprintf(filepath, sizeof(filepath), "%s/%s", path,filename);
    char* source = load_shader_source(filepath);
    if (!source) return 0;

    const char* define = NULL;
    const char* default_precision = NULL;
    if (type == GL_VERTEX_SHADER) {
        define = "#define VERTEX\n";
    } else if (type == GL_FRAGMENT_SHADER) {
        define = "#define FRAGMENT\n";
        default_precision =
            "#ifdef GL_ES\n"
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
            "precision highp float;\n"
            "#else\n"
            "precision mediump float;\n"
            "#endif\n"
            "#endif\n";
    } else {
        fprintf(stderr, "Unsupported shader type\n");
        free(source);
        return 0;
    }

    const char* version_start = strstr(source, "#version");
    const char* version_end = version_start ? strchr(version_start, '\n') : NULL;

    const char* replacement_version = "#version 300 es\n";
    const char* fallback_version = "#version 100\n";

    char* combined = NULL;
    size_t define_len = strlen(define);
    size_t precision_len = default_precision ? strlen(default_precision) : 0;
    size_t source_len = strlen(source);
    size_t combined_len = 0;

    // Helper: check if the version is one of the desktop ones to upgrade
    int should_replace_with_300es = 0;
    if (version_start && version_end) {
        char version_str[32] = {0};
        size_t len = version_end - version_start;
        if (len < sizeof(version_str)) {
            strncpy(version_str, version_start, len);
            version_str[len] = '\0';

            // Check for desktop GLSL versions that should be replaced
            if (
                strstr(version_str, "#version 110") ||
                strstr(version_str, "#version 120") ||
                strstr(version_str, "#version 130") ||
                strstr(version_str, "#version 140") ||
                strstr(version_str, "#version 150") ||
                strstr(version_str, "#version 330") ||
                strstr(version_str, "#version 400") ||
                strstr(version_str, "#version 410") ||
                strstr(version_str, "#version 420") ||
                strstr(version_str, "#version 430") ||
                strstr(version_str, "#version 440") ||
                strstr(version_str, "#version 450")
            ) {
                should_replace_with_300es = 1;
            }
        }
    }

    if (version_start && version_end && should_replace_with_300es) {
        // Replace old desktop version with 300 es
        size_t header_len = version_end - source + 1;
        size_t version_len = strlen(replacement_version);
        combined_len = version_len + define_len + precision_len + (source_len - header_len) + 1;
        combined = (char*)malloc(combined_len);
        if (!combined) {
            fprintf(stderr, "Out of memory\n");
            free(source);
            return 0;
        }

        strcpy(combined, replacement_version);
        strcat(combined, define);
        if (default_precision) strcat(combined, default_precision);
        strcat(combined, source + header_len);
    } else if (version_start && version_end) {
        // Keep existing version, insert define after it
        size_t header_len = version_end - source + 1;
        combined_len = header_len + define_len + precision_len + (source_len - header_len) + 1;
        combined = (char*)malloc(combined_len);
        if (!combined) {
            fprintf(stderr, "Out of memory\n");
            free(source);
            return 0;
        }

        memcpy(combined, source, header_len);
        memcpy(combined + header_len, define, define_len);
        if (default_precision)
            memcpy(combined + header_len + define_len, default_precision, precision_len);
        strcpy(combined + header_len + define_len + precision_len, source + header_len);
    } else {
        // No version — use fallback
        size_t version_len = strlen(fallback_version);
        combined_len = version_len + define_len + precision_len + source_len + 1;
        combined = (char*)malloc(combined_len);
        if (!combined) {
            fprintf(stderr, "Out of memory\n");
            free(source);
            return 0;
        }

        strcpy(combined, fallback_version);
        strcat(combined, define);
        if (default_precision) strcat(combined, default_precision);
        strcat(combined, source);
    }

    GLuint shader = glCreateShader(type);
    const char* combined_ptr = combined;
    glShaderSource(shader, 1, &combined_ptr, NULL);
    glCompileShader(shader);

    free(source);
    free(combined);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compilation failed:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}




SDL_Surface* PLAT_initVideo(void) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	//SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

	if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		LOG_info("Error intializing SDL: %s\n", SDL_GetError());
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	//
	// LOG_info("Available video drivers:\n");
	// for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetVideoDriver(i));
	// }
	// LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	//
	// LOG_info("Available render drivers:\n");
	// for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
	// 	SDL_RendererInfo info;
	// 	SDL_GetRenderDriverInfo(i,&info);
	// 	LOG_info("- %s\n", info.name);
	// }
	//
	// LOG_info("Available display modes:\n");
	// SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;

	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	if(!vid.window)
		LOG_info("Error creating SDL window: %s\n", SDL_GetError());
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"1");

	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
	
	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("Current render driver: %s\n", info.name);

	vid.gl_context = SDL_GL_CreateContext(vid.window);
	SDL_GL_MakeCurrent(vid.window, vid.gl_context);
	glViewport(0, 0, w, h);

	
	GLuint default_vertex = load_shader_from_file(GL_VERTEX_SHADER, "system/default.glsl");
	GLuint default_fragment = load_shader_from_file(GL_FRAGMENT_SHADER, "system/default.glsl");
	g_shader_default = link_program(default_vertex, default_fragment);

	GLuint color_vshader = load_shader_from_file(GL_VERTEX_SHADER, "system/colorfix.glsl");
	GLuint color_shader = load_shader_from_file(GL_FRAGMENT_SHADER, "system/colorfix.glsl");
	g_shader_color = link_program(color_vshader, color_shader);

	GLuint overlay_vshader = load_shader_from_file(GL_VERTEX_SHADER, "system/overlay.glsl");
	GLuint overlay_shader = load_shader_from_file(GL_FRAGMENT_SHADER, "system/overlay.glsl");
	g_shader_overlay = link_program(overlay_vshader, overlay_shader);

	GLuint vertex_shader1 = load_shader_from_file(GL_VERTEX_SHADER, "default.glsl");
	GLuint fragment_shader1 = load_shader_from_file(GL_FRAGMENT_SHADER, "default.glsl"); 
	shaders[0]->shader_p = link_program(vertex_shader1, fragment_shader1);

	GLuint vertex_shader2 = load_shader_from_file(GL_VERTEX_SHADER, "default.glsl");
	GLuint fragment_shader2 = load_shader_from_file(GL_FRAGMENT_SHADER, "default.glsl"); 
	shaders[1]->shader_p =  link_program(vertex_shader2, fragment_shader2);

	GLuint vertex_shader3 = load_shader_from_file(GL_VERTEX_SHADER, "default.glsl");
	GLuint fragment_shader3 = load_shader_from_file(GL_FRAGMENT_SHADER, "default.glsl"); 
	shaders[2]->shader_p =  link_program(vertex_shader3, fragment_shader3);



	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer2 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer3 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer4 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	
	vid.target	= NULL; // only needed for non-native sizes
	
	vid.screen = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);

	SDL_SetSurfaceBlendMode(vid.screen, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer2, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer3, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer4, SDL_BLENDMODE_BLEND);
	
	
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;

	SDL_transparentBlack = SDL_MapRGBA(vid.screen->format, 0, 0, 0, 0);
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}
int shadersupdated = 0;

void PLAT_resetShaders() {
	shadersupdated = 1;
}

void PLAT_updateShader(int i, const char *filename, int *scale, int *filter) {
    // Check if the shader index is valid
    if (i < 0 || i >= 3) {
        LOG_error("Invalid shader index %d\n", i);
        return;
    }

    Shader* shader = shaders[i];

    // Only update shader_p if filename is not NULL
    if (filename != NULL && filename != shader->filename) {
        SDL_GL_MakeCurrent(vid.window, vid.gl_context);
        
        GLuint vertex_shader1 = load_shader_from_file(GL_VERTEX_SHADER, filename,SHADERS_FOLDER);
        GLuint fragment_shader1 = load_shader_from_file(GL_FRAGMENT_SHADER, filename,SHADERS_FOLDER);
        
        // Link the shader program
        shader->shader_p = link_program(vertex_shader1, fragment_shader1);
        
        if (shader->shader_p == 0) {
            LOG_error("Shader linking failed for %s\n", filename);
        }

        GLint success = 0;
        glGetProgramiv(shader->shader_p, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shader->shader_p, 512, NULL, infoLog);
            LOG_error("Shader Program Linking Failed: %s\n", infoLog);
        }
        
        LOG_info("Shader set now to %s\n", filename);
    }

    // Only update scale if it's not NULL
    if (scale != NULL) {
        shader->scale = *scale +1;
    }

    // Only update filter if it's not NULL
    if (filter != NULL) {
        shader->filter = (*filter == 1) ? GL_LINEAR : GL_NEAREST;
    }
}
void PLAT_setShaders(int nr) {
	LOG_info("set nr of shaders to %i\n",nr);
	nrofshaders = nr;
}


static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, SDL_transparentBlack);
		SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);

	if (vid.target_layer3) SDL_DestroyTexture(vid.target_layer3);
	if (vid.target_layer1) SDL_DestroyTexture(vid.target_layer1);
	if (vid.target_layer2) SDL_DestroyTexture(vid.target_layer2);
	if (vid.target_layer4) SDL_DestroyTexture(vid.target_layer4);
	SDL_DestroyTexture(vid.stream_layer1);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// SDL_FillRect(screen, NULL, 0); // TODO: revisit
	SDL_FillRect(screen, NULL, SDL_transparentBlack);
}
void PLAT_clearAll(void) {
	PLAT_clearLayers(0);
	PLAT_clearVideo(vid.screen); 
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_DestroyTexture(vid.stream_layer1);
	
	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_setEffect(int next_type) {
}

void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
	scale1x1_c16(
		renderer->src,renderer->dst,
		renderer->true_w,renderer->true_h,renderer->src_p,
		vid.screen->w,vid.screen->h,vid.screen->pitch // fixed in this implementation
		// renderer->dst_w,renderer->dst_h,renderer->dst_p
	);
}

int screenx = 0;
int screeny = 0;
void PLAT_setOffsetX(int x) {
    if (x < 0 || x > 128) return;
    screenx = x - 64;
}
void PLAT_setOffsetY(int y) {
    if (y < 0 || y > 128) return;
    screeny = y - 64;  
}
static int overlayUpdated=0;
void PLAT_setOverlay(int select, const char* tag) {
    if (vid.overlay) {
        SDL_DestroyTexture(vid.overlay);
        vid.overlay = NULL;
    }

    // Array of overlay filenames
    static const char* overlay_files[] = {
        "",
        "overlay1.png",
        "overlay2.png",
        "overlay3.png",
        "overlay4.png",
        "overlay5.png"
    };
    
    int overlay_count = sizeof(overlay_files) / sizeof(overlay_files[0]);

    if (select < 0 || select >= overlay_count) {
        printf("Invalid selection. Skipping overlay update.\n");
        return;
    }

    const char* filename = overlay_files[select];

    if (!filename || strcmp(filename, "") == 0) {
		overlay_path = strdup("");
        printf("Skipping overlay update.\n");
        return;
    }



    size_t path_len = strlen(OVERLAYS_FOLDER) + strlen(tag) + strlen(filename) + 4; // +3 for slashes and null-terminator
    overlay_path = malloc(path_len);

    if (!overlay_path) {
        perror("malloc failed");
        return;
    }

    snprintf(overlay_path, path_len, "%s/%s/%s", OVERLAYS_FOLDER, tag, filename);
    printf("Overlay path set to: %s\n", overlay_path);
	overlayUpdated=1;
}

static void updateOverlay(void) {
	
	// LOG_info("effect: %s opacity: %i\n", effect_path, opacity);
	if(!vid.overlay) {
		if(overlay_path) {
			SDL_Surface* tmp = IMG_Load(overlay_path);
			if (tmp) {
				vid.overlay = SDL_CreateTextureFromSurface(vid.renderer, tmp);
				SDL_FreeSurface(tmp);
			}
		}
	}
}

void applyRoundedCorners(SDL_Surface* surface, SDL_Rect* rect, int radius) {
	if (!surface) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    SDL_PixelFormat* fmt = surface->format;
	SDL_Rect target = {0, 0, surface->w, surface->h};
	if (rect)
		target = *rect;
    
    Uint32 transparent_black = SDL_MapRGBA(fmt, 0, 0, 0, 0);  // Fully transparent black

	const int xBeg = target.x;
	const int xEnd = target.x + target.w;
	const int yBeg = target.y;
	const int yEnd = target.y + target.h;
	for (int y = yBeg; y < yEnd; ++y)
	{
		for (int x = xBeg; x < xEnd; ++x) {
            int dx = (x < xBeg + radius) ? xBeg + radius - x : (x >= xEnd - radius) ? x - (xEnd - radius - 1) : 0;
            int dy = (y < yBeg + radius) ? yBeg + radius - y : (y >= yEnd - radius) ? y - (yEnd - radius - 1) : 0;
            if (dx * dx + dy * dy > radius * radius) {
                pixels[y * target.w + x] = transparent_black;  // Set to fully transparent black
            }
        }
    }
}

void PLAT_clearLayers(int layer) {
	if(layer==0 || layer==1) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==2) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==3) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==4) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
		SDL_RenderClear(vid.renderer);
	}

	SDL_SetRenderTarget(vid.renderer, NULL);
}

void PLAT_drawOnLayer(SDL_Surface *inputSurface, int x, int y, int w, int h, float brightness, bool maintainAspectRatio,int layer) {
    if (!inputSurface || !vid.target_layer1 || !vid.renderer) return; 

    SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
                                                 SDL_PIXELFORMAT_RGBA8888, 
                                                 SDL_TEXTUREACCESS_TARGET,  
                                                 inputSurface->w, inputSurface->h); 

    if (!tempTexture) {
        printf("Failed to create temporary texture: %s\n", SDL_GetError());
        return;
    }

    SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
    switch (layer)
	{
	case 1:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		break;
	case 2:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		break;
	case 3:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
		break;
	case 4:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
		break;
	default:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		break;
	}

    // Adjust brightness
    Uint8 r = 255, g = 255, b = 255;
    if (brightness < 1.0f) {
        r = g = b = (Uint8)(255 * brightness);
    } else if (brightness > 1.0f) {
        r = g = b = 255;
    }

    SDL_SetTextureColorMod(tempTexture, r, g, b);

    // Aspect ratio handling
    SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h }; 
    SDL_Rect dstRect = { x, y, w, h };  

    if (maintainAspectRatio) {
        float aspectRatio = (float)inputSurface->w / (float)inputSurface->h;
    
        if (w / (float)h > aspectRatio) {
            dstRect.w = (int)(h * aspectRatio);
        } else {
            dstRect.h = (int)(w / aspectRatio);
        }
    }

    SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);
    SDL_SetRenderTarget(vid.renderer, NULL);
    SDL_DestroyTexture(tempTexture);
}

void PLAT_animateSurface(
	SDL_Surface *inputSurface,
	int x, int y,
	int target_x, int target_y,
	int w, int h,
	int duration_ms,
	int start_opacity,
	int target_opacity,
	int layer
) {
	if (!inputSurface || !vid.target_layer2 || !vid.renderer) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);  // Enable blending for opacity

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		// Interpolate opacity
		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);

		if (layer == 0)
			SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		else
			SDL_SetRenderTarget(vid.renderer, vid.target_layer4);

		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0); 
		SDL_RenderClear(vid.renderer);

		SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h };
		SDL_Rect dstRect = { current_x, current_y, w, h };
		SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();
	}

	SDL_DestroyTexture(tempTexture);
}

void PLAT_revealSurface(
	SDL_Surface *inputSurface,
	int x, int y,
	int w, int h,
	int duration_ms,
	const char* direction,
	int opacity,
	int layer
) {
	if (!inputSurface || !vid.target_layer2 || !vid.renderer) return;

	SDL_Surface* formatted = SDL_CreateRGBSurfaceWithFormat(0, inputSurface->w, inputSurface->h, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!formatted) {
		printf("Failed to create formatted surface: %s\n", SDL_GetError());
		return;
	}
	SDL_FillRect(formatted, NULL, SDL_MapRGBA(formatted->format, 0, 0, 0, 0));
	SDL_SetSurfaceBlendMode(inputSurface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(inputSurface,&(SDL_Rect){0,0,w,h}, formatted, &(SDL_Rect){0,0,w,h});

	SDL_Texture* tempTexture = SDL_CreateTextureFromSurface(vid.renderer, formatted);
	SDL_FreeSurface(formatted);

	if (!tempTexture) {
		printf("Failed to create texture: %s\n", SDL_GetError());
		return;
	}

	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(tempTexture, opacity);

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {
		float t = (float)frame / total_frames;
		if (t > 1.0f) t = 1.0f;

		int reveal_w = w;
		int reveal_h = h;
		int src_x = 0;
		int src_y = 0;

		if (strcmp(direction, "left") == 0) {
			reveal_w = (int)(w * t + 0.5f);
		}
		else if (strcmp(direction, "right") == 0) {
			reveal_w = (int)(w * t + 0.5f);
			src_x = w - reveal_w;
		}
		else if (strcmp(direction, "up") == 0) {
			reveal_h = (int)(h * t + 0.5f);
		}
		else if (strcmp(direction, "down") == 0) {
			reveal_h = (int)(h * t + 0.5f);
			src_y = h - reveal_h;
		}

		SDL_Rect srcRect = { src_x, src_y, reveal_w, reveal_h };
		SDL_Rect dstRect = { x + src_x, y + src_y, reveal_w, reveal_h };

		SDL_SetRenderTarget(vid.renderer, (layer == 0) ? vid.target_layer2 : vid.target_layer4);

		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);

		if (reveal_w > 0 && reveal_h > 0)
			SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(tempTexture);
}

static int text_offset = 0;

int PLAT_resetScrollText(TTF_Font* font, const char* in_name,int max_width) {
	int text_width, text_height;
	
    TTF_SizeUTF8(font, in_name, &text_width, &text_height);

	text_offset = 0;

	if (text_width <= max_width) {
		return 0;
	} else {
		return 1;
	}
}
void PLAT_scrollTextTexture(
    TTF_Font* font,
    const char* in_name,
    int x, int y,      // Position on target layer
    int w, int h,      // Clipping width and height
    int padding,
    SDL_Color color,
    float transparency
) {
   
    static int frame_counter = 0;

    if (transparency < 0.0f) transparency = 0.0f;
    if (transparency > 1.0f) transparency = 1.0f;
    color.a = (Uint8)(transparency * 255);

    char scroll_text[1024];
    snprintf(scroll_text, sizeof(scroll_text), "%s  %s", in_name, in_name);

	SDL_Surface* tempSur = TTF_RenderUTF8_Blended(font, scroll_text, color);
	SDL_Surface* text_surface = SDL_CreateRGBSurfaceWithFormat(0,
		tempSur->w, tempSur->h, 32, SDL_PIXELFORMAT_RGBA8888);
	
	SDL_FillRect(text_surface, NULL, THEME_COLOR1);
	SDL_BlitSurface(tempSur, NULL, text_surface, NULL);

    SDL_Texture* full_text_texture = SDL_CreateTextureFromSurface(vid.renderer, text_surface);
    int full_text_width = text_surface->w;
	int full_text_height = text_surface->h;
    SDL_FreeSurface(text_surface);
    SDL_FreeSurface(tempSur);

    if (!full_text_texture) {
        return;
    }

    SDL_SetTextureBlendMode(full_text_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(full_text_texture, color.a);

    SDL_SetRenderTarget(vid.renderer, vid.target_layer4);

    SDL_Rect src_rect = { text_offset, 0, w, full_text_height };
    SDL_Rect dst_rect = { x, y, w, full_text_height };

    SDL_RenderCopy(vid.renderer, full_text_texture, &src_rect, &dst_rect);

    SDL_SetRenderTarget(vid.renderer, NULL);
    SDL_DestroyTexture(full_text_texture);

    if (full_text_width > w + padding) {
        frame_counter++;
        if (frame_counter >= 1) {
            text_offset += 3;
            if (text_offset >= full_text_width / 2) {
                text_offset = 0;
            }
            frame_counter = 0;
        }
    } else {
        text_offset = 0;
    }

	PLAT_GPU_Flip();
}

// super fast without update_texture to draw screen
void PLAT_GPU_Flip() {
	SDL_RenderClear(vid.renderer);
	SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
	SDL_RenderPresent(vid.renderer);
}


void PLAT_animateAndRevealSurfaces(
	SDL_Surface* inputMoveSurface,
	SDL_Surface* inputRevealSurface,
	int move_start_x, int move_start_y,
	int move_target_x, int move_target_y,
	int move_w, int move_h,
	int reveal_x, int reveal_y,
	int reveal_w, int reveal_h,
	const char* reveal_direction,
	int duration_ms,
	int move_start_opacity,
	int move_target_opacity,
	int reveal_opacity,
	int layer1,
	int layer2
) {
	if (!inputMoveSurface || !inputRevealSurface || !vid.renderer || !vid.target_layer2) return;

	SDL_Texture* moveTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputMoveSurface->w, inputMoveSurface->h);
	if (!moveTexture) {
		printf("Failed to create move texture: %s\n", SDL_GetError());
		return;
	}
	SDL_UpdateTexture(moveTexture, NULL, inputMoveSurface->pixels, inputMoveSurface->pitch);
	SDL_SetTextureBlendMode(moveTexture, SDL_BLENDMODE_BLEND);

	SDL_Surface* formatted = SDL_CreateRGBSurfaceWithFormat(0, inputRevealSurface->w, inputRevealSurface->h, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!formatted) {
		SDL_DestroyTexture(moveTexture);
		printf("Failed to create formatted surface for reveal: %s\n", SDL_GetError());
		return;
	}
	SDL_FillRect(formatted, NULL, SDL_MapRGBA(formatted->format, 0, 0, 0, 0));
	SDL_SetSurfaceBlendMode(inputRevealSurface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(inputRevealSurface, &(SDL_Rect){0, 0, reveal_w, reveal_h}, formatted, &(SDL_Rect){0, 0, reveal_w, reveal_h});
	SDL_Texture* revealTexture = SDL_CreateTextureFromSurface(vid.renderer, formatted);
	SDL_FreeSurface(formatted);
	if (!revealTexture) {
		SDL_DestroyTexture(moveTexture);
		printf("Failed to create reveal texture: %s\n", SDL_GetError());
		return;
	}
	SDL_SetTextureBlendMode(revealTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(revealTexture, reveal_opacity);

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {
		float t = (float)frame / total_frames;
		if (t > 1.0f) t = 1.0f;

		int current_x = move_start_x + (int)((move_target_x - move_start_x) * t);
		int current_y = move_start_y + (int)((move_target_y - move_start_y) * t);
		int current_opacity = move_start_opacity + (int)((move_target_opacity - move_start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;
		SDL_SetTextureAlphaMod(moveTexture, current_opacity);

		int reveal_src_x = 0, reveal_src_y = 0;
		int reveal_draw_w = reveal_w, reveal_draw_h = reveal_h;

		if (strcmp(reveal_direction, "left") == 0) {
			reveal_draw_w = (int)(reveal_w * t + 0.5f);
		}
		else if (strcmp(reveal_direction, "right") == 0) {
			reveal_draw_w = (int)(reveal_w * t + 0.5f);
			reveal_src_x = reveal_w - reveal_draw_w;
		}
		else if (strcmp(reveal_direction, "up") == 0) {
			reveal_draw_h = (int)(reveal_h * t + 0.5f);
		}
		else if (strcmp(reveal_direction, "down") == 0) {
			reveal_draw_h = (int)(reveal_h * t + 0.5f);
			reveal_src_y = reveal_h - reveal_draw_h;
		}

		SDL_Rect revealSrc = { reveal_src_x, reveal_src_y, reveal_draw_w, reveal_draw_h };
		SDL_Rect revealDst = { reveal_x + reveal_src_x, reveal_y + reveal_src_y, reveal_draw_w, reveal_draw_h };

		SDL_SetRenderTarget(vid.renderer, (layer1 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(vid.renderer, (2 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);

		SDL_SetRenderTarget(vid.renderer, (layer1 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_Rect moveDst = { current_x, current_y, move_w, move_h };
		SDL_RenderCopy(vid.renderer, moveTexture, NULL, &moveDst);

		SDL_SetRenderTarget(vid.renderer, (layer2 == 0) ? vid.target_layer3 : vid.target_layer4);

		if (reveal_draw_w > 0 && reveal_draw_h > 0)
			SDL_RenderCopy(vid.renderer, revealTexture, &revealSrc, &revealDst);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(moveTexture);
	SDL_DestroyTexture(revealTexture);
}

void PLAT_animateSurfaceOpacity(
	SDL_Surface *inputSurface,
	int x, int y, int w, int h,
	int start_opacity, int target_opacity,
	int duration_ms,
	int layer
) {
	if (!inputSurface) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND); 

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	SDL_Texture* target_layer = (layer == 0) ? vid.target_layer2 : vid.target_layer4;
	if (!target_layer) {
		SDL_DestroyTexture(tempTexture);
		return;
	}

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;
		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);
		SDL_SetRenderTarget(vid.renderer, target_layer);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect dstRect = { x, y, w, h };
		SDL_RenderCopy(vid.renderer, tempTexture, NULL, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_flip(vid.screen,0);

	}

	SDL_DestroyTexture(tempTexture);
}

void PLAT_animateSurfaceOpacityAndScale(
	SDL_Surface *inputSurface,
	int x, int y,                         // Center position
	int start_w, int start_h,
	int target_w, int target_h,
	int start_opacity, int target_opacity,
	int duration_ms,
	int layer
) {
	if (!inputSurface || !vid.renderer) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND); 

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	SDL_Texture* target_layer = (layer == 0) ? vid.target_layer2 : vid.target_layer4;
	if (!target_layer) {
		SDL_DestroyTexture(tempTexture);
		return;
	}

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		int current_w = start_w + (int)((target_w - start_w) * t);
		int current_h = start_h + (int)((target_h - start_h) * t);

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);

		SDL_SetRenderTarget(vid.renderer, target_layer);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect dstRect = {
			x - current_w / 2,
			y - current_h / 2,
			current_w,
			current_h
		};

		SDL_RenderCopy(vid.renderer, tempTexture, NULL, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(tempTexture);
}

SDL_Surface* PLAT_captureRendererToSurface() {
	if (!vid.renderer) return NULL;

	int width, height;
	SDL_GetRendererOutputSize(vid.renderer, &width, &height);

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!surface) {
		printf("Failed to create surface: %s\n", SDL_GetError());
		return NULL;
	}

	Uint32 black = SDL_MapRGBA(surface->format, 0, 0, 0, 255);
	SDL_FillRect(surface, NULL, black);

	if (SDL_RenderReadPixels(vid.renderer, NULL, SDL_PIXELFORMAT_RGBA8888, surface->pixels, surface->pitch) != 0) {
		printf("Failed to read pixels from renderer: %s\n", SDL_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}

	// remove transparancy
	Uint32* pixels = (Uint32*)surface->pixels;
	int total_pixels = (surface->pitch / 4) * surface->h;

	for (int i = 0; i < total_pixels; i++) {
		Uint8 r, g, b, a;
		SDL_GetRGBA(pixels[i], surface->format, &r, &g, &b, &a);
		pixels[i] = SDL_MapRGBA(surface->format, r, g, b, 255);
	}

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
	return surface;
}

void PLAT_animateAndFadeSurface(
	SDL_Surface *inputSurface,
	int x, int y, int target_x, int target_y, int w, int h, int duration_ms,
	SDL_Surface *fadeSurface,
	int fade_x, int fade_y, int fade_w, int fade_h,
	int start_opacity, int target_opacity,int layer
) {
	if (!inputSurface || !vid.renderer) return;

	SDL_Texture* moveTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!moveTexture) {
		printf("Failed to create move texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(moveTexture, NULL, inputSurface->pixels, inputSurface->pitch);

	SDL_Texture* fadeTexture = NULL;
	if (fadeSurface) {
		fadeTexture = SDL_CreateTextureFromSurface(vid.renderer, fadeSurface);
		if (!fadeTexture) {
			printf("Failed to create fade texture: %s\n", SDL_GetError());
			SDL_DestroyTexture(moveTexture);
			return;
		}
		SDL_SetTextureBlendMode(fadeTexture, SDL_BLENDMODE_BLEND);
	}

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	Uint32 start_time = SDL_GetTicks();

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		switch (layer)
		{
		case 1:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
			break;
		case 2:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
			break;
		case 3:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
			break;
		case 4:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
			break;
		default:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
			break;
		}
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect moveSrcRect = { 0, 0, inputSurface->w, inputSurface->h };
		SDL_Rect moveDstRect = { current_x, current_y, w, h };
		SDL_RenderCopy(vid.renderer, moveTexture, &moveSrcRect, &moveDstRect);

		if (fadeTexture) {
			SDL_SetTextureAlphaMod(fadeTexture, current_opacity);
			SDL_Rect fadeDstRect = { fade_x, fade_y, fade_w, fade_h };
			SDL_RenderCopy(vid.renderer, fadeTexture, NULL, &fadeDstRect);
		}

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(moveTexture);
	if (fadeTexture) SDL_DestroyTexture(fadeTexture);
}

void PLAT_present() {
	SDL_RenderPresent(vid.renderer);
}

void setRectToAspectRatio(SDL_Rect* dst_rect) {
    int x = vid.blit->src_x;
    int y = vid.blit->src_y;
    int w = vid.blit->src_w;
    int h = vid.blit->src_h;

    if (vid.blit->aspect == 0) {
        w = vid.blit->src_w * vid.blit->scale;
        h = vid.blit->src_h * vid.blit->scale;
        dst_rect->x = (device_width - w) / 2 + screenx;
        dst_rect->y = (device_height - h) / 2 + screeny;
        dst_rect->w = w;
        dst_rect->h = h;
    } else if (vid.blit->aspect > 0) {
        if (should_rotate) {
            h = device_width;
            w = h * vid.blit->aspect;
            if (w > device_height) {
                w = device_height;
                h = w / vid.blit->aspect;
            }
        } else {
            h = device_height;
            w = h * vid.blit->aspect;
            if (w > device_width) {
                w = device_width;
                h = w / vid.blit->aspect;
            }
        }
        dst_rect->x = (device_width - w) / 2 + screenx;
        dst_rect->y = (device_height - h) / 2 + screeny;
        dst_rect->w = w;
        dst_rect->h = h;
    } else {
        dst_rect->x = screenx;
        dst_rect->y = screeny;
        dst_rect->w = should_rotate ? device_height : device_width;
        dst_rect->h = should_rotate ? device_width : device_height;
    }
}

void PLAT_flipHidden() {
	SDL_RenderClear(vid.renderer);
	resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
	SDL_UpdateTexture(vid.stream_layer1, NULL, vid.screen->pixels, vid.screen->pitch);
	SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
	//  SDL_RenderPresent(vid.renderer); // no present want to flip  hidden
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	// dont think we need this here tbh
	// SDL_RenderClear(vid.renderer);
    if (!vid.blit) {
        resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
        SDL_UpdateTexture(vid.stream_layer1, NULL, vid.screen->pixels, vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
        SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
        SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
		SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
		SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
        SDL_RenderPresent(vid.renderer);
        return;
    }

    SDL_UpdateTexture(vid.stream_layer1, NULL, vid.blit->src, vid.blit->src_p);

    SDL_Texture* target = vid.stream_layer1;
    int x = vid.blit->src_x;
    int y = vid.blit->src_y;
    int w = vid.blit->src_w;
    int h = vid.blit->src_h;

    SDL_Rect* src_rect = &(SDL_Rect){x, y, w, h};
    SDL_Rect* dst_rect = &(SDL_Rect){0, 0, device_width, device_height};

    setRectToAspectRatio(dst_rect);
	
    SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
    
    SDL_RenderPresent(vid.renderer);
    vid.blit = NULL;
}

static int frame_count = 0;
void runShaderPass(GLuint texture, GLuint shader_program, GLuint* fbo, GLuint* tex,
                   int x, int y, int width, int height, int input_tex_w, int input_tex_h, GLfloat texelSize[2],
                   GLenum filter, int layer, int screen_w, int screen_h) {

	static GLuint static_VAO = 0, static_VBO = 0;
	static GLuint last_program = 0;
	static GLfloat last_texelSize[2] = {-1.0f, -1.0f};

	glUseProgram(shader_program);
	if (static_VAO == 0 || shader_program != last_program) {
		if (static_VAO) glDeleteVertexArrays(1, &static_VAO);
		if (static_VBO) glDeleteBuffers(1, &static_VBO);

		glGenVertexArrays(1, &static_VAO);
		glGenBuffers(1, &static_VBO);
		glBindVertexArray(static_VAO);
		glBindBuffer(GL_ARRAY_BUFFER, static_VBO);

		float vertices[] = {
			//   x,     y,    u,    v,    z,    w
			-1.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // top-left
			-1.0f, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f,  // bottom-left
			 1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f,  // top-right
			 1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f   // bottom-right
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		GLint posAttrib = glGetAttribLocation(shader_program, "VertexCoord");
		if (posAttrib >= 0) {
			glEnableVertexAttribArray(posAttrib);
			glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		}
		GLint texAttrib = glGetAttribLocation(shader_program, "TexCoord");
		if (texAttrib >= 0) {
			glEnableVertexAttribArray(texAttrib);
			glVertexAttribPointer(texAttrib, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
		}
		last_program = shader_program;
	}
	
	GLint u_FrameDirection = glGetUniformLocation(shader_program, "FrameDirection");
	GLint u_FrameCount = glGetUniformLocation(shader_program, "FrameCount");
	GLint u_OutputSize = glGetUniformLocation(shader_program, "OutputSize");
	GLint u_TextureSize = glGetUniformLocation(shader_program, "TextureSize");
	GLint u_InputSize = glGetUniformLocation(shader_program, "InputSize");


	if (u_FrameDirection >= 0) glUniform1i(u_FrameDirection, 1);
	if (u_FrameCount >= 0) glUniform1i(u_FrameCount, frame_count);
	if (u_OutputSize >= 0) glUniform2f(u_OutputSize, screen_w, screen_h);
	if (u_TextureSize >= 0) glUniform2f(u_TextureSize, width, height); 
	if (u_InputSize >= 0) glUniform2f(u_InputSize, input_tex_w, input_tex_h); 

	GLint u_MVP = glGetUniformLocation(shader_program, "MVPMatrix");
	if (u_MVP >= 0) {
		float identity[16] = {
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			0,0,0,1
		};
		glUniformMatrix4fv(u_MVP, 1, GL_FALSE, identity);
	}

    if (fbo && tex) {
        if (*fbo == 0) glGenFramebuffers(1, fbo);
        if (*tex == 0) glGenTextures(1, tex);
		glBindTexture(GL_TEXTURE_2D, *tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("Framebuffer not complete!\n");
        }
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

	glBindVertexArray(static_VAO);
	
	if(layer==1) {
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_BLEND);
	}

	glBindTexture(GL_TEXTURE_2D, texture);
    glViewport(x, y, width, height);


	GLint texLocation = glGetUniformLocation(shader_program, "Texture");
    if (texLocation >= 0) {
        glUniform1i(texLocation, layer);  
    }

    GLint texelSizeLocation = glGetUniformLocation(shader_program, "texelSize");
	if (texelSizeLocation >= 0 && (shadersupdated || texelSize[0] != last_texelSize[0] || texelSize[1] != last_texelSize[1])) {
        glUniform2fv(texelSizeLocation, 1, texelSize);
        last_texelSize[0] = texelSize[0];
        last_texelSize[1] = texelSize[1];
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void PLAT_GL_Swap() {
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_Rect dst_rect = {0, 0, device_width, device_height};
    setRectToAspectRatio(&dst_rect);

    if (!vid.blit->src) {
        printf("Error: Texture data (vid.blit->src) is NULL\n");
        return;
    }

    SDL_GL_MakeCurrent(vid.window, vid.gl_context);

    static GLuint overlay_tex = 0;
    static int overlayload = 0;
	if(overlayUpdated) {
		overlay_tex=0;
		overlayload=0;
		overlayUpdated = 0;
	}
	static int overlay_w = 0;
	static int overlay_h = 0;
    if (!overlay_tex && !overlayload && overlay_path) {
        SDL_Surface* tmp = IMG_Load(overlay_path);
        if (tmp) {
            SDL_Surface* rgba = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
            glGenTextures(1, &overlay_tex);
            glBindTexture(GL_TEXTURE_2D, overlay_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
			overlay_w = rgba->w;
			overlay_h = rgba->h;
            SDL_FreeSurface(tmp);
            SDL_FreeSurface(rgba);
            LOG_info("overlay loaded");
        } 
		overlayload = 1;
    }

    static GLuint src_texture = 0;
    static GLuint initial_texture = 0;
    static GLuint fbo = 0;
    static GLuint pass_textures[3] = {0};

    static int src_w_last = 0, src_h_last = 0;
 
    if (!src_texture) glGenTextures(1, &src_texture);
    glBindTexture(GL_TEXTURE_2D, src_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (vid.blit->src_w != src_w_last || vid.blit->src_h != src_h_last) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.blit->src_w, vid.blit->src_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        src_w_last = vid.blit->src_w;
        src_h_last = vid.blit->src_h;
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid.blit->src_w, vid.blit->src_h, GL_RGBA, GL_UNSIGNED_BYTE, vid.blit->src);

    if (!fbo) glGenFramebuffers(1, &fbo);
 
    GLfloat texelSizeSource[2] = {1.0f / vid.blit->src_w, 1.0f / vid.blit->src_h};

	if (!initial_texture) glGenTextures(1, &initial_texture);
	runShaderPass(src_texture, g_shader_color, &fbo, &initial_texture, 0, 0,
		vid.blit->src_w, vid.blit->src_h, vid.blit->src_w, vid.blit->src_h,
				  texelSizeSource, GL_NEAREST, 0,dst_rect.w,dst_rect.h);

	static int last_w=0;
	static int last_h=0;
	static int texture_initialized[3] = {0};
	if(shadersupdated) {
		last_w = 0;
		last_h = 0;
	}
	for(int i=0;i<nrofshaders;i++) {
		if (!pass_textures[i]) glGenTextures(1, &pass_textures[i]);
        glBindTexture(GL_TEXTURE_2D, pass_textures[i]);
		
		int src_w = i == 0 ? vid.blit->src_w:last_w;
    	int src_h = i == 0 ? vid.blit->src_h:last_h;
		int dst_w = vid.blit->src_w * shaders[i]->scale;
    	int dst_h = vid.blit->src_h * shaders[i]->scale;
		if(shaders[i]->scale == 9) {
			dst_w = dst_rect.w;
			dst_h = dst_rect.h;
		}
		if (!texture_initialized[i] || dst_w != last_w || dst_h != last_h) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dst_w, dst_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			texture_initialized[i] = 1;
		}
		last_w = dst_w;
		last_h = dst_h;

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass_textures[i], 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("Framebuffer not complete in pass %d!\n", i);
		}
		// LOG_info("shader pass: %i\n",i);
		// LOG_info("src_w: %i\n",src_w);
		// LOG_info("dst_w: %i\n",dst_w);
		// LOG_info("src_h: %i\n",src_h);
		// LOG_info("dst_h: %i\n",dst_h);

		GLfloat texelPass[2] = {1.0f / src_w, 1.0f / src_h};
        runShaderPass(i==0?initial_texture:pass_textures[i-1], shaders[i]->shader_p, &fbo, &pass_textures[i], 0, 0,
			 dst_w, dst_h,src_w, src_h,
			texelPass, shaders[i]->filter , 0,dst_rect.w,dst_rect.h);
	}


    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// LOG_info("downscaling from: %i\n",last_w);
	GLfloat texelSizeOutput[2] = {1.0f / last_w, 1.0f / last_h};
    runShaderPass(pass_textures[nrofshaders-1], g_shader_default, NULL, NULL,
                  dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
                  last_w, last_h, texelSizeOutput, GL_NEAREST, 0,dst_rect.w,dst_rect.h);

    if (overlay_tex) {
        runShaderPass(overlay_tex, g_shader_overlay, NULL, NULL,
                      0, 0, device_width, device_height,
					  overlay_w, overlay_h, texelSizeOutput, GL_NEAREST, 1,dst_rect.w,dst_rect.h);
    }
    SDL_GL_SwapWindow(vid.window);
    shadersupdated = 0;
    frame_count++;
}

///////////////////////////////

// TODO: 
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 1;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);
}

void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	*is_charging = 1;
	*charge = 100;
}

void PLAT_enableBacklight(int enable) {
	// buh
}

void PLAT_powerOff(void) {
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// buh
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "macOS";
}

void PLAT_getOsVersionInfo(char *output_str, size_t max_len)
{
	sprintf(output_str, "%s", "1.2.3");
}

int PLAT_isOnline(void) {
	return online;
}

/////////////////////////////////
// Remove, just for debug


#define MAX_LINE_LENGTH 200
#define ZONE_PATH "/var/db/timezone/zoneinfo"
#define ZONE_TAB_PATH ZONE_PATH "/zone.tab"

static char cached_timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
static int cached_tz_count = -1;

int compare_timezones(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

void PLAT_initTimezones() {
    if (cached_tz_count != -1) { // Already initialized
        return;
    }
    
    FILE *file = fopen(ZONE_TAB_PATH, "r");
    if (!file) {
        LOG_info("Error opening file %s\n", ZONE_TAB_PATH);
        return;
    }
    
    char line[MAX_LINE_LENGTH];
    cached_tz_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // Skip comment lines
        if (line[0] == '#' || strlen(line) < 3) {
            continue;
        }
        
        char *token = strtok(line, "\t"); // Skip country code
        if (!token) continue;
        
        token = strtok(NULL, "\t"); // Skip latitude/longitude
        if (!token) continue;
        
        token = strtok(NULL, "\t\n"); // Extract timezone
        if (!token) continue;
        
        // Check for duplicates before adding
        int duplicate = 0;
        for (int i = 0; i < cached_tz_count; i++) {
            if (strcmp(cached_timezones[i], token) == 0) {
                duplicate = 1;
                break;
            }
        }
        
        if (!duplicate && cached_tz_count < MAX_TIMEZONES) {
            strncpy(cached_timezones[cached_tz_count], token, MAX_TZ_LENGTH - 1);
            cached_timezones[cached_tz_count][MAX_TZ_LENGTH - 1] = '\0'; // Ensure null-termination
            cached_tz_count++;
        }
    }
    
    fclose(file);
    
    // Sort the list alphabetically
    qsort(cached_timezones, cached_tz_count, MAX_TZ_LENGTH, compare_timezones);
}

void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count) {
    if (cached_tz_count == -1) {
        LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        *tz_count = 0;
        return;
    }
    
    memcpy(timezones, cached_timezones, sizeof(cached_timezones));
    *tz_count = cached_tz_count;
}

char *PLAT_getCurrentTimezone() {
	// call readlink -f /tmp/localtime to get the current timezone path, and
	// then remove /usr/share/zoneinfo/ from the beginning of the path to get the timezone name.
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return NULL;
	}
	if (readlink("/etc/localtime", tz_path, 256) == -1) {
		free(tz_path);
		return NULL;
	}
	tz_path[255] = '\0'; // Ensure null-termination
	char *tz_name = strstr(tz_path, ZONE_PATH "/");
	if (tz_name) {
		tz_name += strlen(ZONE_PATH "/");
		return strdup(tz_name);
	} else {
		return strdup(tz_path);
	}
}

void PLAT_setCurrentTimezone(const char* tz) {
	return;
	if (cached_tz_count == -1)
	{
		LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        return;
	}

	// tzset()

	// tz will be in format Asia/Shanghai
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return;
	}
	snprintf(tz_path, 256, ZONE_PATH "/%s", tz);
	if (unlink("/tmp/localtime") == -1) {
		LOG_error("Failed to remove existing symlink: %s\n", strerror(errno));
	}
	if (symlink(tz_path, "/tmp/localtime") == -1) {
		LOG_error("Failed to set timezone: %s\n", strerror(errno));
	}
	free(tz_path);
}

/////////////////////

 void PLAT_wifiInit() {}
 bool PLAT_hasWifi() { return true; }
 bool PLAT_wifiEnabled() { return true; }
 void PLAT_wifiEnable(bool on) {}

 int PLAT_wifiScan(struct WIFI_network *networks, int max) {
	for (int i = 0; i < 5; i++) {
		struct WIFI_network *network = &networks[i];

		sprintf(network->ssid, "Network%d", i);
		strcpy(network->bssid, "01:01:01:01:01:01");
		network->rssi = (70 / 5) * (i + 1);
		network->freq = 2400;
		network->security = i % 2 ? SECURITY_WPA2_PSK : SECURITY_WEP;
	}
	return 5;
 }
 bool PLAT_wifiConnected() { return true; }
 int PLAT_wifiConnection(struct WIFI_connection *connection_info) {
	connection_info->freq = 2400;
	strcpy(connection_info->ip, "127.0.0.1");
	strcpy(connection_info->ssid, "Network1");
	return 0;
 }
 bool PLAT_wifiHasCredentials(char *ssid, WifiSecurityType sec) { return false; }
 void PLAT_wifiForget(char *ssid, WifiSecurityType sec) {}
 void PLAT_wifiConnect(char *ssid, WifiSecurityType sec) {}
 void PLAT_wifiConnectPass(const char *ssid, WifiSecurityType sec, const char* pass) {}
 void PLAT_wifiDisconnect() {}