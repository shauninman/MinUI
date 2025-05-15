// tg5040
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"
#include <time.h>
#include <pthread.h>

#include "opengl.h"

#include <dirent.h>

static int finalScaleFilter=GL_LINEAR;
static int reloadShaderTextures = 1;

// shader stuff

typedef struct Shader {
	int srcw;
	int srch;
	int texw;
	int texh;
	int filter;
	GLuint shader_p;
	int scale;
	int srctype;
	int scaletype;
	char *filename;
	GLuint texture;
	int updated;
	GLint u_FrameDirection;
	GLint u_FrameCount;
	GLint u_OutputSize;
	GLint u_TextureSize;
	GLint u_InputSize;
	GLint OrigInputSize;
	GLint texLocation;
	GLint texelSizeLocation;
	ShaderParam *pragmas;  // Dynamic array of parsed pragma parameters
	int num_pragmas;       // Count of valid pragma parameters

} Shader;

GLuint g_shader_default = 0;
GLuint g_shader_overlay = 0;
GLuint g_noshader = 0;

Shader* shaders[MAXSHADERS] = {
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR, .scaletype = 1, .srctype = 0, .filename ="stock.glsl", .texture = 0, .updated = 1 },
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR, .scaletype = 1, .srctype = 0, .filename ="stock.glsl", .texture = 0, .updated = 1 },
    &(Shader){ .shader_p = 0, .scale = 1, .filter = GL_LINEAR, .scaletype = 1, .srctype = 0, .filename ="stock.glsl", .texture = 0, .updated = 1 },
};

static int nrofshaders = 0; // choose between 1 and 3 pipelines, > pipelines = more cpu usage, but more shader options and shader upscaling stuff
///////////////////////////////

int is_brick = 0;
static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	
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
static uint32_t SDL_transparentBlack = 0;

#define OVERLAYS_FOLDER SDCARD_PATH "/Overlays"

static char* overlay_path = NULL;


#define MAX_SHADERLINE_LENGTH 512
int extractPragmaParameters(const char *shaderSource, ShaderParam *params, int maxParams) {
    const char *pragmaPrefix = "#pragma parameter";
    char line[MAX_SHADERLINE_LENGTH];
    int paramCount = 0;

    const char *currentPos = shaderSource;

    while (*currentPos && paramCount < maxParams) {
        int i = 0;

        // Read a line
        while (*currentPos && *currentPos != '\n' && i < MAX_SHADERLINE_LENGTH - 1) {
            line[i++] = *currentPos++;
        }
        line[i] = '\0';
        if (*currentPos == '\n') currentPos++;

        // Check if it's a #pragma parameter line
        if (strncmp(line, pragmaPrefix, strlen(pragmaPrefix)) == 0) {
            const char *start = line + strlen(pragmaPrefix);
            while (*start == ' ') start++;

            ShaderParam *p = &params[paramCount];

            // Try to parse
            if (sscanf(start, "%127s \"%127[^\"]\" %f %f %f %f",
                       p->name, p->label, &p->def, &p->min, &p->max, &p->step) == 6) {
                paramCount++;
            } else {
                fprintf(stderr, "Failed to parse line:\n%s\n", line);
            }
        }
    }

    return paramCount; // number of parameters found
}

GLuint link_program(GLuint vertex_shader, GLuint fragment_shader, const char* cache_key) {
    char cache_path[512];
    snprintf(cache_path, sizeof(cache_path), "/mnt/SDCARD/.shadercache/%s.bin", cache_key);

    GLuint program = glCreateProgram();
    GLint success;

    // Try to load cached binary first
    FILE *f = fopen(cache_path, "rb");
    if (f) {
        GLint binaryFormat;
        fread(&binaryFormat, sizeof(GLint), 1, f);
        fseek(f, 0, SEEK_END);
        size_t length = ftell(f) - sizeof(GLint);
        fseek(f, sizeof(GLint), SEEK_SET);
        void *binary = malloc(length);
        fread(binary, 1, length, f);
        fclose(f);

        glProgramBinary(program, binaryFormat, binary, length);
        free(binary);

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success) {
            LOG_info("Loaded shader program from cache: %s\n", cache_key);
            return program;
        } else {
            LOG_info("Cache load failed, falling back to compile.\n");
            glDeleteProgram(program);
            program = glCreateProgram();
        }
    }

    // Compile and link if cache failed
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        char* log = (char*)malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        printf("Program link error: %s\n", log);
        free(log);
        return program;
    }

    GLint binaryLength;
    GLenum binaryFormat;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
    void* binary = malloc(binaryLength);
    glGetProgramBinary(program, binaryLength, NULL, &binaryFormat, binary);

    mkdir("/mnt/SDCARD/.shadercache", 0755); 
    f = fopen(cache_path, "wb");
    if (f) {
        fwrite(&binaryFormat, sizeof(GLenum), 1, f);
        fwrite(binary, 1, binaryLength, f);
        fclose(f);
        LOG_info("Saved shader program to cache: %s\n", cache_key);
    }
    free(binary);

    LOG_info("Program linked and cached\n");
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
            "#endif\n"
			"#define PARAMETER_UNIFORM\n";
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

void PLAT_initShaders() {
	SDL_GL_MakeCurrent(vid.window, vid.gl_context);
	glViewport(0, 0, device_width, device_height);
	
	GLuint vertex;
	GLuint fragment;

	vertex = load_shader_from_file(GL_VERTEX_SHADER, "default.glsl",SYSSHADERS_FOLDER);
	fragment = load_shader_from_file(GL_FRAGMENT_SHADER, "default.glsl",SYSSHADERS_FOLDER);
	g_shader_default = link_program(vertex, fragment,"defaultv2.glsl");

	vertex = load_shader_from_file(GL_VERTEX_SHADER, "overlay.glsl",SYSSHADERS_FOLDER);
	fragment = load_shader_from_file(GL_FRAGMENT_SHADER, "overlay.glsl",SYSSHADERS_FOLDER);
	g_shader_overlay = link_program(vertex, fragment,"overlay.glsl");

	vertex = load_shader_from_file(GL_VERTEX_SHADER, "noshader.glsl",SYSSHADERS_FOLDER);
	fragment = load_shader_from_file(GL_FRAGMENT_SHADER, "noshader.glsl",SYSSHADERS_FOLDER);
	g_noshader = link_program(vertex, fragment,"noshader.glsl");
	
	LOG_info("default shaders loaded, %i\n\n",g_shader_default);
}


SDL_Surface* PLAT_initVideo(void) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	// LOG_info("DEVICE: %s is_brick: %i\n", device, is_brick);
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
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

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"1");

	vid.gl_context = SDL_GL_CreateContext(vid.window);
	SDL_GL_MakeCurrent(vid.window, vid.gl_context);
	glViewport(0, 0, w, h);

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

	PWR_disablePowerOff();
	
	SDL_transparentBlack = SDL_MapRGBA(vid.screen->format, 0, 0, 0, 0);
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}

void PLAT_resetShaders() {

}

char* PLAT_findFileInDir(const char *directory, const char *filename) {
    char *filename_copy = strdup(filename);
    if (!filename_copy) {
        perror("strdup");
        return NULL;
    }

    // Strip extension from filename
    char *dot_pos = strrchr(filename_copy, '.');
    if (dot_pos) {
        *dot_pos = '\0';
    }

    DIR *dir = opendir(directory);
    if (!dir) {
        perror("opendir");
        free(filename_copy);
        return NULL;
    }

    struct dirent *entry;
    char *full_path = NULL;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, filename_copy) == entry->d_name) {
            full_path = (char *)malloc(strlen(directory) + strlen(entry->d_name) + 2); // +1 for slash, +1 for '\0'
            if (!full_path) {
                perror("malloc");
                closedir(dir);
                free(filename_copy);
                return NULL;
            }

            snprintf(full_path, strlen(directory) + strlen(entry->d_name) + 2, "%s/%s", directory, entry->d_name);
            closedir(dir);
            free(filename_copy);
            return full_path;
        }
    }

    closedir(dir);
    free(filename_copy);
    return NULL;
}

#define MAX_SHADER_PRAGMAS 32
void loadShaderPragmas(Shader *shader, const char *shaderSource) {
	shader->pragmas = calloc(MAX_SHADER_PRAGMAS, sizeof(ShaderParam));
	if (!shader->pragmas) {
		fprintf(stderr, "Out of memory allocating pragmas for %s\n", shader->filename);
		return;
	}
	shader->num_pragmas = extractPragmaParameters(shaderSource, shader->pragmas, MAX_SHADER_PRAGMAS);
}

ShaderParam* PLAT_getShaderPragmas(int i) {
    return shaders[i]->pragmas;
}

void PLAT_updateShader(int i, const char *filename, int *scale, int *filter, int *scaletype, int *srctype) {

    if (i < 0 || i >= nrofshaders) {
        return;
    }
    Shader* shader = shaders[i];

    if (filename != NULL) {
        SDL_GL_MakeCurrent(vid.window, vid.gl_context);
        LOG_info("loading shader \n");

		char filepath[512];
		snprintf(filepath, sizeof(filepath), SHADERS_FOLDER "/glsl/%s",filename);
		const char *shaderSource  = load_shader_source(filepath);
		loadShaderPragmas(shader,shaderSource);

		GLuint vertex_shader1 = load_shader_from_file(GL_VERTEX_SHADER, filename,SHADERS_FOLDER "/glsl");
		GLuint fragment_shader1 = load_shader_from_file(GL_FRAGMENT_SHADER, filename,SHADERS_FOLDER "/glsl");
			
        // Link the shader program
		if (shader->shader_p != 0) {
			LOG_info("Deleting previous shader %i\n",shader->shader_p);
			glDeleteProgram(shader->shader_p);
		}
        shader->shader_p = link_program(vertex_shader1, fragment_shader1,filename);
        
		shader->u_FrameDirection = glGetUniformLocation( shader->shader_p, "FrameDirection");
		shader->u_FrameCount = glGetUniformLocation( shader->shader_p, "FrameCount");
		shader->u_OutputSize = glGetUniformLocation( shader->shader_p, "OutputSize");
		shader->u_TextureSize = glGetUniformLocation( shader->shader_p, "TextureSize");
		shader->u_InputSize = glGetUniformLocation( shader->shader_p, "InputSize");
		shader->OrigInputSize = glGetUniformLocation( shader->shader_p, "OrigInputSize");
		shader->texLocation = glGetUniformLocation(shader->shader_p, "Texture");
		shader->texelSizeLocation = glGetUniformLocation(shader->shader_p, "texelSize");
		for (int i = 0; i < shader->num_pragmas; ++i) {
			shader->pragmas[i].uniformLocation = glGetUniformLocation(shader->shader_p, shader->pragmas[i].name);
			shader->pragmas[i].value = shader->pragmas[i].def;

			printf("Param: %s = %f (min: %f, max: %f, step: %f)\n",
				shader->pragmas[i].name,
				shader->pragmas[i].def,
				shader->pragmas[i].min,
				shader->pragmas[i].max,
				shader->pragmas[i].step);
		}

        if (shader->shader_p == 0) {
            LOG_info("Shader linking failed for %s\n", filename);
        }

        GLint success = 0;
        glGetProgramiv(shader->shader_p, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shader->shader_p, 512, NULL, infoLog);
            LOG_info("Shader Program Linking Failed: %s\n", infoLog);
        } else {
			LOG_info("Shader Program Linking Success %s shader ID is %i\n", filename,shader->shader_p);
		}
		shader->filename = strdup(filename);
    }
    if (scale != NULL) {
        shader->scale = *scale +1;
		reloadShaderTextures = 1;
    }
    if (scaletype != NULL) {
        shader->scaletype = *scaletype;
    }
    if (srctype != NULL) {
        shader->srctype = *srctype;
    }
    if (filter != NULL) {
        shader->filter = (*filter == 1) ? GL_LINEAR : GL_NEAREST;
		reloadShaderTextures = 1;
    }
	shader->updated = 1;

}

void PLAT_setShaders(int nr) {
	LOG_info("set nr of shaders to %i\n",nr);
	nrofshaders = nr;
	reloadShaderTextures = 1;
}


uint32_t PLAT_get_dominant_color() {
    if (!vid.screen) {
        fprintf(stderr, "Error: vid.screen is NULL.\n");
        return 0;
    }

    if (vid.screen->format->format != SDL_PIXELFORMAT_RGBA8888) {
        fprintf(stderr, "Error: Surface is not in RGBA8888 format.\n");
        return 0;
    }

    uint32_t *pixels = (uint32_t *)vid.screen->pixels;
    if (!pixels) {
        fprintf(stderr, "Error: Unable to access pixel data.\n");
        return 0;
    }

    int width = vid.screen->w;
    int height = vid.screen->h;
    int pixel_count = width * height;

    // Use dynamic memory allocation for the histogram
    uint32_t *color_histogram = (uint32_t *)calloc(256 * 256 * 256, sizeof(uint32_t));
    if (!color_histogram) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 0;
    }

    for (int i = 0; i < pixel_count; i++) {
        uint32_t pixel = pixels[i];

        // Extract R, G, B from RGBA8888
        uint8_t r = (pixel >> 24) & 0xFF;
        uint8_t g = (pixel >> 16) & 0xFF;
        uint8_t b = (pixel >> 8) & 0xFF;

        uint32_t rgb = (r << 16) | (g << 8) | b;
        color_histogram[rgb]++;
    }

    // Find the most frequent color
    uint32_t dominant_color = 0;
    uint32_t max_count = 0;
    for (int i = 0; i < 256 * 256 * 256; i++) {
        if (color_histogram[i] > max_count) {
            max_count = color_histogram[i];
            dominant_color = i;
        }
    }

    free(color_histogram);

    // Return as RGBA8888 with full alpha
    return (dominant_color << 8) | 0xFF;
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


	glFinish();
	SDL_GL_DeleteContext(vid.gl_context);
	SDL_FreeSurface(vid.screen);

	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	if (vid.overlay) SDL_DestroyTexture(vid.overlay);
	if (vid.target_layer3) SDL_DestroyTexture(vid.target_layer3);
	if (vid.target_layer1) SDL_DestroyTexture(vid.target_layer1);
	if (vid.target_layer2) SDL_DestroyTexture(vid.target_layer2);
	if (vid.target_layer4) SDL_DestroyTexture(vid.target_layer4);
	if (overlay_path) free(overlay_path);
	SDL_DestroyTexture(vid.stream_layer1);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// SDL_FillRect(screen, NULL, 0); // TODO: revisit
	SDL_FillRect(screen, NULL, SDL_transparentBlack);
}
void PLAT_clearAll(void) {
	// ok honestely mixing SDL and OpenGL is really bad, but hey it works just got to sometimes clear gpu stuff and pull context back to SDL 
	// so yeah clear all layers and pull a flip() to make it switch back to SDL before clearing
	PLAT_clearLayers(0);
	PLAT_flip(vid.screen,0);

	// then do normal SDL clearing stuff
	PLAT_clearVideo(vid.screen); 
	SDL_RenderClear(vid.renderer);

}

void PLAT_setVsync(int vsync) {
	
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native, is this true?
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	// else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient for 640x480)
	else hard_scale = 4;

	// LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_DestroyTexture(vid.stream_layer1);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	// SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		// SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}
	

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;

	reloadShaderTextures = 1;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	if(sharpness==1) {
		finalScaleFilter=GL_LINEAR;
	} 
	else {
		finalScaleFilter = GL_NEAREST;
	}
	reloadShaderTextures = 1;
}

static struct FX_Context {
	int scale;
	int type;
	int color;
	int next_scale;
	int next_type;
	int next_color;
	int live_type;
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
	.color = 0,
	.next_color = 0,
};
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Extract the red component (5 bits)
    uint8_t red = (rgb565 >> 11) & 0x1F;
    // Extract the green component (6 bits)
    uint8_t green = (rgb565 >> 5) & 0x3F;
    // Extract the blue component (5 bits)
    uint8_t blue = rgb565 & 0x1F;

    // Scale the values to 8-bit range
    *r = (red << 3) | (red >> 2);
    *g = (green << 2) | (green >> 4);
    *b = (blue << 3) | (blue >> 2);
}
static char* effect_path;
static int effectUpdated = 0;
static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type && effect.next_color==effect.color) return; // unchanged
	
	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;
	
	if (effect.type==EFFECT_NONE) return; // disabled
	if (effect.type==effect.live_type && effect.scale==live_scale && effect.color==live_color) return; // already loaded
	
	int opacity = 128; // 1 - 1/2 = 50%
	if (effect.type==EFFECT_LINE) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/line-2.png";
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/line-3.png";
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/line-4.png";
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/line-5.png";
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/line-6.png";
		}
		else {
			effect_path = RES_PATH "/line-8.png";
		}
	}
	else if (effect.type==EFFECT_GRID) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/grid-2.png";
			opacity = 64; // 1 - 3/4 = 25%
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/grid-3.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/grid-4.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/grid-5.png";
			opacity = 160; // 1 - 9/25 = ~64%
			// opacity = 96; // TODO: tmp, for white grid
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/grid-6.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<11) {
			effect_path = RES_PATH "/grid-8.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else {
			effect_path = RES_PATH "/grid-11.png";
			opacity = 136; // 1 - 57/121 = ~52%
		}
	}
	effectUpdated = 1;
	
}
int screenx = 0;
int screeny = 0;
void PLAT_setOffsetX(int x) {
    if (x < 0 || x > 128) return;
    screenx = x - 64;
	LOG_info("screenx: %i %i\n",screenx,x);
}
void PLAT_setOffsetY(int y) {
    if (y < 0 || y > 128) return;
    screeny = y - 64; 
	LOG_info("screeny: %i %i\n",screeny,y);
}
static int overlayUpdated=0;
void PLAT_setOverlay(const char* filename, const char* tag) {
    if (vid.overlay) {
        SDL_DestroyTexture(vid.overlay);
        vid.overlay = NULL;
    }
	overlay_path = NULL;
	overlayUpdated=1;

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
		// blit to 0 for normal draw
		vid.blit = 0;
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
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("getScaler for scale: %i\n", renderer->scale);
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
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

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

void PLAT_clearShaders() {
	// this funciton was empty so am abusing it for now for this, later need to make a seperate function for it
	// set blit to 0 maybe should be seperate function later
	vid.blit = NULL;
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
    if (vid.sharpness == SHARPNESS_CRISP) {
		
        SDL_SetRenderTarget(vid.renderer, vid.target);
        SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
        SDL_SetRenderTarget(vid.renderer, NULL);
        x *= hard_scale;
        y *= hard_scale;
        w *= hard_scale;
        h *= hard_scale;
        target = vid.target;
    }

    SDL_Rect* src_rect = &(SDL_Rect){x, y, w, h};
    SDL_Rect* dst_rect = &(SDL_Rect){0, 0, device_width, device_height};

    setRectToAspectRatio(dst_rect);
	
    SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);

    SDL_RenderPresent(vid.renderer);
    vid.blit = NULL;
}

static int frame_count = 0;
void runShaderPass(GLuint src_texture, GLuint shader_program, GLuint* target_texture,
                   int x, int y, int dst_width, int dst_height, Shader* shader, int alpha, int filter) {

	static GLuint static_VAO = 0, static_VBO = 0;
	static GLuint last_program = 0;
	static GLfloat last_texelSize[2] = {-1.0f, -1.0f};
	static GLfloat texelSize[2] = {-1.0f, -1.0f};
	static GLuint fbo = 0;
	texelSize[0] = 1.0f / shader->texw;
	texelSize[1] = 1.0f / shader->texh;


	if (shader_program != last_program)
    	glUseProgram(shader_program);

	if (static_VAO == 0) {
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
	}
	

	if (shader_program != last_program) {
		GLint posAttrib = glGetAttribLocation(shader_program, "VertexCoord");
		if (posAttrib >= 0) {
			
			glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(posAttrib);
		}
		GLint texAttrib = glGetAttribLocation(shader_program, "TexCoord");
		if (texAttrib >= 0) {
			
			glVertexAttribPointer(texAttrib, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
			glEnableVertexAttribArray(texAttrib);
		}



		if (shader->u_FrameDirection >= 0) glUniform1i(shader->u_FrameDirection, 1);
		if (shader->u_FrameCount >= 0) glUniform1i(shader->u_FrameCount, frame_count);
		if (shader->u_OutputSize >= 0) glUniform2f(shader->u_OutputSize, dst_width, dst_height);
		if (shader->u_TextureSize >= 0) glUniform2f(shader->u_TextureSize, shader->texw, shader->texh); 
		if (shader->OrigInputSize >= 0) glUniform2f(shader->OrigInputSize, shader->srcw, shader->srch); 
		if (shader->u_InputSize >= 0) glUniform2f(shader->u_InputSize, shader->srcw, shader->srch); 
		for (int i = 0; i < shader->num_pragmas; ++i) {
			glUniform1f(shader->pragmas[i].uniformLocation, shader->pragmas[i].value);
		}

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
		glBindVertexArray(static_VAO);
	}
	static GLuint lastfbo = -1;
	if (target_texture) {
		if (*target_texture==0 || shader->updated || reloadShaderTextures) { 
			
			// if(target_texture) {
			// 	glDeleteTextures(1,target_texture);
			// }
			if(*target_texture==0)
				glGenTextures(1, target_texture);
			glBindTexture(GL_TEXTURE_2D, *target_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dst_width, dst_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			shader->updated = 0;
		}
		if (fbo == 0) {
			glGenFramebuffers(1, &fbo);
		}
		
		if (lastfbo == 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			
		}
		lastfbo = fbo;
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *target_texture, 0);
		
    } else {
		// things like overlays and stuff we don't need to write to another texture so they can be directly written to screen framebuffer
		if (lastfbo != 0) {
        	glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		lastfbo = 0;
    }

	if(alpha==1) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}

	static GLuint last_bound_texture = 0;
	if (src_texture != last_bound_texture) {
		glBindTexture(GL_TEXTURE_2D, src_texture);
		last_bound_texture = src_texture;
	}
	glViewport(x, y, dst_width, dst_height);

	
	if (shader->texLocation >= 0) glUniform1i(shader->texLocation, 0);  
	

	if (shader->texelSizeLocation >= 0 && (shader->updated || texelSize[0] != last_texelSize[0] || texelSize[1] != last_texelSize[1])) {
		glUniform2fv(shader->texelSizeLocation, 1, texelSize);
		last_texelSize[0] = texelSize[0];
		last_texelSize[1] = texelSize[1];
	}
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	last_program = shader_program;
}

typedef struct {
    SDL_Surface* loaded_effect;
    SDL_Surface* loaded_overlay;
    int effect_ready;
    int overlay_ready;
} FramePreparation;

static FramePreparation frame_prep = {0};

int prepareFrameThread(void *data) {
    while (1) {
		updateEffect();

        if (effectUpdated) {
			LOG_info("effect updated %s\n",effect_path);
			if(effect_path) {
				SDL_Surface* tmp = IMG_Load(effect_path);
				if (tmp) {
					frame_prep.loaded_effect = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
					SDL_FreeSurface(tmp);
				} else {
					frame_prep.loaded_effect = 0;
				}
			} else {
				frame_prep.loaded_effect = 0;
			}
			effectUpdated = 0;
			frame_prep.effect_ready = 1; 
        }
		if(effect.type == EFFECT_NONE && frame_prep.loaded_effect !=0) {
			frame_prep.loaded_effect = 0;
			frame_prep.effect_ready = 1;
	
		}

        if (overlayUpdated) {

			LOG_info("overlay updated\n");
			if(overlay_path) {
				SDL_Surface* tmp = IMG_Load(overlay_path);
				if (tmp) {
					frame_prep.loaded_overlay = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
					SDL_FreeSurface(tmp);
				} else {
					frame_prep.loaded_overlay = 0;
				}
			} else {
				frame_prep.loaded_overlay = 0;
			}
			frame_prep.overlay_ready = 1;
			overlayUpdated=0;
        }

        SDL_Delay(120); 
    }
    return 0;
}

static SDL_Thread *prepare_thread = NULL;

void PLAT_GL_Swap() {

	if (prepare_thread == NULL) {
        prepare_thread = SDL_CreateThread(prepareFrameThread, "PrepareFrameThread", NULL);

        if (prepare_thread == NULL) {
            printf("Error creating background thread: %s\n", SDL_GetError());
            return; 
        }
    }

    static int lastframecount = 0;
    if (reloadShaderTextures) lastframecount = frame_count;
    if (frame_count < lastframecount + 3)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_Rect dst_rect = {0, 0, device_width, device_height};
    setRectToAspectRatio(&dst_rect);

    if (!vid.blit->src) {
        return;
    }

	SDL_GL_MakeCurrent(vid.window, vid.gl_context);

    static GLuint effect_tex = 0;
    static int effect_w = 0, effect_h = 0;
    static GLuint overlay_tex = 0;
    static int overlay_w = 0, overlay_h = 0;
    static int overlayload = 0;


	 if (frame_prep.effect_ready) {
		if(frame_prep.loaded_effect) {
			if(!effect_tex) glGenTextures(1, &effect_tex);
			glBindTexture(GL_TEXTURE_2D, effect_tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_prep.loaded_effect->w, frame_prep.loaded_effect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_prep.loaded_effect->pixels);
			effect_w = frame_prep.loaded_effect->w;
			effect_h = frame_prep.loaded_effect->h;
		} else {
			if (effect_tex) {
				glDeleteTextures(1, &effect_tex);
			}
			effect_tex = 0;
		}
        frame_prep.effect_ready = 0; 
    }

    if (frame_prep.overlay_ready) {
		if(frame_prep.loaded_overlay) {
			if(!overlay_tex) glGenTextures(1, &overlay_tex);
			glBindTexture(GL_TEXTURE_2D, overlay_tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_prep.loaded_overlay->w, frame_prep.loaded_overlay->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_prep.loaded_overlay->pixels);
			overlay_w = frame_prep.loaded_overlay->w;
			overlay_h = frame_prep.loaded_overlay->h;
		
		} else {
			if (overlay_tex) {
				glDeleteTextures(1, &overlay_tex);
			}
			overlay_tex = 0;
		}
        frame_prep.overlay_ready = 0; 
    }
	
    static GLuint src_texture = 0;
    static int src_w_last = 0, src_h_last = 0;
    static int last_w = 0, last_h = 0;

    if (!src_texture || reloadShaderTextures) {
        // if (src_texture) {
        //     glDeleteTextures(1, &src_texture);
        //     src_texture = 0;
        // }
		if (src_texture==0)
        	glGenTextures(1, &src_texture);
        glBindTexture(GL_TEXTURE_2D, src_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nrofshaders > 0 ? shaders[0]->filter : finalScaleFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nrofshaders > 0 ? shaders[0]->filter : finalScaleFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, src_texture);
    if (vid.blit->src_w != src_w_last || vid.blit->src_h != src_h_last || reloadShaderTextures) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.blit->src_w, vid.blit->src_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, vid.blit->src);
        src_w_last = vid.blit->src_w;
        src_h_last = vid.blit->src_h;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid.blit->src_w, vid.blit->src_h, GL_RGBA, GL_UNSIGNED_BYTE, vid.blit->src);
    }

    if (nrofshaders < 1) {
        runShaderPass(src_texture, g_shader_default, NULL, dst_rect.x, dst_rect.y,
            dst_rect.w, dst_rect.h,
            &(Shader){.srcw = vid.blit->src_w, .srch = vid.blit->src_h, .texw = vid.blit->src_w, .texh = vid.blit->src_h},
            0, GL_NONE);
    }

    last_w = vid.blit->src_w;
    last_h = vid.blit->src_h;

    for (int i = 0; i < nrofshaders; i++) {
        int src_w = last_w;
        int src_h = last_h;
        int dst_w = src_w * shaders[i]->scale;
        int dst_h = src_h * shaders[i]->scale;

        if (shaders[i]->scale == 9) {
            dst_w = dst_rect.w;
            dst_h = dst_rect.h;
        }

        if (reloadShaderTextures) {
            for (int j = i; j < nrofshaders; j++) {
                int real_input_w = (i == 0) ? vid.blit->src_w : last_w;
                int real_input_h = (i == 0) ? vid.blit->src_h : last_h;

                shaders[i]->srcw = shaders[i]->srctype == 0 ? vid.blit->src_w : shaders[i]->srctype == 2 ? dst_rect.w : real_input_w;
                shaders[i]->srch = shaders[i]->srctype == 0 ? vid.blit->src_h : shaders[i]->srctype == 2 ? dst_rect.h : real_input_h;
                shaders[i]->texw = shaders[i]->scaletype == 0 ? vid.blit->src_w : shaders[i]->scaletype == 2 ? dst_rect.w : real_input_w;
                shaders[i]->texh = shaders[i]->scaletype == 0 ? vid.blit->src_h : shaders[i]->scaletype == 2 ? dst_rect.h : real_input_h;
            }
        }

        static int shaderinfocount = 0;
        static int shaderinfoscreen = 0;
        if (shaderinfocount > 600 && shaderinfoscreen == i) {
            currentshaderpass = i + 1;
            currentshadertexw = shaders[i]->texw;
            currentshadertexh = shaders[i]->texh;
            currentshadersrcw = shaders[i]->srcw;
            currentshadersrch = shaders[i]->srch;
            currentshaderdstw = dst_w;
            currentshaderdsth = dst_h;
            shaderinfocount = 0;
            shaderinfoscreen++;
            if (shaderinfoscreen >= nrofshaders)
                shaderinfoscreen = 0;
        }
        shaderinfocount++;

        if (shaders[i]->shader_p) {
            runShaderPass(
                (i == 0) ? src_texture : shaders[i - 1]->texture,
                shaders[i]->shader_p,
                &shaders[i]->texture,
                0, 0, dst_w, dst_h,
                shaders[i],
                0,
                (i == nrofshaders - 1) ? finalScaleFilter : shaders[i + 1]->filter
            );
        } else {
            runShaderPass(
                (i == 0) ? src_texture : shaders[i - 1]->texture,
                g_noshader,
                &shaders[i]->texture,
                0, 0, dst_w, dst_h,
                shaders[i],
                0,
                (i == nrofshaders - 1) ? finalScaleFilter : shaders[i + 1]->filter
            );
        }

        last_w = dst_w;
        last_h = dst_h;
    }

    if (nrofshaders > 0) {
        runShaderPass(
            shaders[nrofshaders - 1]->texture,
            g_shader_default,
            NULL,
            dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
            &(Shader){.srcw = last_w, .srch = last_h, .texw = last_w, .texh = last_h},
            0, GL_NONE
        );
    }

    if (effect_tex) {
        runShaderPass(
            effect_tex,
            g_shader_overlay,
            NULL,
			dst_rect.x, dst_rect.y, effect_w, effect_h,
            &(Shader){.srcw = effect_w, .srch = effect_h, .texw = effect_w, .texh = effect_h},
            1, GL_NONE
        );
    }

    if (overlay_tex) {
        runShaderPass(
            overlay_tex,
            g_shader_overlay,
            NULL,
            0, 0, device_width, device_height,
            &(Shader){.srcw = vid.blit->src_w, .srch = vid.blit->src_h, .texw = overlay_w, .texh = overlay_h},
            1, GL_NONE
        );
    }

    SDL_GL_SwapWindow(vid.window);
    frame_count++;
    reloadShaderTextures = 0;
}



// tryin to some arm neon optimization for first time for flipping image upside down, they sit in platform cause not all have neon extensions
void PLAT_pixelFlipper(uint8_t* pixels, int width, int height) {
    const int rowBytes = width * 4;
    uint8_t* rowTop;
    uint8_t* rowBottom;

    for (int y = 0; y < height / 2; ++y) {
        rowTop = pixels + y * rowBytes;
        rowBottom = pixels + (height - 1 - y) * rowBytes;

        int x = 0;
        for (; x + 15 < rowBytes; x += 16) {
            uint8x16_t top = vld1q_u8(rowTop + x);
            uint8x16_t bottom = vld1q_u8(rowBottom + x);

            vst1q_u8(rowTop + x, bottom);
            vst1q_u8(rowBottom + x, top);
        }
        for (; x < rowBytes; ++x) {
            uint8_t temp = rowTop[x];
            rowTop[x] = rowBottom[x];
            rowBottom[x] = temp;
        }
    }
}

unsigned char* PLAT_GL_screenCapture(int* outWidth, int* outHeight) {
    glViewport(0, 0, device_width, device_height);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
	
    int width = viewport[2];
    int height = viewport[3];

    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;

    unsigned char* pixels = malloc(width * height * 4); // RGBA
    if (!pixels) return NULL;

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	PLAT_pixelFlipper(pixels, width, height);

    return pixels; // caller must free
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

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);

	// worry less about battery and more about the game you're playing
	     if (*charge>80) *charge = 100;
	else if (*charge>60) *charge =  80;
	else if (*charge>40) *charge =  60;
	else if (*charge>20) *charge =  40;
	else if (*charge>10) *charge =  20;
	else           		 *charge =  10;
}
void PLAT_getCPUTemp() {
	currentcputemp = getInt("/sys/devices/virtual/thermal/thermal_zone0/temp")/1000;

}
void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	*charge = getInt("/sys/class/power_supply/axp2202-battery/capacity");

	// // wifi status, just hooking into the regular PWR polling
	//char status[16];
	//getFile("/sys/class/net/wlan0/operstate", status,16);
	//online = prefixMatch("up", status);
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		if (is_brick) SetRawBrightness(8);
		SetBrightness(GetBrightness());
	}
	else {
		SetRawBrightness(0);
	}
}

void PLAT_powerOff(void) {
	if (CFG_getHaptics()) {
		VIB_singlePulse(VIB_bootStrength, VIB_bootDuration_ms);
	}
	system("rm -f /tmp/nextui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
	touch("/tmp/poweroff");
	sync();
	exit(0);
}

int PLAT_supportsDeepSleep(void) { return 1; }

///////////////////////////////

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}
double get_process_cpu_time_sec() {
	// this gives cpu time in nanoseconds needed to accurately calculate cpu usage in very short time frames. 
	// unfortunately about 20ms between meassures seems the lowest i can go to get accurate results
	// maybe in the future i will find and even more granual way to get cpu time, but might just be a limit of C or Linux alltogether
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}

static pthread_mutex_t currentcpuinfo;
// a roling average for the display values of about 2 frames, otherwise they are unreadable jumping too fast up and down and stuff to read
#define ROLLING_WINDOW 120  

volatile int useAutoCpu = 1;
void *PLAT_cpu_monitor(void *arg) {
    struct timespec start_time, curr_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

    long clock_ticks_per_sec = sysconf(_SC_CLK_TCK);

    double prev_real_time = get_time_sec();
    double prev_cpu_time = get_process_cpu_time_sec();

	const int cpu_frequencies[] = {408,450,500,550,  600,650,700,750, 800,850,900,950, 1000,1050,1100,1150, 1200,1250,1300,1350, 1400,1450,1500,1550, 1600,1650,1700,1750, 1800,1850,1900,1950, 2000};
    const int num_freqs = sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0]);
    int current_index = 5; 

    double cpu_usage_history[ROLLING_WINDOW] = {0};
    double cpu_speed_history[ROLLING_WINDOW] = {0};
    int history_index = 0;
    int history_count = 0; 

    while (true) {
        if (useAutoCpu) {
            double curr_real_time = get_time_sec();
            double curr_cpu_time = get_process_cpu_time_sec();

            double elapsed_real_time = curr_real_time - prev_real_time;
            double elapsed_cpu_time = curr_cpu_time - prev_cpu_time;
            double cpu_usage = 0;

            if (elapsed_real_time > 0) {
                cpu_usage = (elapsed_cpu_time / elapsed_real_time) * 100.0;
            }

            pthread_mutex_lock(&currentcpuinfo);

			// the goal here is is to keep cpu usage between 75% and 85% at the lowest possible speed so device stays cool and battery usage is at a minimum
			// if usage falls out of this range it will either scale a step down or up 
			// but if usage hits above 95% we need that max boost and we instant scale up to 2000mhz as long as needed
			// all this happens very fast like 60 times per second, so i'm applying roling averages to display values, so debug screen is readable and gives a good estimate on whats happening cpu wise
			// the roling averages are purely for displaying, the actual scaling is happening realtime each run. 
            if (cpu_usage > 95) {
                current_index = num_freqs - 1; // Instant power needed, cpu is above 95% Jump directly to max boost 2000MHz
            }
            else if (cpu_usage > 85 && current_index < num_freqs - 1) { // otherwise try to keep between 75 and 85 at lowest clock speed
                current_index++; 
            } 
            else if (cpu_usage < 75 && current_index > 0) {
                current_index--; 
            }

            PLAT_setCustomCPUSpeed(cpu_frequencies[current_index] * 1000);

            cpu_usage_history[history_index] = cpu_usage;
            cpu_speed_history[history_index] = cpu_frequencies[current_index];

            history_index = (history_index + 1) % ROLLING_WINDOW;
            if (history_count < ROLLING_WINDOW) {
                history_count++; 
            }

            double sum_cpu_usage = 0, sum_cpu_speed = 0;
            for (int i = 0; i < history_count; i++) {
                sum_cpu_usage += cpu_usage_history[i];
                sum_cpu_speed += cpu_speed_history[i];
            }

            currentcpuse = sum_cpu_usage / history_count;
            currentcpuspeed = sum_cpu_speed / history_count;

            pthread_mutex_unlock(&currentcpuinfo);

            prev_real_time = curr_real_time;
            prev_cpu_time = curr_cpu_time;
			// 20ms really seems lowest i can go, anything lower it becomes innacurate, maybe one day I will find another even more granual way to calculate usage accurately and lower this shit to 1ms haha, altough anything lower than 10ms causes cpu usage in itself so yeah
			// Anyways screw it 20ms is pretty much on a frame by frame basis anyways, so will anything lower really make a difference specially if that introduces cpu usage by itself? 
			// Who knows, maybe some CPU engineer will find my comment here one day and can explain, maybe this is looking for the limits of C and needs Assambler or whatever to call CPU instructions directly to go further, but all I know is PUSH and MOV, how did the orignal Roller Coaster Tycoon developer wrote a whole game like this anyways? Its insane..
            usleep(20000);
        } else {
            // Just measure CPU usage without changing frequency
            double curr_real_time = get_time_sec();
            double curr_cpu_time = get_process_cpu_time_sec();

            double elapsed_real_time = curr_real_time - prev_real_time;
            double elapsed_cpu_time = curr_cpu_time - prev_cpu_time;

            if (elapsed_real_time > 0) {
                double cpu_usage = (elapsed_cpu_time / elapsed_real_time) * 100.0;

                pthread_mutex_lock(&currentcpuinfo);

                cpu_usage_history[history_index] = cpu_usage;

                history_index = (history_index + 1) % ROLLING_WINDOW;
                if (history_count < ROLLING_WINDOW) {
                    history_count++;
                }

                double sum_cpu_usage = 0;
                for (int i = 0; i < history_count; i++) {
                    sum_cpu_usage += cpu_usage_history[i];
                }

                currentcpuse = sum_cpu_usage / history_count;

                pthread_mutex_unlock(&currentcpuinfo);
            }

            prev_real_time = curr_real_time;
            prev_cpu_time = curr_cpu_time;
            usleep(100000); 
        }
    }
}


#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
void PLAT_setCustomCPUSpeed(int speed) {
    FILE *fp = fopen(GOVERNOR_PATH, "w");
    if (fp == NULL) {
        perror("Failed to open scaling_setspeed");
        return;
    }

    fprintf(fp, "%d\n", speed);
    fclose(fp);
}
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; currentcpuspeed = 600; break;
		case CPU_SPEED_POWERSAVE:	freq = 1200000; currentcpuspeed = 1200; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; currentcpuspeed = 1600; break;
		case CPU_SPEED_PERFORMANCE: freq = 2000000; currentcpuspeed = 2000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define MAX_STRENGTH 0xFFFF
#define MIN_VOLTAGE 500000
#define MAX_VOLTAGE 3300000
#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"
#define RUMBLE_VOLTAGE_PATH "/sys/class/motor/voltage"

void PLAT_setRumble(int strength) {
	int voltage = MAX_VOLTAGE;

	if(strength > 0 && strength < MAX_STRENGTH) {
		voltage = MIN_VOLTAGE + (int)(strength * ((long long)(MAX_VOLTAGE - MIN_VOLTAGE) / MAX_STRENGTH));
		putInt(RUMBLE_VOLTAGE_PATH, voltage);
	}
	else {
		putInt(RUMBLE_VOLTAGE_PATH, MAX_VOLTAGE);
	}

	// enable rumble - removed the FN switch disabling haptics
	// did not make sense 
	putInt(RUMBLE_PATH, (strength) ? 1 : 0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	char* model = getenv("TRIMUI_MODEL");
	if (model) return model;
	return "Trimui Smart Pro";
}

void PLAT_getOsVersionInfo(char* output_str, size_t max_len)
{
	return getFile("/etc/version", output_str,max_len);
}

int PLAT_isOnline(void) {
	return online;
}





void PLAT_chmod(const char *file, int writable)
{
    struct stat statbuf;
    if (stat(file, &statbuf) == 0)
    {
        mode_t newMode;
        if (writable)
        {
            // Add write permissions for all users
            newMode = statbuf.st_mode | S_IWUSR | S_IWGRP | S_IWOTH;
        }
        else
        {
            // Remove write permissions for all users
            newMode = statbuf.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
        }

        // Apply the new permissions
        if (chmod(file, newMode) != 0)
        {
            printf("chmod error %d %s", writable, file);
        }
    }
    else
    {
        printf("stat error %d %s", writable, file);
    }
}




void PLAT_initDefaultLeds() {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	if(is_brick) {
	lightsDefault[0] = (LightSettings) {
		"FN 1 key",
		"f1",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[1] = (LightSettings) {
		"FN 2 key",
		"f2",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[2] = (LightSettings) {
		"Topbar",
		"m",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[3] = (LightSettings) {
		"L/R triggers",
		"lr",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
} else {
	lightsDefault[0] = (LightSettings) {
		"Joysticks",
		"lr",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[1] = (LightSettings) {
		"Logo",
		"m",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
}
}
void PLAT_initLeds(LightSettings *lights) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);

	PLAT_initDefaultLeds();
	FILE *file;
	if(is_brick) {
		file = PLAT_OpenSettings("ledsettings_brick.txt");
	}
	else {
		file = PLAT_OpenSettings("ledsettings.txt");
	}

    if (file == NULL)
    {
		
        LOG_info("Unable to open led settings file\n");
	
    }
	else {
		char line[256];
		int current_light = -1;
		while (fgets(line, sizeof(line), file))
		{
			if (line[0] == '[')
			{
				// Section header
				char light_name[255];
				if (sscanf(line, "[%49[^]]]", light_name) == 1)
				{
					current_light++;
					if (current_light < MAX_LIGHTS)
					{
						strncpy(lights[current_light].name, light_name, 255 - 1);
						lights[current_light].name[255 - 1] = '\0'; // Ensure null-termination
						lights[current_light].cycles = -1; // cycles (times animation loops) should basically always be -1 for unlimited unless specifically set
					}
					else
					{
						current_light = -1; // Reset if max_lights exceeded
					}
				}
			}
			else if (current_light >= 0 && current_light < MAX_LIGHTS)
			{
				int temp_value;
				uint32_t temp_color;
				char filename[255];

				if (sscanf(line, "filename=%s", &filename) == 1)
				{
					strncpy(lights[current_light].filename, filename, 255 - 1);
					continue;
				}
				if (sscanf(line, "effect=%d", &temp_value) == 1)
				{
					lights[current_light].effect = temp_value;
					continue;
				}
				if (sscanf(line, "color1=%x", &temp_color) == 1)
				{
					lights[current_light].color1 = temp_color;
					continue;
				}
				if (sscanf(line, "color2=%x", &temp_color) == 1)
				{
					lights[current_light].color2 = temp_color;
					continue;
				}
				if (sscanf(line, "speed=%d", &temp_value) == 1)
				{
					lights[current_light].speed = temp_value;
					continue;
				}
				if (sscanf(line, "brightness=%d", &temp_value) == 1)
				{
					lights[current_light].brightness = temp_value;
					continue;
				}
				if (sscanf(line, "trigger=%d", &temp_value) == 1)
				{
					lights[current_light].trigger = temp_value;
					continue;
				}
				if (sscanf(line, "inbrightness=%d", &temp_value) == 1)
				{
					lights[current_light].inbrightness = temp_value;
					continue;
				}
			}
		}

		fclose(file);
	}

	
	LOG_info("lights setup\n");
}

#define LED_PATH1 "/sys/class/led_anim/max_scale"
#define LED_PATH2 "/sys/class/led_anim/max_scale_lr"
#define LED_PATH3 "/sys/class/led_anim/max_scale_f1f2" 

void PLAT_setLedInbrightness(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
	if(is_brick) {
		if (strcmp(led->filename, "m") == 0) {
			snprintf(filepath, sizeof(filepath), LED_PATH1);
		} else if (strcmp(led->filename, "f1") == 0) {
			snprintf(filepath, sizeof(filepath),LED_PATH3);
		} else  {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_%s", led->filename);
		}
	} else {
		snprintf(filepath, sizeof(filepath), LED_PATH1);
	}
	if (strcmp(led->filename, "f2") != 0) {
		// do nothhing for f2
		PLAT_chmod(filepath, 1);
		file = fopen(filepath, "w");
		if (file != NULL)
		{
			fprintf(file, "%i\n", led->inbrightness);
			fclose(file);
		}
		PLAT_chmod(filepath, 0);
	}
}
void PLAT_setLedBrightness(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
	if(is_brick) {
		if (strcmp(led->filename, "m") == 0) {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale");
		} else if (strcmp(led->filename, "f1") == 0) {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_f1f2");
		} else  {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_%s", led->filename);
		}
	} else {
		snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale");
	}
	if (strcmp(led->filename, "f2") != 0) {
		// do nothhing for f2
		PLAT_chmod(filepath, 1);
		file = fopen(filepath, "w");
		if (file != NULL)
		{
			fprintf(file, "%i\n", led->brightness);
			fclose(file);
		}
		PLAT_chmod(filepath, 0);
	}
}
void PLAT_setLedEffect(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->effect);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedEffectCycles(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_cycles_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->cycles);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedEffectSpeed(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_duration_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->speed);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedColor(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_rgb_hex_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%06X\n", led->color1);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}

//////////////////////////////////////////////

int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	sprintf(cmd, "date -s '%d-%d-%d %d:%d:%d'; hwclock -u -w", y,m,d,h,i,s);
	system(cmd);
	return 0; // why does this return an int?
}

#define MAX_LINE_LENGTH 200
#define ZONE_PATH "/usr/share/zoneinfo"
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

	char *output = (char *)malloc(256);
	if (!output) {
		return false;
	}
	FILE *fp = popen("uci get system.@system[0].zonename", "r");
	if (!fp) {
		free(output);
		return false;
	}
	fgets(output, 256, fp);
	pclose(fp);
	trimTrailingNewlines(output);

	return output;
}

void PLAT_setCurrentTimezone(const char* tz) {
	if (cached_tz_count == -1) {
		LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        return;
    }

	// This makes it permanent
	char *zonename = (char *)malloc(256);
	if (!zonename)
		return;
	snprintf(zonename, 256, "uci set system.@system[0].zonename=\"%s\"", tz);
	system(zonename);
	//system("uci set system.@system[0].zonename=\"Europe/Berlin\"");
	system("uci del -q system.@system[0].timezone");
	system("uci commit system");
	free(zonename);

	// This fixes the timezone until the next reboot
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return;
	}
	snprintf(tz_path, 256, ZONE_PATH "/%s", tz);
	// replace existing symlink
	if (unlink("/tmp/localtime") == -1) {
		LOG_error("Failed to remove existing symlink: %s\n", strerror(errno));
	}
	if (symlink(tz_path, "/tmp/localtime") == -1) {
		LOG_error("Failed to set timezone: %s\n", strerror(errno));
	}
	free(tz_path);

	// apply timezone to kernel
	system("date -k");
}

bool PLAT_getNetworkTimeSync(void) {
	char *output = (char *)malloc(256);
	if (!output) {
		return false;
	}
	FILE *fp = popen("uci get system.ntp.enable", "r");
	if (!fp) {
		free(output);
		return false;
	}
	fgets(output, 256, fp);
	pclose(fp);
	bool result = (output[0] == '1');
	free(output);
	return result;
}

void PLAT_setNetworkTimeSync(bool on) {
	// note: this is not the service residing at /etc/init.d/ntpd - that one has hardcoded time server URLs and does not interact with UCI.
	if (on) {
		// permanment
		system("uci set system.ntp.enable=1");
		system("uci commit system");
		system("/etc/init.d/ntpd reload");
	} else {
		// permanment
		system("uci set system.ntp.enable=0");
		system("uci commit system");
		system("/etc/init.d/ntpd stop");
	}
}

/////////////////////////

bool PLAT_supportSSH() { return true; }

/////////////////////////

#include <wifi_intf.h>
#include "wmg_debug.h"
#include "wifi_udhcpc.h"

static struct WIFI_Context {
	const aw_wifi_interface_t *interface;
	int lastEvent;
	bool enabled;
	bool connected;
} wifi = {
	.interface = NULL,
	.lastEvent = STATE_UNKNOWN,
	.enabled = false,
	.connected = false};

static void wifi_state_handle(struct Manager *w, int event_label)
{
    LOG_info("WMG: event_label 0x%x\n", event_label);

	wifi.lastEvent = w->StaEvt.state;
	switch (w->StaEvt.state)
	{
		 case CONNECTING:
		 {
			LOG_info("WMG: Connecting to the network......\n");
			break;
		 }
		 case CONNECTED:
		 {
			LOG_info("WMG: Connected to the AP\n");
			start_udhcpc();
			wifi.connected = true;
			break;
		 }
		 case OBTAINING_IP:
		 {
			LOG_info("WMG: Getting ip address......\n");
			break;
		 }
		 case NETWORK_CONNECTED:
		 {
			LOG_info("WMG: Successful network connection\n");
			break;
		 }
		case DISCONNECTED:
		{
			wifi.connected = false;
			LOG_info("WMG: Disconnected,the reason:%s\n", wmg_event_txt(w->StaEvt.event));
			break;
		}
    }
}

bool PLAT_hasWifi() { return true; }
void PLAT_wifiInit() {
	LOG_info("Wifi init\n");
	wifi.enabled = CFG_getWifi();
	PLAT_wifiEnable(wifi.enabled);
}

bool PLAT_wifiEnabled() {
	// less efficient, more accurate: check "$(cat /sys/class/net/wlan0/flags 2>/dev/null)" != "0x1003"
	// if we can be reasonably sure that nobody killed wifi and bypassed our code:
	return wifi.enabled;
}

#define MAX_CONNECTION_ATTEMPTS 5

void PLAT_wifiEnable(bool on) {
	if(on) {
		LOG_info("turning wifi on...\n");
		
		// This shouldnt be needed, but we cant really rely on nobody else messing with this stuff. 
		// Make sure supplicant is up and rfkill doesnt block.
		system("rfkill unblock wifi");
		//system("ifconfig wlan0 down");
		system("/etc/init.d/wpa_supplicant enable");
		system("/etc/init.d/wpa_supplicant start&");

		int event_label = 42;
		for (int i = 0 ; i<= MAX_CONNECTION_ATTEMPTS ;i++) {
			wifi.interface = aw_wifi_on(wifi_state_handle, event_label);
			if(wifi.interface != NULL)
				break;
			ms_sleep(1000);
			LOG_info("connect wpa_supplicant: tried %d times\n", i+1);
		}
		if(wifi.interface == NULL) {
			LOG_error("failed to turn on wifi.\n");
			wifi.enabled = false;
		}
		else {
			wifi.enabled = true;
		}
	}
	else if(wifi.interface) {
		LOG_debug("turning wifi off...\n");

		// Honestly, I'd rather not do this but it seems to keep the  questionable wifi implementation
		// on Trimui from randomly reconnecting automatically
		system("rfkill block wifi");
		//system("ifconfig wlan0 up");
		system("/etc/init.d/wpa_supplicant stop&");

		int ret = aw_wifi_off(wifi.interface);
		if(ret < 0)
		{
			LOG_error("Test failed: wifi off error!\n");
			return;
		}
		// only necessary for the wmg_log output (debugging)
		fflush(stdout);

		wifi.interface = NULL;
		wifi.enabled = false;
	}

	// Keep config in sync
	CFG_setWifi(wifi.enabled);
}

int PLAT_wifiScan(struct WIFI_network *networks, int max)
{
	if(wifi.interface == NULL) {
		LOG_info("PLAT_wifiScan: failed to get wifi interface.\n");
		return -1;
	}

	char results[4096];
	int length = 4096;
	if(wifi.interface->get_scan_results(results, &length) < 0) {
		LOG_info("PLAT_wifiScan: failed to get wifi scan results.\n");
		return -1;
	}

	LOG_info("%s\n", results);

	// Results will be in this form:
	//[INFO] bssid / frequency / signal level / flags / ssid
	//04:b4:fe:32:f9:73	2462	-63	[WPA2-PSK-CCMP][WPS][ESS]	frynet
	//04:b4:fe:32:e4:50	2437	-56	[WPA2-PSK-CCMP][WPS][ESS]	frynet

	// Parse the results string into a list of WIFI_network elements
	char *line = strtok(results, "\n");
	// skip the first line which only has the column headers
	line = strtok(NULL, "\n");
	int count = 0;
	while(line != NULL && count < max) {
		struct WIFI_network *network = &networks[count];
		network->bssid[0] = '\0';
		network->ssid[0] = '\0';
		network->freq = -1;
		network->rssi = -1;
		network->security = SECURITY_NONE;

		char features[128];
		sscanf(line, "%17[0-9a-fA-F:]\t%d\t%d\t%127[^\t]\t%127[^\n]", network->bssid, &network->freq, &network->rssi,
			   features, network->ssid);
		
		line = strtok(NULL, "\n");
		
		// skip over "hidden" networks with empty SSID. We would need to adapt wifimgr classes
		// to properly support them, I dont think anyone will miss them.
		if(!network->ssid || !network->ssid[0]) {
			LOG_info("Ignoring network %s with empty SSID\n", network->bssid);
		}
		else {
			if(containsString(features,"WPA2-PSK"))
				network->security = SECURITY_WPA2_PSK;
			else if(containsString(features,"WPA-PSK"))
				network->security = SECURITY_WPA_PSK;
			else if(containsString(features,"WEP"))
				network->security = SECURITY_WEP;
			else if(containsString(features,"EAP"))
				network->security = SECURITY_UNSUPPORTED;
			
			count++;
		}
	}
	return count;
}

bool PLAT_wifiConnected()
{
	if (wifi.interface) {
		char ssid[128] = "";
		int ssid_len = sizeof(ssid);
		int ret = wifi.interface->is_ap_connected(ssid, &ssid_len);
		if(ret >= 0 && ssid[0] != '\0') {
			LOG_info("is_ap_connected: yes - %s\n", ssid);
			return true;
		}
		else {
			LOG_info("is_ap_connected: %d\n",ret);
		}
	}
	return false;
}

int PLAT_wifiConnection(struct WIFI_connection *connection_info)
{
	if (wifi.interface && connection_info) {
		if(PLAT_wifiConnected()) {
			connection_status status;
			if(wifi.interface->get_connection_info(&status) >= 0) {
				connection_info->freq = status.freq;
				connection_info->link_speed = status.link_speed;
				connection_info->noise = status.noise;
				connection_info->rssi = status.noise;
				strcpy(connection_info->ip, status.ip_address);
				strcpy(connection_info->ssid, status.ssid);

				// get_connection_info returns garbage SSID sometimes
				char ssid[128] = "";
				int ssid_len = sizeof(ssid);
				int ret = wifi.interface->is_ap_connected(ssid, &ssid_len);
				if(ret == 0)
					strcpy(connection_info->ssid, ssid);
			}
			else {
				LOG_error("Failed to get Wifi connection info\n");
			}
			LOG_info("Connected AP: %s\n", connection_info->ssid);
			LOG_info("IP address: %s\n", connection_info->ip);
		}
		else {
			connection_info->freq = -1;
			connection_info->link_speed = -1;
			connection_info->noise = -1;
			connection_info->rssi = -1;
			*connection_info->ip = '\0';
			*connection_info->ssid = '\0';
			LOG_info("PLAT_wifiConnection: Not connected\n", connection_info->ssid);
		}

		return 0;
	}
	return -1;
}

bool PLAT_wifiHasCredentials(char *ssid, WifiSecurityType sec)
{
	if(wifi.interface == NULL) {
		LOG_info("failed to get wifi interface.\n");
		return false;
	}

	if(sec == SECURITY_UNSUPPORTED){
		LOG_info("unsupported WifiDecurityType.\n");
		return false;
	}

	char net_id[10]="";
    int id_len = sizeof(net_id);
	int ret = wifi.interface->get_netid(ssid, (tKEY_MGMT)sec, net_id, &id_len);

	if (ret == 0) {
		LOG_info("Got netid %s for ssid %s sectype %d\n", net_id, ssid, sec);
		return true;
	}
	return false;
}

void PLAT_wifiForget(char *ssid, WifiSecurityType sec)
{
	if(wifi.interface == NULL) {
		LOG_info("failed to get wifi interface.\n");
		return;
	}

	if(sec == SECURITY_UNSUPPORTED){
		LOG_info("unsupported WifiDecurityType.\n");
		return;
	}

	int ret = wifi.interface->remove_network(ssid, (tKEY_MGMT)sec);
	LOG_info("wifi clear_network returned %d for %s with sectype %d\n", ret, ssid, sec);
}

void PLAT_wifiConnect(char *ssid, WifiSecurityType sec)
{
	if(wifi.interface == NULL) {
		LOG_info("failed to get wifi interface.\n");
		return;
		 //-1;
	}

	if(sec == SECURITY_UNSUPPORTED){
		LOG_info("unsupported WifiDecurityType.\n");
		return;
	}

	LOG_info("Attempting to connect to SSID %s\n", ssid);

	char net_id[10]="";
    int id_len = sizeof(net_id);
	int ret = wifi.interface->get_netid(ssid, (tKEY_MGMT)sec, net_id, &id_len);
	if(ret != 0) {
		LOG_info("netid failed \n");
		return;
	}
	else {
		LOG_info("Got netid %s for ssid %s sectype %d\n", net_id, ssid, sec);
	}

	ret = wifi.interface->connect_ap_with_netid(net_id, 42);
	LOG_info("wifi connect_ap_with_netid %s returned %d\n", net_id, ret);
	if (aw_wifi_get_wifi_state() == NETWORK_CONNECTED)
		LOG_info("wifi connected.\n");
	else
		LOG_info("wifi connection failed.\n");
}

void PLAT_wifiConnectPass(const char *ssid, WifiSecurityType sec, const char* pass)
{
	if(wifi.interface == NULL) {
		LOG_info("failed to get wifi interface.\n");
		return;
	}

	if(sec == SECURITY_UNSUPPORTED){
		LOG_info("unsupported WifiDecurityType.\n");
		return;
	}

	int ret = wifi.interface->connect_ap_key_mgmt(ssid, (tKEY_MGMT)sec, pass, 42);
	LOG_info("wifi connect_ap returned %d\n", ret);
	if (aw_wifi_get_wifi_state() == NETWORK_CONNECTED)
		LOG_info("wifi connected.\n");
	else
		LOG_info("wifi connection failed.\n");
}

void PLAT_wifiDisconnect()
{
	if(wifi.interface == NULL) {
		LOG_info("failed to get wifi interface.\n");
		return;
	}

	int ret = wifi.interface->disconnect_ap(42);
	LOG_info("wifi disconnect_ap returned %d\n", ret);
}