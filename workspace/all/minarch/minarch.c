#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <msettings.h>

#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>

#include "libretro.h"
#include "defines.h"
#include "api.h"
#include "utils.h"
#include "scaler_neon.h"

///////////////////////////////////////

static SDL_Surface* screen;
static int quit;
static int show_menu;

enum {
	SCALE_NATIVE,
	SCALE_ASPECT,
	SCALE_FULLSCREEN,
};

// default frontend options
static int screen_scaling = SCALE_ASPECT;
static int show_scanlines = 0;
static int optimize_text = 1;
static int prevent_tearing = 1; // lenient
static int show_debug = 0;
static int max_ff_speed = 3; // 4x
static int fast_forward = 0;
static int overclock = 1; // normal

GFX_Renderer renderer;

///////////////////////////////////////

static struct Core {
	int initialized;
	int need_fullpath;
	
	const char tag[8]; // eg. GBC
	const char name[128]; // eg. gambatte
	const char version[128]; // eg. Gambatte (v0.5.0-netlink 7e02df6)
	const char extensions[128]; // eg. gb|gbc|dmg
	
	const char config_dir[MAX_PATH]; // eg. /mnt/sdcard/.userdata/rg35xx/GB-gambatte
	const char states_dir[MAX_PATH]; // eg. /mnt/sdcard/.userdata/arm-480/GB-gambatte
	const char saves_dir[MAX_PATH]; // eg. /mnt/sdcard/Saves/GB
	const char bios_dir[MAX_PATH]; // eg. /mnt/sdcard/Bios/GB
	
	double fps;
	double sample_rate;
	double aspect_ratio;
	
	void* handle;
	void (*init)(void);
	void (*deinit)(void);
	
	void (*get_system_info)(struct retro_system_info *info);
	void (*get_system_av_info)(struct retro_system_av_info *info);
	void (*set_controller_port_device)(unsigned port, unsigned device);
	
	void (*reset)(void);
	void (*run)(void);
	size_t (*serialize_size)(void);
	bool (*serialize)(void *data, size_t size);
	bool (*unserialize)(const void *data, size_t size);
	bool (*load_game)(const struct retro_game_info *game);
	bool (*load_game_special)(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*unload_game)(void);
	unsigned (*get_region)(void);
	void *(*get_memory_data)(unsigned id);
	size_t (*get_memory_size)(unsigned id);
	
	// retro_audio_buffer_status_callback_t audio_buffer_status;
} core;

///////////////////////////////////////
// based on picoarch/unzip.c

#define ZIP_HEADER_SIZE 30
#define ZIP_CHUNK_SIZE 65536
#define ZIP_LE_READ16(buf) ((uint16_t)(((uint8_t *)(buf))[1] << 8 | ((uint8_t *)(buf))[0]))
#define ZIP_LE_READ32(buf) ((uint32_t)(((uint8_t *)(buf))[3] << 24 | ((uint8_t *)(buf))[2] << 16 | ((uint8_t *)(buf))[1] << 8 | ((uint8_t *)(buf))[0]))
typedef int (*Zip_extract_t)(FILE* zip, FILE* dst, size_t size);

static int Zip_copy(FILE* zip, FILE* dst, size_t size) { // uncompressed 
	uint8_t buffer[ZIP_CHUNK_SIZE];
	while (size) {
		size_t sz = MIN(size, ZIP_CHUNK_SIZE);
		if (sz!= fread(buffer, 1, sz, zip)) return -1;
		if (sz!=fwrite(buffer, 1, sz, dst)) return -1;
		size -= sz;
	}
	return 0;
}
static int Zip_inflate(FILE* zip, FILE* dst, size_t size) { // compressed
	z_stream stream = {0};
	size_t have = 0;
	uint8_t  in[ZIP_CHUNK_SIZE];
	uint8_t out[ZIP_CHUNK_SIZE];
	int ret = -1;

	ret = inflateInit2(&stream, -MAX_WBITS);
	if (ret != Z_OK)
		return ret;
	
	do {
		size_t insize = MIN(size, ZIP_CHUNK_SIZE);

		stream.avail_in = fread(in, 1, insize, zip);
		if (ferror(zip)) {
			(void)inflateEnd(&stream);
			return Z_ERRNO;
		}

		if (!stream.avail_in)
			break;
		stream.next_in = in;

		do {
			stream.avail_out = ZIP_CHUNK_SIZE;
			stream.next_out = out;

			ret = inflate(&stream, Z_NO_FLUSH);
			switch(ret) {
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&stream);
					return ret;
			}

			have = ZIP_CHUNK_SIZE - stream.avail_out;
			if (fwrite(out, 1, have, dst) != have || ferror(dst)) {
				(void)inflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (stream.avail_out == 0);

		size -= insize;
	} while (size && ret != Z_STREAM_END);

	(void)inflateEnd(&stream);

	if (!size || ret == Z_STREAM_END) {
		return Z_OK;
	} else {
		return Z_DATA_ERROR;
	}
}

///////////////////////////////////////

static struct Game {
	char path[MAX_PATH];
	char name[MAX_PATH]; // TODO: rename to basename?
	char m3u_path[MAX_PATH];
	char tmp_path[MAX_PATH]; // location of unzipped file
	void* data;
	size_t size;
	int is_open;
} game;
static void Game_open(char* path) {
	LOG_info("Game_open\n");
	memset(&game, 0, sizeof(game));
	
	strcpy((char*)game.path, path);
	strcpy((char*)game.name, strrchr(path, '/')+1);
	
	// if we have a zip file
	if (suffixMatch(".zip", game.path)) {
		LOG_info("is zip file\n");
		int supports_zip = 0;
		int i = 0;
		char* ext;
		char exts[128];
		char* extensions[32];
		strcpy(exts,core.extensions);
		while ((ext=strtok(i?NULL:exts,"|"))) {
			extensions[i++] = ext;
			if (!strcmp("zip", ext)) {
				supports_zip = 1;
				break;
			}
		}
		extensions[i] = NULL;
	
		// if the core doesn't support zip files natively
		if (!supports_zip) {
			FILE *zip = fopen(game.path, "r");
			if (zip==NULL) {
				LOG_error("Error opening archive: %s\n\t%s\n", game.path, strerror(errno));
				return;
			}
			
			// extract a known file format
			uint8_t header[ZIP_HEADER_SIZE];
			uint32_t next = 0;
			uint16_t len = 0;
			char filename[MAX_PATH];
			uint32_t compressed_size = 0;
			char extension[8];
			while (1) {
				if (next) fseek(zip, next, SEEK_CUR);
				
				if (ZIP_HEADER_SIZE!=fread(header, 1, ZIP_HEADER_SIZE, zip)) break;
				
				if ((uint16_t)(header[6]) & 0x0008) break;
				
				len = ZIP_LE_READ16(&header[26]);
				if (len>=MAX_PATH) break;
				
				if (len!=fread(filename,1,len,zip)) break;
				filename[len] = '\0';
				LOG_info("filename: %s\n", filename);
				
				compressed_size = ZIP_LE_READ32(&header[18]);
				
				fseek(zip, ZIP_LE_READ16(&header[28]), SEEK_CUR);
				next = compressed_size;
				
				int found = 0;
				for (i=0; extensions[i]; i++) {
					sprintf(extension, ".%s", extensions[i]);
					if (suffixMatch(extension, filename)) {
						
						found = 1;
						break;
					}
				}
				if (!found) continue;
				
				char tmp_template[MAX_PATH];
				strcpy(tmp_template, "/tmp/minarch-XXXXXX");
				char* tmp_dirname = mkdtemp(tmp_template);
				LOG_info("tmp_dirname: %s\n", tmp_dirname);
				sprintf(game.tmp_path, "%s/%s", tmp_dirname, basename(filename));
				
				// TODO: we need to clear game.tmp_path if anything below this point fails!
				
				FILE* dst = fopen(game.tmp_path, "w");
				if (dst==NULL) {
					game.tmp_path[0] = '\0';
					LOG_error("Error extracting file: %s\n\t%s\n", filename, strerror(errno));
					return;
				}
				
				Zip_extract_t extract = NULL;
				switch (ZIP_LE_READ16(&header[8])) {
					case 0: extract = Zip_copy; break;
					case 8: extract = Zip_inflate; break;
				}
				
				if (!extract || extract(zip,dst,compressed_size)) {
					game.tmp_path[0] = '\0';
					LOG_error("Error extracting file: %s\n\t%s\n", filename, strerror(errno));
					return;
				}
				
				fclose(dst);
				
				break;
			}
			
			fclose(zip);
		}
	}
		
	// some cores handle opening files themselves, eg. pcsx_rearmed
	// if the frontend tries to load a 500MB file itself bad things happen
	if (!core.need_fullpath) {
		path = game.tmp_path[0]=='\0'?game.path:game.tmp_path;
		
		FILE *file = fopen(path, "r");
		if (file==NULL) {
			LOG_error("Error opening game: %s\n\t%s\n", path, strerror(errno));
			return;
		}
	
		fseek(file, 0, SEEK_END);
		game.size = ftell(file);
	
		rewind(file);
		game.data = malloc(game.size);
		if (game.data==NULL) {
			LOG_error("Couldn't allocate memory for file: %s\n", path);
			return;
		}
	
		fread(game.data, sizeof(uint8_t), game.size, file);
	
		fclose(file);
	}
	
	// m3u-based?
	char* tmp;
	char m3u_path[256];
	char base_path[256];
	char dir_name[256];

	strcpy(m3u_path, game.path);
	tmp = strrchr(m3u_path, '/') + 1;
	tmp[0] = '\0';
	
	strcpy(base_path, m3u_path);
	
	tmp = strrchr(m3u_path, '/');
	tmp[0] = '\0';

	tmp = strrchr(m3u_path, '/');
	strcpy(dir_name, tmp);
	
	tmp = m3u_path + strlen(m3u_path); 
	strcpy(tmp, dir_name);
	
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");
	
	if (exists(m3u_path)) {
		strcpy(game.m3u_path, m3u_path);
		strcpy((char*)game.name, strrchr(m3u_path, '/')+1);
	}
	
	game.is_open = 1;
}
static void Game_close(void) {
	if (game.data) free(game.data);
	if (game.tmp_path[0]) remove(game.tmp_path);
	game.is_open = 0;
	VIB_setStrength(0); // just in case
}

static struct retro_disk_control_ext_callback disk_control_ext;
static void Game_changeDisc(char* path) {
	
	if (exactMatch(game.path, path) || !exists(path)) return;
	
	Game_close();
	Game_open(path);
	
	struct retro_game_info game_info = {};
	game_info.path = game.path;
	game_info.data = game.data;
	game_info.size = game.size;
	
	disk_control_ext.replace_image_index(0, &game_info);
	putFile(CHANGE_DISC_PATH, path); // MinUI still needs to know this to update recents.txt
}

///////////////////////////////////////

static void SRAM_getPath(char* filename) {
	sprintf(filename, "%s/%s.sav", core.saves_dir, game.name);
}
static void SRAM_read(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (read): %s\n", filename);
	
	FILE *sram_file = fopen(filename, "r");
	if (!sram_file) return;

	void* sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || !fread(sram, 1, sram_size, sram_file)) {
		LOG_error("Error reading SRAM data\n");
	}

	fclose(sram_file);
}
static void SRAM_write(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (write): %s\n", filename);
		
	FILE *sram_file = fopen(filename, "w");
	if (!sram_file) {
		LOG_error("Error opening SRAM file: %s\n", strerror(errno));
		return;
	}

	void *sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || sram_size != fwrite(sram, 1, sram_size, sram_file)) {
		LOG_error("Error writing SRAM data to file\n");
	}

	fclose(sram_file);

	sync();
}

///////////////////////////////////////

static int state_slot = 0;
static void State_getPath(char* filename) {
	sprintf(filename, "%s/%s.st%i", core.states_dir, game.name, state_slot);
}
static void State_read(void) { // from picoarch
	size_t state_size = core.serialize_size();
	if (!state_size) return;

	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);
	
	FILE *state_file = fopen(filename, "r");
	if (!state_file) {
		if (state_slot!=8) { // st8 is a default state in MiniUI and may not exist, that's okay
			LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
		}
		goto error;
	}

	if (state_size != fread(state, 1, state_size, state_file)) {
		LOG_error("Error reading state data from file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (!core.unserialize(state, state_size)) {
		LOG_error("Error restoring save state: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

error:
	if (state) free(state);
	if (state_file) fclose(state_file);
}
static void State_write(void) { // from picoarch
	size_t state_size = core.serialize_size();
	if (!state_size) return;

	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);
	
	FILE *state_file = fopen(filename, "w");
	if (!state_file) {
		LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (!core.serialize(state, state_size)) {
		LOG_error("Error creating save state: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

	if (state_size != fwrite(state, 1, state_size, state_file)) {
		LOG_error("Error writing state data to file: %s (%s)\n", filename, strerror(errno));
		goto error;
	}

error:
	if (state) free(state);
	if (state_file) fclose(state_file);

	sync();
}
static void State_autosave(void) {
	int last_state_slot = state_slot;
	state_slot = AUTO_RESUME_SLOT;
	State_write();
	state_slot = last_state_slot;
}
static void State_resume(void) {
	if (!exists(RESUME_SLOT_PATH)) return;
	
	int last_state_slot = state_slot;
	state_slot = getInt(RESUME_SLOT_PATH);
	unlink(RESUME_SLOT_PATH);
	State_read();
	state_slot = last_state_slot;
}

///////////////////////////////

typedef struct Option {
	char* key;
	char* name; // desc
	char* desc; // info, truncated
	char* full; // info
	char* var;
	int default_value;
	int value;
	int count; // TODO: drop this?
	int lock;
	char** values;
	char** labels;
} Option;
typedef struct OptionList OptionList;
// typedef void (*OptionList_callback_t)(OptionList* list, const char* key);
typedef struct OptionList {
	int count;
	int changed;
	Option* options;
	int enabled_count;
	Option** enabled_options;
	// OptionList_callback_t on_set;
} OptionList;

static char* onoff_labels[] = {
	"Off",
	"On",
	NULL
};
static char* scaling_labels[] = {
	"Native",
	"Aspect",
	"Fullscreen",
	NULL
};
static char* tearing_labels[] = {
	"Off",
	"Lenient",
	"Strict",
	NULL
};
static char* max_ff_labels[] = {
	"None",
	"2x",
	"3x",
	"4x",
	"5x",
	"6x",
	"7x",
	"8x",
	NULL,
};

///////////////////////////////

enum {
	FE_OPT_SCALING,
	FE_OPT_SCANLINES,
	FE_OPT_TEXT,
	FE_OPT_TEARING,
	FE_OPT_OVERCLOCK,
	FE_OPT_DEBUG,
	FE_OPT_MAXFF,
	FE_OPT_COUNT,
};

enum {
	SHORTCUT_SAVE_STATE,
	SHORTCUT_LOAD_STATE,
	SHORTCUT_RESET_GAME,
	SHORTCUT_CYCLE_SCALE,
	SHORTCUT_TOGGLE_SCANLINES,
	SHORTCUT_TOGGLE_FF,
	SHORTCUT_HOLD_FF,
	SHORTCUT_COUNT,
};

#define LOCAL_BUTTON_COUNT 14
#define RETRO_BUTTON_COUNT 16 // allow L3/R3 to be remapped by user if desired, eg. Virtual Boy uses extra buttons for right d-pad

typedef struct ButtonMapping { 
	char* name;
	int retro;
	int local; // TODO: dislike this name...
	int mod;
	int default_;
	int ignore;
} ButtonMapping;

static ButtonMapping default_button_mapping[] = { // used if pak.cfg doesn't exist or doesn't have bindings
	{"Up",			RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_UP},
	{"Down",		RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DOWN},
	{"Left",		RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_LEFT},
	{"Right",		RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_RIGHT},
	{"A Button",	RETRO_DEVICE_ID_JOYPAD_A,		BTN_ID_A},
	{"B Button",	RETRO_DEVICE_ID_JOYPAD_B,		BTN_ID_B},
	{"X Button",	RETRO_DEVICE_ID_JOYPAD_X,		BTN_ID_X},
	{"Y Button",	RETRO_DEVICE_ID_JOYPAD_Y,		BTN_ID_Y},
	{"Start",		RETRO_DEVICE_ID_JOYPAD_START,	BTN_ID_START},
	{"Select",		RETRO_DEVICE_ID_JOYPAD_SELECT,	BTN_ID_SELECT},
	{"L1 Button",	RETRO_DEVICE_ID_JOYPAD_L,		BTN_ID_L1},
	{"R1 Button",	RETRO_DEVICE_ID_JOYPAD_R,		BTN_ID_R1},
	{"L2 Button",	RETRO_DEVICE_ID_JOYPAD_L2,		BTN_ID_L2},
	{"R2 Button",	RETRO_DEVICE_ID_JOYPAD_R2,		BTN_ID_R2},
	{"L3 Button",	RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_NONE},
	{"R3 Button",	RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_NONE},
	{NULL,0,0}
};
static ButtonMapping button_label_mapping[] = { // used to lookup the retro_id and local btn_id from button name
	{"NONE",	-1,								BTN_ID_NONE},
	{"UP",		RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_UP},
	{"DOWN",	RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DOWN},
	{"LEFT",	RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_LEFT},
	{"RIGHT",	RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_RIGHT},
	{"A",		RETRO_DEVICE_ID_JOYPAD_A,		BTN_ID_A},
	{"B",		RETRO_DEVICE_ID_JOYPAD_B,		BTN_ID_B},
	{"X",		RETRO_DEVICE_ID_JOYPAD_X,		BTN_ID_X},
	{"Y",		RETRO_DEVICE_ID_JOYPAD_Y,		BTN_ID_Y},
	{"START",	RETRO_DEVICE_ID_JOYPAD_START,	BTN_ID_START},
	{"SELECT",	RETRO_DEVICE_ID_JOYPAD_SELECT,	BTN_ID_SELECT},
	{"L1",		RETRO_DEVICE_ID_JOYPAD_L,		BTN_ID_L1},
	{"R1",		RETRO_DEVICE_ID_JOYPAD_R,		BTN_ID_R1},
	{"L2",		RETRO_DEVICE_ID_JOYPAD_L2,		BTN_ID_L2},
	{"R2",		RETRO_DEVICE_ID_JOYPAD_R2,		BTN_ID_R2},
	{"L3",		RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_NONE},
	{"R3",		RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_NONE},
	{NULL,0,0}
};
static ButtonMapping core_button_mapping[RETRO_BUTTON_COUNT+1] = {0};

static const char* device_button_names[LOCAL_BUTTON_COUNT] = {
	[BTN_ID_UP]		= "UP",
	[BTN_ID_DOWN]	= "DOWN",
	[BTN_ID_LEFT]	= "LEFT",
	[BTN_ID_RIGHT]	= "RIGHT",
	[BTN_ID_SELECT]	= "SELECT",
	[BTN_ID_START]	= "START",
	[BTN_ID_Y]		= "Y",
	[BTN_ID_X]		= "X",
	[BTN_ID_B]		= "B",
	[BTN_ID_A]		= "A",
	[BTN_ID_L1]		= "L1",
	[BTN_ID_R1]		= "R1",
	[BTN_ID_L2]		= "L2",
	[BTN_ID_R2]		= "R2",
};


// NOTE: these must be in BTN_ID_ order also off by 1 because of NONE (which is -1 in BTN_ID_ land)
static char* button_labels[] = {
	"NONE", // displayed by default
	"UP",
	"DOWN",
	"LEFT",
	"RIGHT",
	"A",
	"B",
	"X",
	"Y",
	"START",
	"SELECT",
	"L1",
	"R1",
	"L2",
	"R2",
	NULL,
};
static char* shortcut_labels[] = {
	"NONE", // displayed by default
	"UP",
	"DOWN",
	"LEFT",
	"RIGHT",
	"A",
	"B",
	"X",
	"Y",
	"START",
	"SELECT",
	"L1",
	"R1",
	"L2",
	"R2",
	"MENU+UP",
	"MENU+DOWN",
	"MENU+LEFT",
	"MENU+RIGHT",
	"MENU+A",
	"MENU+B",
	"MENU+X",
	"MENU+Y",
	"MENU+START",
	"MENU+SELECT",
	"MENU+L1",
	"MENU+R1",
	"MENU+L2",
	"MENU+R2",
	NULL,
};
static char* overclock_labels[] = {
	"Powersave",
	"Normal",
	"Performance",
	NULL,
};

enum {
	CONFIG_NONE,
	CONFIG_CONSOLE,
	CONFIG_GAME,
};

static struct Config {
	char* default_cfg; // pak.cfg based on platform limitations
	char* user_cfg; // minarch.cfg or game.cfg based on user preference
	OptionList frontend;
	OptionList core;
	ButtonMapping* controls;
	ButtonMapping* shortcuts;
	int loaded;
	int initialized;
} config = {
	.frontend = (OptionList){
		.count = FE_OPT_COUNT,
		.options = (Option[]){
			[FE_OPT_SCALING] = {
				.key	= "minarch_screen_scaling", 
				.name	= "Screen Scaling",
				.desc	= "Native uses integer scaling. Aspect uses the core reported\naspect ratio. Fullscreen will produce non-square pixels. Gross.",
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = scaling_labels,
				.labels = scaling_labels,
			},
			[FE_OPT_SCANLINES] = {
				.key	= "minarch_scanlines_grid", 
				.name	= "Scanlines/Grid",
				.desc	= "Simulate scanlines (or a pixel grid at odd scales).\nOnly applies to native scaling.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_TEXT] = {
				.key	= "minarch_optimize_text", 
				.name	= "Optimize Text",
				.desc	= "Prioritize a consistent stroke width when upscaling single\npixel lines using nearest neighbor scaler. Increases CPU load.\nOnly applies to native scaling.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_TEARING] = {
				.key	= "minarch_prevent_tearing",
				.name	= "Prevent Tearing",
				.desc	= "Wait for vsync before drawing the next frame. Lenient\nonly waits when within frame budget. Strict always waits.",
				.default_value = VSYNC_LENIENT,
				.value = VSYNC_LENIENT,
				.count = 3,
				.values = tearing_labels,
				.labels = tearing_labels,
			},
			[FE_OPT_OVERCLOCK] = {
				.key	= "minarch_cpu_speed",
				.name	= "CPU Speed",
				.desc	= "Over- or underclock the CPU to prioritize\npure performance or power savings.",
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = overclock_labels,
				.labels = overclock_labels,
			},
			[FE_OPT_DEBUG] = {
				.key	= "minarch_debug_hud",
				.name	= "Debug HUD",
				.desc	= "Show frames per second, cpu load,\nresolution, and scaler information.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_MAXFF] = {
				.key	= "minarch_max_ff_speed",
				.name	= "Max FF Speed",
				.desc	= "Fast forward will not exceed the selected speed\n(but may be less than depending on game and emulator).",
				.default_value = 3, // 4x
				.value = 3, // 4x
				.count = 8,
				.values = max_ff_labels,
				.labels = max_ff_labels,
			},
			[FE_OPT_COUNT] = {NULL}
		}
	},
	.core = (OptionList){
		.count = 0,
		.options = (Option[]){
			{NULL},
		},
	},
	.controls = default_button_mapping,
	.shortcuts = (ButtonMapping[]){
		[SHORTCUT_SAVE_STATE]			= {"Save State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_LOAD_STATE]			= {"Load State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_RESET_GAME]			= {"Reset Game",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_CYCLE_SCALE]			= {"Cycle Scaling",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_TOGGLE_SCANLINES]		= {"Toggle Scanlines",	-1, BTN_ID_NONE, 0},
		[SHORTCUT_TOGGLE_FF]			= {"Toggle FF",			-1, BTN_ID_NONE, 0},
		[SHORTCUT_HOLD_FF]				= {"Hold FF",			-1, BTN_ID_NONE, 0},
		{NULL}
	},
};
static int Config_getValue(char* cfg, const char* key, char* out_value, int* lock) {
	char* tmp = cfg;
	while ((tmp = strstr(tmp, key))) {
		if (lock!=NULL && tmp>cfg && *(tmp-1)=='-') *lock = 1; // prefixed with a `-` means lock
		tmp += strlen(key);
		if (!strncmp(tmp, " = ", 3)) break;
	};
	if (!tmp) return 0;
	tmp += 3;
	
	strncpy(out_value, tmp, 256);
	out_value[256 - 1] = '\0';
	tmp = strchr(out_value, '\n');
	if (!tmp) tmp = strchr(out_value, '\r');
	if (tmp) *tmp = '\0';

	// LOG_info("\t%s = %s\n", key, out_value);
	return 1;
}

static void setOverclock(int i) {
	overclock = i;
	switch (i) {
		case 0: POW_setCPUSpeed(CPU_SPEED_POWERSAVE); break;
		case 1: POW_setCPUSpeed(CPU_SPEED_NORMAL); break;
		case 2: POW_setCPUSpeed(CPU_SPEED_PERFORMANCE); break;
	}
}
static void Config_syncFrontend(int i, int value) {
	switch (i) {
		case FE_OPT_SCALING:	screen_scaling 	= value; renderer.dst_p = 0; break;
		case FE_OPT_SCANLINES:	show_scanlines 	= value; renderer.dst_p = 0; break;
		case FE_OPT_TEXT:		optimize_text 	= value; renderer.dst_p = 0; break;
		case FE_OPT_TEARING:	prevent_tearing = value; break;
		case FE_OPT_OVERCLOCK:	overclock		= value; break;
		case FE_OPT_DEBUG:		show_debug 		= value; break;
		case FE_OPT_MAXFF:		max_ff_speed 	= value; break;
	}
	Option* option = &config.frontend.options[i];
	option->value = value;
}
static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value);
enum {
	CONFIG_WRITE_ALL,
	CONFIG_WRITE_GAME,
};
static void Config_getPath(char* filename, int override) {
	if (override) sprintf(filename, "%s/%s.cfg", core.config_dir, game.name);
	else sprintf(filename, "%s/minarch.cfg", core.config_dir);
}
static void Config_init(void) {
	if (!config.default_cfg || config.initialized) return;
	
	LOG_info("Config_init\n");
	char* tmp = config.default_cfg;
	char* tmp2;
	char* key;
	
	char button_name[128];
	char button_id[128];
	int i = 0;
	while ((tmp = strstr(tmp, "bind "))) {
		tmp += 5; // tmp now points to the button name (plus the rest of the line)
		key = tmp;
		tmp = strstr(tmp, " = ");
		if (!tmp) break;
		
		int len = tmp-key;
		strncpy(button_name, key, len);
		button_name[len] = '\0';
		
		tmp += 3;
		strncpy(button_id, tmp, 128);
		tmp2 = strchr(button_id, '\n');
		if (!tmp2) tmp2 = strchr(button_id, '\r');
		if (tmp2) *tmp2 = '\0';
		
		int retro_id = -1;
		int local_id = -1;
		
		tmp2 = strrchr(button_id, ':');
		int remap = 0;
		if (tmp2) {
			for (int j=0; button_label_mapping[j].name; j++) {
				ButtonMapping* button = &button_label_mapping[j];
				if (!strcmp(tmp2+1,button->name)) {
					retro_id = button->retro;
					break;
				}
			}
			*tmp2 = '\0';
		}
		for (int j=0; button_label_mapping[j].name; j++) {
			ButtonMapping* button = &button_label_mapping[j];
			if (!strcmp(button_id,button->name)) {
				local_id = button->local;
				if (retro_id==-1) retro_id = button->retro;
				break;
			}
		}
		
		tmp += strlen(button_id); // prepare to continue search
		
		LOG_info("\tbind %s (%s) %i:%i\n", button_name, button_id, local_id, retro_id);
		
		// TODO: test this without a final line return
		tmp2 = calloc(strlen(button_name)+1, sizeof(char));
		strcpy(tmp2, button_name);
		ButtonMapping* button = &core_button_mapping[i++];
		button->name = tmp2;
		button->retro = retro_id;
		button->local = local_id;
	};
	
	config.initialized = 1;
}
static void Config_quit(void) {
	if (!config.initialized) return;
	for (int i=0; core_button_mapping[i].name; i++) {
		free(core_button_mapping[i].name);
	}
}
static void Config_readOptionsString(char* cfg) {
	if (!cfg) return;

	LOG_info("Config_readOptions\n");
	char key[256];
	char value[256];
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.frontend, option->key, value);
		Config_syncFrontend(i, option->value);
	}
	
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.core, option->key, value);
	}
}
static void Config_readControlsString(char* cfg) {
	if (!cfg) return;

	LOG_info("Config_readControls\n");
	
	char key[256];
	char value[256];
	char* tmp;
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		sprintf(key, "bind %s", mapping->name);
		sprintf(value, "NONE");
		
		if (!Config_getValue(cfg, key, value, NULL)) continue;
		if ((tmp = strrchr(value, ':'))) *tmp = '\0'; // this is a binding artifact in default.cfg, ignore
		
		int id = -1;
		for (int j=0; button_labels[j]; j++) {
			if (!strcmp(button_labels[j],value)) {
				id = j - 1;
				break;
			}
		}
		// LOG_info("\t%s (%i)\n", value, id);
		
		mapping->local = id;
		mapping->mod = 0;
	}
	
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		sprintf(key, "bind %s", mapping->name);
		sprintf(value, "NONE");

		if (!Config_getValue(cfg, key, value, NULL)) continue;
		
		int id = -1;
		for (int j=0; shortcut_labels[j]; j++) {
			if (!strcmp(shortcut_labels[j],value)) {
				id = j - 1;
				break;
			}
		}
		
		int mod = 0;
		if (id>=LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}
		// LOG_info("shortcut %s:%s (%i:%i)\n", key,value, id, mod);

		mapping->local = id;
		mapping->mod = mod;
	}
}
static void Config_load(void) {
	LOG_info("Config_load\n");
	
	char default_path[MAX_PATH];
	getEmuPath((char *)core.tag, default_path);
	char* tmp = strrchr(default_path, '/');
	strcpy(tmp,"/default.cfg");
	
	if (exists(default_path)) config.default_cfg = allocFile(default_path);
	else config.default_cfg = NULL;
	
	char path[MAX_PATH];
	config.loaded = CONFIG_NONE;
	int override = 0;
	Config_getPath(path, CONFIG_WRITE_GAME);
	if (exists(path)) override = 1; 
	if (!override) Config_getPath(path, CONFIG_WRITE_ALL);
	
	config.user_cfg = allocFile(path);
	if (!config.user_cfg) return;
	
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
}
static void Config_free(void) {
	if (config.default_cfg) free(config.default_cfg);
	if (config.user_cfg) free(config.user_cfg);
}
static void Config_readOptions(void) {
	Config_readOptionsString(config.default_cfg);
	Config_readOptionsString(config.user_cfg);

	// screen_scaling = SCALE_NATIVE; // TODO: tmp
}
static void Config_readControls(void) {
	Config_readControlsString(config.default_cfg);
	Config_readControlsString(config.user_cfg);
}
static void Config_write(int override) {
	char path[MAX_PATH];
	// sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
	Config_getPath(path, CONFIG_WRITE_GAME);
	
	if (!override) {
		if (config.loaded==CONFIG_GAME) unlink(path);
		Config_getPath(path, CONFIG_WRITE_ALL);
	}
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
	
	FILE *file = fopen(path, "wb");
	if (!file) return;
	
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, shortcut_labels[j]);
	}
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, shortcut_labels[j]);
	}
	
	fclose(file);
	sync();
}
static void Config_restore(void) {
	char path[MAX_PATH];
	if (config.loaded==CONFIG_GAME) {
		sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
		unlink(path);
	}
	else if (config.loaded==CONFIG_CONSOLE) {
		sprintf(path, "%s/minarch.cfg", core.config_dir);
		unlink(path);
	}
	config.loaded = CONFIG_NONE;
	
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		option->value = option->default_value;
		Config_syncFrontend(i, option->value);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		option->value = option->default_value;
	}
	config.core.changed = 1; // let the core know

	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		mapping->local = mapping->default_;
		mapping->mod = 0;
	}
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		mapping->local = BTN_ID_NONE;
		mapping->mod = 0;
	}
	
	Config_load();
	Config_readOptions();
	Config_readControls();
	Config_free();
	
	renderer.dst_p = 0;
}

///////////////////////////////

static  int Option_getValueIndex(Option* item, const char* value) {
	if (!value) return 0;
	for (int i=0; i<item->count; i++) {
		if (!strcmp(item->values[i], value)) return i;
	}
	return 0;
}
static void Option_setValue(Option* item, const char* value) {
	// TODO: store previous value?
	item->value = Option_getValueIndex(item, value);
}

// the following 3 functions always touch config.core, the rest can operate on arbitrary OptionLists
static void OptionList_init(const struct retro_core_option_definition *defs) {
	LOG_info("OptionList_init\n");
	int count;
	for (count=0; defs[count].key; count++);
	
	// LOG_info("count: %i\n", count);
	
	// TODO: add frontend options to this? so the can use the same override method? eg. minarch_*
	
	config.core.count = count;
	if (count) {
		config.core.options = calloc(count+1, sizeof(Option));
		
		for (int i=0; i<config.core.count; i++) {
			int len;
			const struct retro_core_option_definition *def = &defs[i];
			Option* item = &config.core.options[i];
			len = strlen(def->key) + 1;
		
			item->key = calloc(len, sizeof(char));
			strcpy(item->key, def->key);
			
			len = strlen(def->desc) + 1;
			item->name = calloc(len, sizeof(char));
			strcpy(item->name, def->desc);
			
			if (def->info) {
				len = strlen(def->info) + 1;
				item->desc = calloc(len, sizeof(char));
				strncpy(item->desc, def->info, len);

				item->full = calloc(len, sizeof(char));
				strncpy(item->full, item->desc, len);
				// item->desc[len-1] = '\0';
				
				// these magic numbers are more about chars per line than pixel width 
				// so it's not going to be relative to the screen size, only the scale
				GFX_wrapText(font.tiny, item->desc, SCALE1(240), 2); // TODO magic number!
				GFX_wrapText(font.medium, item->full, SCALE1(260), 7); // TODO: magic number!
			}
		
			for (count=0; def->values[count].value; count++);
		
			item->count = count;
			item->values = calloc(count+1, sizeof(char*));
			item->labels = calloc(count+1, sizeof(char*));
	
			for (int j=0; j<count; j++) {
				const char* value = def->values[j].value;
				const char* label = def->values[j].label;
		
				len = strlen(value) + 1;
				item->values[j] = calloc(len, sizeof(char));
				strcpy(item->values[j], value);
		
				if (label) {
					len = strlen(label) + 1;
					item->labels[j] = calloc(len, sizeof(char));
					strcpy(item->labels[j], label);
				}
				else {
					item->labels[j] = item->values[j];
				}
				// printf("\t%s\n", item->labels[j]);
			}
			
			item->value = Option_getValueIndex(item, def->default_value);
			item->default_value = item->value;
			
			LOG_info("\tINIT %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		}
	}
	// fflush(stdout);
}
static void OptionList_vars(const struct retro_variable *vars) {
	LOG_info("OptionList_vars\n");
	int count;
	for (count=0; vars[count].key; count++);
	
	config.core.count = count;
	if (count) {
		config.core.options = calloc(count+1, sizeof(Option));
	
		for (int i=0; i<config.core.count; i++) {
			int len;
			const struct retro_variable *var = &vars[i];
			Option* item = &config.core.options[i];

			len = strlen(var->key) + 1;
			item->key = calloc(len, sizeof(char));
			strcpy(item->key, var->key);
			
			len = strlen(var->value) + 1;
			item->var = calloc(len, sizeof(char));
			strcpy(item->var, var->value);
			
			char* tmp = strchr(item->var, ';');
			if (tmp && *(tmp+1)==' ') {
				*tmp = '\0';
				item->name = item->var;
				tmp += 2;
			}
			
			char* opt = tmp;
			for (count=0; (tmp=strchr(tmp, '|')); tmp++, count++);
			count += 1; // last entry after final '|'
		
			item->count = count;
			item->values = calloc(count+1, sizeof(char*));
			item->labels = calloc(count+1, sizeof(char*));

			tmp = opt;
			int j;
			for (j=0; (tmp=strchr(tmp, '|')); j++) {
				item->values[j] = opt;
				item->labels[j] = opt;
				*tmp = '\0';
				tmp += 1;
				opt = tmp; 
			}
			item->values[j] = opt;
			item->labels[j] = opt;
			
			// no native default_value support for retro vars
			item->value = 0;
			item->default_value = item->value;
			// printf("SET %s to %s (%i)\n", item->key, default_value, item->value); fflush(stdout);
		}
	}
	// fflush(stdout);
}
static void OptionList_reset(void) {
	if (!config.core.count) return;
	
	for (int i=0; i<config.core.count; i++) {
		Option* item = &config.core.options[i];
		if (item->var) {
			// values/labels are all points to var
			// so no need to free individually
			free(item->var);
		}
		else {
			if (item->desc) free(item->desc);
			if (item->full) free(item->full);
			for (int j=0; j<item->count; j++) {
				char* value = item->values[j];
				char* label = item->labels[j];
				if (label!=value) free(label);
				free(value);
			}
		}
		free(item->values);
		free(item->labels);
		free(item->key);
		free(item->name);
	}
	if (config.core.enabled_options) free(config.core.enabled_options);
	config.core.enabled_count = 0;
	free(config.core.options);
}

static Option* OptionList_getOption(OptionList* list, const char* key) {
	for (int i=0; i<list->count; i++) {
		Option* item = &list->options[i];
		if (!strcmp(item->key, key)) return item;
	}
	return NULL;
}
static char* OptionList_getOptionValue(OptionList* list, const char* key) {
	Option* item = OptionList_getOption(list, key);
	if (item) LOG_info("\tGET %s (%s) = %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
	
	if (item) return item->values[item->value];
	else LOG_warn("unknown option %s \n", key);
	return NULL;
}
static void OptionList_setOptionRawValue(OptionList* list, const char* key, int value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		item->value = value;
		list->changed = 1;
		LOG_info("\tRAW SET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);
	}
	else LOG_info("unknown option %s \n", key);
}
static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		Option_setValue(item, value);
		list->changed = 1;
		LOG_info("\tSET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);
	}
	else LOG_info("unknown option %s \n", key);
}
// static void OptionList_setOptionVisibility(OptionList* list, const char* key, int visible) {
// 	Option* item = OptionList_getOption(list, key);
// 	if (item) item->visible = visible;
// 	else printf("unknown option %s \n", key); fflush(stdout);
// }

///////////////////////////////

static void Menu_beforeSleep(void);
static void Menu_afterSleep(void);

static uint32_t buttons = 0; // RETRO_DEVICE_ID_JOYPAD_* buttons
static int ignore_menu = 0;
static void input_poll_callback(void) {
	PAD_poll();

	int show_setting = 0;
	POW_update(NULL, &show_setting, Menu_beforeSleep, Menu_afterSleep);

	// I _think_ this can stay as is...
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
	}
	if (PAD_isPressed(BTN_MENU) && (PAD_isPressed(BTN_PLUS) || PAD_isPressed(BTN_MINUS))) {
		ignore_menu = 1;
	}
	
	static int toggled_ff_on = 0; // this logic only works because TOGGLE_FF is before HOLD_FF in the menu...
	for (int i=0; i<SHORTCUT_COUNT; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int btn = 1 << mapping->local;
		if (btn==BTN_NONE) continue; // not bound
		if (!mapping->mod || PAD_isPressed(BTN_MENU)) {
			if (i==SHORTCUT_TOGGLE_FF) {
				if (PAD_justPressed(btn)) {
					fast_forward = toggled_ff_on = !fast_forward;
					if (mapping->mod) ignore_menu = 1;
					break;
				}
				else if (PAD_justReleased(btn)) {
					if (mapping->mod) ignore_menu = 1;
					break;
				}
			}
			else if (i==SHORTCUT_HOLD_FF) {
				// don't allow turn off fast_forward with a release of the hold button 
				// if it was initially turned on with the toggle button
				if (PAD_justPressed(btn) || (!toggled_ff_on && PAD_justReleased(btn))) {
					fast_forward = PAD_isPressed(btn);
					if (mapping->mod) ignore_menu = 1; // very unlikely but just in case
				}
			}
			else if (PAD_justPressed(btn)) {
				switch (i) {
					case SHORTCUT_SAVE_STATE: State_write(); break;
					case SHORTCUT_LOAD_STATE: State_read(); break;
					case SHORTCUT_RESET_GAME: core.reset(); break;
					case SHORTCUT_CYCLE_SCALE:
						screen_scaling += 1;
						if (screen_scaling>=3) screen_scaling -= 3;
						Config_syncFrontend(FE_OPT_SCALING, screen_scaling);
						break;
					case SHORTCUT_TOGGLE_SCANLINES:
						if (screen_scaling==SCALE_NATIVE) {
							Config_syncFrontend(FE_OPT_SCANLINES, !show_scanlines);
						}
						break;
					default: break;
				}
				
				if (mapping->mod) ignore_menu = 1;
			}
		}
	}
	
	if (!ignore_menu && PAD_justReleased(BTN_MENU)) {
		show_menu = 1;
	}
	
	buttons = 0;
	for (int i=0; config.controls[i].name; i++) {
		int btn = 1 << config.controls[i].local;
		if (btn==BTN_NONE) continue; // present buttons can still be unbound
		if (PAD_isPressed(btn)) buttons |= 1 << config.controls[i].retro;
		//  && !POW_ignoreSettingInput(btn, show_setting)
	}
	
	// if (buttons) LOG_info("buttons: %i\n", buttons);
}
static int16_t input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
	// id == RETRO_DEVICE_ID_JOYPAD_MASK or RETRO_DEVICE_ID_JOYPAD_*
	if (port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0) {
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return buttons;
		return (buttons >> id) & 1;
	}
	return 0;
}
///////////////////////////////

static void Input_init(const struct retro_input_descriptor *vars) {
	static int input_initialized = 0;
	if (input_initialized) return;

	LOG_info("Input_init\n");
	
	config.controls = core_button_mapping[0].name ? core_button_mapping : default_button_mapping;
	
	puts("---------------------------------");

	const char* core_button_names[RETRO_BUTTON_COUNT] = {0};
	int present[RETRO_BUTTON_COUNT];
	int core_mapped = 0;
	if (vars) {
		core_mapped = 1;
		// identify buttons available in this core
		for (int i=0; vars[i].description; i++) {
			const struct retro_input_descriptor* var = &vars[i];
			if (var->port!=0 || var->device!=RETRO_DEVICE_JOYPAD || var->index!=0) continue;

			// TODO: don't ignore unavailable buttons, just override them to BTN_ID_NONE!
			if (var->id>=RETRO_BUTTON_COUNT) {
				printf("UNAVAILABLE: %s\n", var->description); fflush(stdout);
				continue;
			}
			else {
				printf("PRESENT    : %s\n", var->description); fflush(stdout);
			}
			present[var->id] = 1;
			core_button_names[var->id] = var->description;
		}
	}
	
	puts("---------------------------------");

	for (int i=0;default_button_mapping[i].name; i++) {
		ButtonMapping* mapping = &default_button_mapping[i];
		LOG_info("DEFAULT %s (%s): <%s>\n", core_button_names[mapping->retro], mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]));
		mapping->name = (char*)core_button_names[mapping->retro];
	}
	
	puts("---------------------------------");

	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		mapping->default_ = mapping->local;

		// ignore mappings that aren't available in this core
		if (core_mapped && !present[mapping->retro]) {
			mapping->ignore = 1;
			continue;
		}
		LOG_info("%s: <%s> (%i:%i)\n", mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]), mapping->local, mapping->retro);
	}
	
	puts("---------------------------------");
	input_initialized = 1;
}

static bool set_rumble_state(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
	// TODO: handle other args? not sure I can
	VIB_setStrength(strength);
	return 1;
}
static bool environment_callback(unsigned cmd, void *data) { // copied from picoarch initially
	// LOG_info("environment_callback: %i\n", cmd);
	
	switch(cmd) {
	case RETRO_ENVIRONMENT_GET_OVERSCAN: { /* 2 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: { /* 3 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE: { /* 6 */
		const struct retro_message *message = (const struct retro_message*)data;
		if (message) LOG_info("%s\n", message->msg);
		break;
	}
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: { /* 8 */
		// puts("RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL");
		// TODO: used by fceumm at least
	}
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: { /* 9 */
		const char **out = (const char **)data;
		if (out) {
			*out = core.bios_dir;
		}
		
		break;
	}
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: { /* 10 */
		const enum retro_pixel_format *format = (enum retro_pixel_format *)data;

		if (*format != RETRO_PIXEL_FORMAT_RGB565) { // TODO: pull from platform.h?
			/* 565 is only supported format */
			return false;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: { /* 11 */
		// puts("RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
		Input_init((const struct retro_input_descriptor *)data);
		return false;
	} break;
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: { /* 13 */
		const struct retro_disk_control_callback *var =
			(const struct retro_disk_control_callback *)data;

		if (var) {
			memset(&disk_control_ext, 0, sizeof(struct retro_disk_control_ext_callback));
			memcpy(&disk_control_ext, var, sizeof(struct retro_disk_control_callback));
		}
		break;
	}

	// TODO: this is called whether using variables or options
	case RETRO_ENVIRONMENT_GET_VARIABLE: { /* 15 */
		// puts("RETRO_ENVIRONMENT_GET_VARIABLE");
		struct retro_variable *var = (struct retro_variable *)data;
		if (var && var->key) {
			var->value = OptionList_getOptionValue(&config.core, var->key);
			// printf("\t%s = %s\n", var->key, var->value);
		}
		break;
	}
	// TODO: I think this is where the core reports its variables (the precursor to options)
	// TODO: this is called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION sets out to 0
	// TODO: not used by anything yet
	case RETRO_ENVIRONMENT_SET_VARIABLES: { /* 16 */
		// puts("RETRO_ENVIRONMENT_SET_VARIABLES");
		const struct retro_variable *vars = (const struct retro_variable *)data;
		if (vars) {
			OptionList_reset();
			OptionList_vars(vars);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: { /* 18 */
		bool flag = *(bool*)data;
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %i\n", cmd, flag);
		break;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: { /* 17 */
		bool *out = (bool *)data;
		if (out) {
			*out = config.core.changed;
			config.core.changed = 0;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: { /* 21 */
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK\n", cmd);
		break;
	}
	case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: { /* 23 */
	        struct retro_rumble_interface *iface = (struct retro_rumble_interface*)data;

	        // LOG_info("Setup rumble interface.\n");
	        iface->set_rumble_state = set_rumble_state;
		break;
	}
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: { /* 27 */
		struct retro_log_callback *log_cb = (struct retro_log_callback *)data;
		if (log_cb)
			log_cb->log = (void (*)(enum retro_log_level, const char*, ...))LOG_note; // same difference
		break;
	}
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: { /* 31 */
		const char **out = (const char **)data;
		if (out)
			*out = core.saves_dir; // save_dir;
		break;
	}
	// RETRO_ENVIRONMENT_GET_LANGUAGE 39
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: { /* 51 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: { /* 52 */
		// puts("RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION");
		unsigned *out = (unsigned *)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: { /* 53 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS");
		if (data) {
			OptionList_reset();
			OptionList_init((const struct retro_core_option_definition *)data); 
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: { /* 54 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL");
		const struct retro_core_options_intl *options = (const struct retro_core_options_intl *)data;
		if (options && options->us) {
			OptionList_reset();
			OptionList_init(options->us);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: { /* 55 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY");
		// const struct retro_core_option_display *display = (const struct retro_core_option_display *)data;
	// 	if (display) OptionList_setOptionVisibility(&config.core, display->key, display->visible);
		break;
	}
	case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: { /* 57 */
		unsigned *out =	(unsigned *)data;
		if (out) *out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: { /* 58 */
		const struct retro_disk_control_ext_callback *var =
			(const struct retro_disk_control_ext_callback *)data;

		if (var) {
			memcpy(&disk_control_ext, var, sizeof(struct retro_disk_control_ext_callback));
		}
		break;
	}
	// TODO: RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
	// TODO: used by mgba, (but only during frameskip?)
	// case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: { /* 62 */
	// 	LOG_info("RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK\n");
	// 	const struct retro_audio_buffer_status_callback *cb = (const struct retro_audio_buffer_status_callback *)data;
	// 	if (cb) {
	// 		LOG_info("has audo_buffer_status callback\n");
	// 		core.audio_buffer_status = cb->callback;
	// 	} else {
	// 		LOG_info("no audo_buffer_status callback\n");
	// 		core.audio_buffer_status = NULL;
	// 	}
	// 	break;
	// }
	// TODO: used by mgba, (but only during frameskip?)
	// case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: { /* 63 */
	// 	LOG_info("RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY\n");
	//
	// 	const unsigned *latency_ms = (const unsigned *)data;
	// 	if (latency_ms) {
	// 		unsigned frames = *latency_ms * core.fps / 1000;
	// 		if (frames < 30)
	// 			// audio_buffer_size_override = frames;
	// 			LOG_info("audio_buffer_size_override = %i (unused?)\n", frames);
	// 		else
	// 			LOG_info("Audio buffer change out of range (%d), ignored\n", frames);
	// 	}
	// 	break;
	// }

	// TODO: RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE 64
	case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: { /* 65 */
		// const struct retro_system_content_info_override* info = (const struct retro_system_content_info_override* )data;
		// if (info) LOG_info("has overrides");
		break;
	}
	// TODO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK 69
	// TODO: used by gambatte for L/R palette switching (seems like it needs to return true even if data is NULL to indicate support)
	case RETRO_ENVIRONMENT_SET_VARIABLE: {
		// puts("RETRO_ENVIRONMENT_SET_VARIABLE");
		const struct retro_variable *var = (const struct retro_variable *)data;
		if (var && var->key) {
			// printf("\t%s = %s\n", var->key, var->value);
			OptionList_setOptionValue(&config.core, var->key, var->value);
			break;
		}

		int *out = (int *)data;
		if (out) *out = 1;
		
		break;
	}
	
	// unused
	// case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
	// 	puts("RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK"); fflush(stdout);
	// 	break;
	// }
	// case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
	// 	puts("RETRO_ENVIRONMENT_GET_THROTTLE_STATE"); fflush(stdout);
	// 	break;
	// }
	// case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
	// 	puts("RETRO_ENVIRONMENT_GET_FASTFORWARDING"); fflush(stdout);
	// 	break;
	// };
	
	// TODO: these unknowns are probably some flag OR'd to RETRO_ENVIRONMENT_EXPERIMENTAL
	// TODO: UNKNOWN 65572
	// TODO: UNKNOWN 65578
	// TODO: UNKNOWN 65581
	// TODO: UNKNOWN 65587
	default:
		LOG_debug("Unsupported environment cmd: %u\n", cmd);
		return false;
	}
	return true;
}

///////////////////////////////

// TODO: this is a dumb API
// TODO: this hasn't been adjusted for SCALE
SDL_Surface* digits;
#define DIGIT_WIDTH 18
#define DIGIT_HEIGHT 16
#define DIGIT_TRACKING -4
enum {
	DIGIT_SLASH = 10,
	DIGIT_DOT,
	DIGIT_PERCENT,
	DIGIT_X,
	DIGIT_OP, // (
	DIGIT_CP, // )
	DIGIT_COUNT,
};
#define DIGIT_SPACE DIGIT_COUNT
static void MSG_init(void) {
	// TODO: scale
	digits = SDL_CreateRGBSurface(SDL_SWSURFACE,DIGIT_WIDTH*DIGIT_COUNT,DIGIT_HEIGHT,FIXED_DEPTH, 0,0,0,0);
	SDL_FillRect(digits, NULL, RGB_BLACK);
	
	SDL_Surface* digit;
	char* chars[] = { "0","1","2","3","4","5","6","7","8","9","/",".","%","x","(",")", NULL };
	char* c;
	int i = 0;
	while ((c = chars[i])) {
		digit = TTF_RenderUTF8_Blended(font.tiny, c, COLOR_WHITE);
		SDL_BlitSurface(digit, NULL, digits, &(SDL_Rect){ (i * DIGIT_WIDTH) + (DIGIT_WIDTH - digit->w)/2, (DIGIT_HEIGHT - digit->h)/2});
		SDL_FreeSurface(digit);
		i += 1;
	}
}
static int MSG_blitChar(int n, int x, int y) {
	if (n!=DIGIT_SPACE) SDL_BlitSurface(digits, &(SDL_Rect){n*DIGIT_WIDTH,0,DIGIT_WIDTH,DIGIT_HEIGHT}, screen, &(SDL_Rect){x,y});
	return x + DIGIT_WIDTH + DIGIT_TRACKING;
}
static int MSG_blitInt(int num, int x, int y) {
	int i = num;
	int n;
	
	if (i > 999) {
		n = i / 1000;
		i -= n * 1000;
		x = MSG_blitChar(n,x,y);
	}
	if (i > 99) {
		n = i / 100;
		i -= n * 100;
		x = MSG_blitChar(n,x,y);
	}
	else if (num>99) {
		x = MSG_blitChar(0,x,y);
	}
	if (i > 9) {
		n = i / 10;
		i -= n * 10;
		x = MSG_blitChar(n,x,y);
	}
	else if (num>9) {
		x = MSG_blitChar(0,x,y);
	}
	
	n = i;
	x = MSG_blitChar(n,x,y);
	
	return x;
}
static int MSG_blitDouble(double num, int x, int y) {
	int i = num;
	int r = (num-i) * 10;
	int n;
	
	x = MSG_blitInt(i, x,y);

	n = DIGIT_DOT;
	x = MSG_blitChar(n,x,y);
	
	n = r;
	x = MSG_blitChar(n,x,y);
	return x;
}
static void MSG_quit(void) {
	SDL_FreeSurface(digits);
}

///////////////////////////////

// from gambatte-dms
//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))

static int cpu_ticks = 0;
static int fps_ticks = 0;
static int use_ticks = 0;
static double fps_double = 0;
static double cpu_double = 0;
static double use_double = 0;
static uint32_t sec_start = 0;


//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight1_1(A, B)  ((((cR(A) + cR(B)) >> 1) & 0x1f) << 11 | (((cG(A) + cG(B)) >> 1) & 0x3f) << 5 | (((cB(A) + cB(B)) >> 1) & 0x1f))
#define Weight1_2(A, B)  ((((cR(A) + (cR(B) << 1)) / 3) & 0x1f) << 11 | (((cG(A) + (cG(B) << 1)) / 3) & 0x3f) << 5 | (((cB(A) + (cB(B) << 1)) / 3) & 0x1f))
#define Weight2_1(A, B)  ((((cR(B) + (cR(A) << 1)) / 3) & 0x1f) << 11 | (((cG(B) + (cG(A) << 1)) / 3) & 0x3f) << 5 | (((cB(B) + (cB(A) << 1)) / 3) & 0x1f))
#define Weight1_3(A, B)  ((((cR(A) + (cR(B) * 3)) >> 2) & 0x1f) << 11 | (((cG(A) + (cG(B) * 3)) >> 2) & 0x3f) << 5 | (((cB(A) + (cB(B) * 3)) >> 2) & 0x1f))
#define Weight3_1(A, B)  ((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight1_4(A, B)  ((((cR(A) + (cR(B) << 2)) / 5) & 0x1f) << 11 | (((cG(A) + (cG(B) << 2)) / 5) & 0x3f) << 5 | (((cB(A) + (cB(B) << 2)) / 5) & 0x1f))
#define Weight4_1(A, B)  ((((cR(B) + (cR(A) << 2)) / 5) & 0x1f) << 11 | (((cG(B) + (cG(A) << 2)) / 5) & 0x3f) << 5 | (((cB(B) + (cB(A) << 2)) / 5) & 0x1f))
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))
#define Weight1_1_1_1(A, B, C, D)  ((((cR(A) + cR(B) + cR(C) + cR(D)) >> 2) & 0x1f) << 11 | (((cG(A) + cG(B) + cG(C) + cG(D)) >> 2) & 0x3f) << 5 | (((cB(A) + cB(B) + cB(C) + cB(D)) >> 2) & 0x1f))

static void scaleNull(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {}
static void scale1x(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// pitch of src image not src buffer!
	// eg. gb has a 160 pixel wide image but 
	// gambatte uses a 256 pixel wide buffer
	// (only matters when using memcpy) 
	int src_pitch = w * FIXED_BPP; 
	int src_stride = pitch / FIXED_BPP;
	int dst_stride = dst_pitch / FIXED_BPP;
	int cpy_pitch = MIN(src_pitch, dst_pitch);
	
	uint16_t* restrict src_row = (uint16_t*)src;
	uint16_t* restrict dst_row = (uint16_t*)dst;
	for (int y=0; y<h; y++) {
		memcpy(dst_row, src_row, cpy_pitch);
		dst_row += dst_stride;
		src_row += src_stride;
	}
	
}
static void scale1x_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// pitch of src image not src buffer!
	// eg. gb has a 160 pixel wide image but 
	// gambatte uses a 256 pixel wide buffer
	// (only matters when using memcpy) 
	int src_pitch = w * FIXED_BPP; 
	int src_stride = 2 * pitch / FIXED_BPP;
	int dst_stride = 2 * dst_pitch / FIXED_BPP;
	int cpy_pitch = MIN(src_pitch, dst_pitch);
	
	uint16_t k = 0x0000;
	uint16_t* restrict src_row = (uint16_t*)src;
	uint16_t* restrict dst_row = (uint16_t*)dst;
	for (int y=0; y<h; y+=2) {
		memcpy(dst_row, src_row, cpy_pitch);
		dst_row += dst_stride;
		src_row += src_stride;
		
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *(src_row + x);
			*(dst_row + x) = Weight3_1(s, k);
		}
	}
	
}
static void scale2x(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 2;
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *src_row;
			
			// row 1
			*(dst_row     ) = s;
			*(dst_row + 1 ) = s;
			
			// row 2
			*(dst_row + screen_w    ) = s;
			*(dst_row + screen_w + 1) = s;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
static void scale2x_lcd(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 2;
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *src_row;
            uint16_t r = (s & 0b1111100000000000);
            uint16_t g = (s & 0b0000011111100000);
            uint16_t b = (s & 0b0000000000011111);
			
			*(dst_row                       ) = r;
			*(dst_row + 1                   ) = b;
			
			*(dst_row + screen_w * 1    ) = g;
			*(dst_row + screen_w * 1 + 1) = k;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
static void scale2x_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 2;
		for (unsigned x = 0; x < w; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			*(dst_row     ) = c1;
			*(dst_row + 1 ) = c1;
			
			*(dst_row + screen_w    ) = c2;
			*(dst_row + screen_w + 1) = c2;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
static void scale2x_grid(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 2;
		for (unsigned x = 0; x < w; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_1( c1, k);
			
			*(dst_row     ) = c2;
			*(dst_row + 1 ) = c2;
			
			*(dst_row + screen_w    ) = c2;
			*(dst_row + screen_w + 1) = c1;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
static void scale3x(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	int row3 = screen_w * 2;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 3;
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *src_row;
			
			// row 1
			*(dst_row    ) = s;
			*(dst_row + 1) = s;
			*(dst_row + 2) = s;
			
			// row 2
			*(dst_row + screen_w    ) = s;
			*(dst_row + screen_w + 1) = s;
			*(dst_row + screen_w + 2) = s;

			// row 3
			*(dst_row + row3    ) = s;
			*(dst_row + row3 + 1) = s;
			*(dst_row + row3 + 2) = s;

			src_row += 1;
			dst_row += 3;
		}
	}
}
static void scale3x_lcd(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	int row3 = screen_w * 2;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 3;
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *src_row;
            uint16_t r = (s & 0b1111100000000000);
            uint16_t g = (s & 0b0000011111100000);
            uint16_t b = (s & 0b0000000000011111);
			
			// row 1
			*(dst_row    ) = k;
			*(dst_row + 1) = g;
			*(dst_row + 2) = k;
			
			// row 2
			*(dst_row + screen_w    ) = r;
			*(dst_row + screen_w + 1) = g;
			*(dst_row + screen_w + 2) = b;

			// row 3
			*(dst_row + row3    ) = r;
			*(dst_row + row3 + 1) = k;
			*(dst_row + row3 + 2) = b;

			src_row += 1;
			dst_row += 3;
		}
	}
}
static void scale3x_dmg(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t g = 0xffff;
	int row3 = screen_w * 2;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 3;
		for (unsigned x = 0; x < w; x++) {
			uint16_t a = *src_row;
            uint16_t b = Weight3_2( a, g);
            uint16_t c = Weight2_3( a, g);
			
			// row 1
			*(dst_row    ) = b;
			*(dst_row + 1) = a;
			*(dst_row + 2) = a;
			
			// row 2
			*(dst_row + screen_w    ) = b;
			*(dst_row + screen_w + 1) = a;
			*(dst_row + screen_w + 2) = a;

			// row 3
			*(dst_row + row3    ) = c;
			*(dst_row + row3 + 1) = b;
			*(dst_row + row3 + 2) = b;

			src_row += 1;
			dst_row += 3;
		}
	}
}
static void scale3x_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 3;
		for (unsigned x = 0; x < w; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row                       ) = c2;
			*(dst_row                    + 1) = c2;
			*(dst_row                    + 2) = c2;

			// row 2
			*(dst_row + screen_w * 1    ) = c1;
			*(dst_row + screen_w * 1 + 1) = c1;
			*(dst_row + screen_w * 1 + 2) = c1;

			// row 3
			*(dst_row + screen_w * 2    ) = c1;
			*(dst_row + screen_w * 2 + 1) = c1;
			*(dst_row + screen_w * 2 + 2) = c1;

			src_row += 1;
			dst_row += 3;
		}
	}
}
static void scale3x_grid(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 3;
		for (unsigned x = 0; x < w; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			uint16_t c3 = Weight2_3( c1, k);
			
			// row 1
			*(dst_row                       ) = c2;
			*(dst_row                    + 1) = c1;
			*(dst_row                    + 2) = c1;

			// row 2
			*(dst_row + screen_w * 1    ) = c2;
			*(dst_row + screen_w * 1 + 1) = c1;
			*(dst_row + screen_w * 1 + 2) = c1;

			// row 3
			*(dst_row + screen_w * 2    ) = c3;
			*(dst_row + screen_w * 2 + 1) = c2;
			*(dst_row + screen_w * 2 + 2) = c2;

			src_row += 1;
			dst_row += 3;
		}
	}
}
static void scale4x(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	int row3 = screen_w * 2;
	int row4 = screen_w * 3;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 4;
		for (unsigned x = 0; x < w; x++) {
			uint16_t s = *src_row;
			
			// row 1
			*(dst_row    ) = s;
			*(dst_row + 1) = s;
			*(dst_row + 2) = s;
			*(dst_row + 3) = s;
			
			// row 2
			*(dst_row + screen_w    ) = s;
			*(dst_row + screen_w + 1) = s;
			*(dst_row + screen_w + 2) = s;
			*(dst_row + screen_w + 3) = s;

			// row 3
			*(dst_row + row3    ) = s;
			*(dst_row + row3 + 1) = s;
			*(dst_row + row3 + 2) = s;
			*(dst_row + row3 + 3) = s;

			// row 4
			*(dst_row + row4    ) = s;
			*(dst_row + row4 + 1) = s;
			*(dst_row + row4 + 2) = s;
			*(dst_row + row4 + 3) = s;

			src_row += 1;
			dst_row += 4;
		}
	}
}
static void scale4x_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	int screen_w = screen->w;
	int row3 = screen_w * 2;
	int row4 = screen_w * 3;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < h; y++) {
		uint16_t* restrict src_row = (void*)src + y * pitch;
		uint16_t* restrict dst_row = (void*)dst + y * dst_pitch * 4;
		for (unsigned x = 0; x < w; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row    ) = c1;
			*(dst_row + 1) = c1;
			*(dst_row + 2) = c1;
			*(dst_row + 3) = c1;
			
			// row 2
			*(dst_row + screen_w    ) = c2;
			*(dst_row + screen_w + 1) = c2;
			*(dst_row + screen_w + 2) = c2;
			*(dst_row + screen_w + 3) = c2;

			// row 3
			*(dst_row + row3    ) = c1;
			*(dst_row + row3 + 1) = c1;
			*(dst_row + row3 + 2) = c1;
			*(dst_row + row3 + 3) = c1;

			// row 4
			*(dst_row + row4    ) = c2;
			*(dst_row + row4 + 1) = c2;
			*(dst_row + row4 + 2) = c2;
			*(dst_row + row4 + 3) = c2;

			src_row += 1;
			dst_row += 4;
		}
	}
}

static void scaleNN_WORKING(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	LOG_info("scaleNN_WORKING(%p,%p,%i,%i,%i,%i,%i,%i)\n", src,dst,w,h,pitch,dst_w,dst_h,dst_pitch);
	
	// TODO: I think all NN are broken on Smart because we're dealing with a mix of rotated and unrotated values...
	
	uint16_t* s = (uint16_t*)src;
	uint16_t* d = (uint16_t*)dst;
	
	int rw = dst_w;
	int rh = dst_h;

	int sp = pitch / FIXED_BPP;
	int dp = dst_pitch / FIXED_BPP;
	
	int mx = (w << 16) / rw;
	int my = (h << 16) / rh;
	int sx = 0;
	int sy = 0;
	int lr = -1;
	int sr = 0;
	int dr = 0;
	int cp = dp * FIXED_BPP;
	for (int dy=0; dy<rh; dy++) {
		sx = 0;
		sr = (sy >> 16) * sp;
		if (sr==lr) memcpy(d+dr,d+dr-dp,cp);
		else {
	        for (int dx=0; dx<rw; dx++) {
	            d[dr + dx] = s[sr + (sx >> 16)];
				sx += mx;
	        }
		}
		lr = sr;
		sy += my;
		dr += dp;
    }
}

// TODO: NN versions of scanline need updating to use blended scanlines (or maybe not for performance?) 
static void scaleNN(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// LOG_info("scaleNN(%p,%p,%i,%i,%i,%i,%i,%i)\n", src,dst,w,h,pitch,dst_w,dst_h,dst_pitch);
	
	int dy = -dst_h;
	unsigned lines = h;
	bool copy = false;
	size_t cpy_w = dst_w * FIXED_BPP;

	while (lines) {
		int dx = -dst_w;
		const uint16_t *psrc16 = src;
		uint16_t *pdst16 = dst;

		if (copy) {
			copy = false;
			memcpy(dst, dst - dst_pitch, cpy_w);
			dst += dst_pitch;
			dy += h;
		} else if (dy < 0) {
			int col = w;
			while(col--) {
				while (dx < 0) {
					*pdst16++ = *psrc16;
					dx += w;
				}

				dx -= dst_w;
				psrc16++;
			}

			dst += dst_pitch;
			dy += h;
		}

		if (dy >= 0) {
			dy -= dst_h;
			src += pitch;
			lines--;
		} else {
			copy = true;
		}
	}
}
static void scaleNN_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// LOG_info("scaleNN_scanline(%p,%p,%i,%i,%i,%i,%i,%i)\n", src,dst,w,h,pitch,dst_w,dst_h,dst_pitch);
	
	int dy = -dst_h;
	unsigned lines = h;
	int row = 0;
	
	while (lines) {
		int dx = -dst_w;
		const uint16_t *psrc16 = src;
		uint16_t *pdst16 = dst;
		
		if (row%2==0) {
			int col = w;
			while(col--) {
				while (dx < 0) {
					*pdst16 = *(pdst16 + dst_pitch) = *psrc16;
					pdst16 += 1;
					dx += w;
				}

				dx -= dst_w;
				psrc16++;
			}
		}

		dst += dst_pitch;
		dy += h;
				
		if (dy >= 0) {
			dy -= dst_h;
			src += pitch;
			lines--;
		}
		row += 1;
	}
}
static void scaleNN_text(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// LOG_info("scaleNN_text(%p,%p,%i,%i,%i,%i,%i,%i)\n", src,dst,w,h,pitch,dst_w,dst_h,dst_pitch);
	
	int dy = -dst_h;
	unsigned lines = h;
	bool copy = false;

	size_t cpy_w = dst_w * FIXED_BPP;
		
	int safe = w - 1; // don't look behind when there's nothing to see
	uint16_t l1,l2;
	while (lines) {
		int dx = -dst_w;
		const uint16_t *psrc16 = src;
		uint16_t *pdst16 = dst;
		l1 = l2 = 0x0;
		
		if (copy) {
			copy = false;
			memcpy(dst, dst - cpy_w, cpy_w);
			dst += dst_pitch;
			dy += h;
		} else if (dy < 0) {
			int col = w;
			while(col--) {
				int d = 0;
				if (col<safe && l1!=l2) {
					// https://stackoverflow.com/a/71086522/145965
					uint16_t r = (l1 >> 10) & 0x3E;
					uint16_t g = (l1 >> 5) & 0x3F;
					uint16_t b = (l1 << 1) & 0x3E;
					uint16_t luma = (r * 218) + (g * 732) + (b * 74);
					luma = (luma >> 10) + ((luma >> 9) & 1); // 0-63
					d = luma > 24;
				}

				uint16_t s = *psrc16;
				
				while (dx < 0) {
					*pdst16++ = d ? l1 : s;
					dx += w;

					l2 = l1;
					l1 = s;
					d = 0;
				}

				dx -= dst_w;
				psrc16++;
			}

			dst += dst_pitch;
			dy += h;
		}

		if (dy >= 0) {
			dy -= dst_h;
			src += pitch;
			lines--;
		} else {
			copy = true;
		}
	}
}
static void scaleNN_text_scanline(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_pitch) {
	// LOG_info("scaleNN_text_scanline(%p,%p,%i,%i,%i,%i,%i,%i)\n", src,dst,w,h,pitch,dst_w,dst_h,dst_pitch);
	
	int dy = -dst_h;
	unsigned lines = h;

	int row = 0;
	int safe = w - 1; // don't look behind when there's nothing to see
	uint16_t l1,l2;
	while (lines) {
		int dx = -dst_w;
		const uint16_t *psrc16 = src;
		uint16_t *pdst16 = dst;
		l1 = l2 = 0x0;
		
		if (row%2==0) {
			int col = w;
			while(col--) {
				int d = 0;
				if (col<safe && l1!=l2) {
					// https://stackoverflow.com/a/71086522/145965
					uint16_t r = (l1 >> 10) & 0x3E;
					uint16_t g = (l1 >> 5) & 0x3F;
					uint16_t b = (l1 << 1) & 0x3E;
					uint16_t luma = (r * 218) + (g * 732) + (b * 74);
					luma = (luma >> 10) + ((luma >> 9) & 1); // 0-63
					d = luma > 24;
				}

				uint16_t s = *psrc16;
				
				while (dx < 0) {
					*pdst16 = *(pdst16 + dst_pitch) = d ? l1 : s;
					pdst16 += 1;
					
					dx += w;

					l2 = l1;
					l1 = s;
					d = 0;
				}

				dx -= dst_w;
				psrc16 += 1;
			}
		}

		dst += dst_pitch;
		dy += h;
				
		if (dy >= 0) {
			dy -= dst_h;
			src += pitch;
			lines--;
		}
		row += 1;
	}
}

static SDL_Surface* scaler_surface;
static void selectScaler_PAR(int width, int height, int pitch) {
	LOG_info("selectScaler_PAR\n");
	
	renderer.blit = scaleNull;
	renderer.src_w = width;
	renderer.src_h = height;
	renderer.src_p = pitch;
	renderer.dst_p = FIXED_PITCH;

	int use_nearest = 0;
	
	int scale_x = FIXED_WIDTH / width;
	int scale_y = FIXED_HEIGHT / height;
	int scale = MIN(scale_x,scale_y);
	
	// this is not an aspect ratio but rather the ratio between 
	// the proposed aspect ratio and the target aspect ratio
	double near_ratio = (double)width / height / core.aspect_ratio;
	// TODO: not sure I like this anymore
	#define ACCEPTABLE_UPPER_BOUNDS 1.14 // catch SotN/FFVII's 368x224 as needing nn scaling 
	#define ACCEPTABLE_LOWER_BOUNDS 0.79 // but allow SotN's 512x240 scaled 1x2 to pass as core's 4/3 aspect ratio
	
	char scaler_name[8];
	
	// fixed to allow things like 640x478 to pass through as 1x instead of NN_C
	if (scale<=1 && (near_ratio<ACCEPTABLE_LOWER_BOUNDS || near_ratio>ACCEPTABLE_UPPER_BOUNDS)) {
		LOG_info("nearest\n");
		scale = scale_x>0 || scale_y>0;
		use_nearest = 1;
		// TODO: handle unscaled size too big for FIXED_* size
		if (scale_y>scale_x) {
			strcpy(scaler_name, "NN_A");
			// PS: SotN/FFVII (menus 368x224)
			renderer.dst_h = height * scale_y;
			
			// if the aspect ratio of an unmodified
			// w to dst_h is within an acceptable range
			// of the target aspect_ratio don't force
			near_ratio = (double)width / renderer.dst_h / core.aspect_ratio;
			if (near_ratio>=ACCEPTABLE_LOWER_BOUNDS && near_ratio<=ACCEPTABLE_UPPER_BOUNDS) {
				renderer.dst_w = width; // close enough (eg. SotN 512x240 logo over moon in intro)
			}
			else {
				renderer.dst_w = renderer.dst_h * core.aspect_ratio;
				renderer.dst_w -= renderer.dst_w % 2;
			}
			
			if (renderer.dst_w>FIXED_WIDTH) {
				renderer.dst_w = FIXED_WIDTH;
				renderer.dst_h = renderer.dst_w / core.aspect_ratio;
				renderer.dst_h -= renderer.dst_w % 2;
				if (renderer.dst_h>FIXED_HEIGHT) renderer.dst_h = FIXED_HEIGHT;
			}
		}
		else if (scale_x>scale_y) {
			strcpy(scaler_name, "NN_B");
			// PS: Cotton (loading screen, 320x480)
			renderer.dst_w = width * scale_x;
			
			// see above
			near_ratio = (double)renderer.dst_w / height / core.aspect_ratio;
			if (near_ratio>=ACCEPTABLE_LOWER_BOUNDS && near_ratio<=ACCEPTABLE_UPPER_BOUNDS) {
				renderer.dst_h = height; // close enough
			}
			else {
				renderer.dst_h = renderer.dst_w / core.aspect_ratio;
				renderer.dst_h -= renderer.dst_w % 2;
			}
		
			if (renderer.dst_h>FIXED_HEIGHT) {
				renderer.dst_h = FIXED_HEIGHT;
				renderer.dst_w = renderer.dst_h * core.aspect_ratio;
				renderer.dst_w -= renderer.dst_w % 2;
				if (renderer.dst_w>FIXED_WIDTH) renderer.dst_w = FIXED_WIDTH;
			}
		}
		else {
			strcpy(scaler_name, "NN_C");
			// PS: Tekken 3 (in-game, 368x480)
			renderer.dst_w = width * scale_x;
			renderer.dst_h = height * scale_y;
		
			// see above
			near_ratio = (double)renderer.dst_w / renderer.dst_h / core.aspect_ratio;
			if (near_ratio>=ACCEPTABLE_LOWER_BOUNDS && near_ratio<=ACCEPTABLE_UPPER_BOUNDS) {
				// close enough
			}
			else {
				if (renderer.dst_h>renderer.dst_w) {
					renderer.dst_w = renderer.dst_h * core.aspect_ratio;
					renderer.dst_w -= renderer.dst_w % 2;
				}
				else {
					renderer.dst_h = renderer.dst_w / core.aspect_ratio;
					renderer.dst_h -= renderer.dst_w % 2;
				}
			}
		
			if (renderer.dst_w>FIXED_WIDTH) {
				renderer.dst_w = FIXED_WIDTH;
			}
			if (renderer.dst_h>FIXED_HEIGHT) {
				renderer.dst_h = FIXED_HEIGHT;
			}
		}
	}
	
	if (scale==0) {
		sprintf(scaler_name, "NN0");
		LOG_info("downsample\n");
		use_nearest = 1;
		renderer.dst_h = FIXED_HEIGHT;
		renderer.dst_w = FIXED_HEIGHT * core.aspect_ratio;
		if (renderer.dst_w>FIXED_WIDTH) {
			renderer.dst_w = FIXED_WIDTH;
			renderer.dst_h = FIXED_WIDTH / core.aspect_ratio;
		}
	}
	else if (!use_nearest) {
		LOG_info("integer\n");
		
		// sane consoles :joy:
		renderer.dst_w = width * scale;
		renderer.dst_h = height * scale;
	}
	
	renderer.dst_x = (FIXED_WIDTH - renderer.dst_w) / 2;
	renderer.dst_y = (FIXED_HEIGHT - renderer.dst_h) / 2;

	LOG_info("%i,%i %ix%i (%i)\n", renderer.dst_x,renderer.dst_y,renderer.dst_w,renderer.dst_h,renderer.dst_p);

	if (use_nearest) 
		if (show_scanlines) renderer.blit = optimize_text ? scaleNN_text_scanline : scaleNN_scanline;
		else renderer.blit = optimize_text ? scaleNN_text : scaleNN;
	else {
		sprintf(scaler_name, "%iX", scale);
		if (show_scanlines) {
			switch (scale) {
				case 6: 	renderer.blit = scaleNN_scanline; break;
				case 5: 	renderer.blit = scaleNN_scanline; break;
				case 4: 	renderer.blit = scale4x_scanline; break;
				case 3: 	renderer.blit = scale3x_grid; break;
				case 2: 	renderer.blit = scale2x_scanline; break;
				default:	renderer.blit = scale1x_scanline; break;
			}
		}
		else {
			switch (scale) {
				case 6: 	renderer.blit = scale6x6_n16; break;
				case 5: 	renderer.blit = scale5x5_n16; break;
				case 4: 	renderer.blit = scale4x4_n16; break;
				case 3: 	renderer.blit = scale3x3_n16; break;
				case 2: 	renderer.blit = scale2x2_n16; break;
				default:	renderer.blit = scale1x1_n16; break;

				// my lesser scalers :sweat_smile:
				// case 4: 	renderer.blit = scale4x; break;
				// case 3: 	renderer.blit = scale3x; break;
				// case 3: 	renderer.blit = scale3x_dmg; break;
				// case 3: 	renderer.blit = scale3x_lcd; break;
				// case 3: 	renderer.blit = scale3x_scanline; break;
				// case 2: 	renderer.blit = scale2x; break;
				// case 2: 	renderer.blit = scale2x_lcd; break;
				// case 2: 	renderer.blit = scale2x_scanline; break;
				// default:	renderer.blit = scale1x; break;
			}
		}
	}
	//////////////////////////////
	
	// DEBUG HUD
	if (scaler_surface) SDL_FreeSurface(scaler_surface);
	scaler_surface = TTF_RenderUTF8_Blended(font.tiny, scaler_name, COLOR_WHITE);
	LOG_info("%s\n", scaler_name);
	
	screen = GFX_resize(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH);
}
static void selectScaler_AR(int width, int height, int pitch) {
	renderer.blit = scaleNull;
	renderer.src_w = width;
	renderer.src_h = height;
	renderer.src_p = pitch;
	
	int src_w = width;
	int src_h = height;
	
	int scale_x = CEIL_DIV(FIXED_WIDTH, src_w);
	int scale_y = CEIL_DIV(FIXED_HEIGHT,src_h);
	int scale = MAX(scale_x, scale_y);
	// scale = 1;
	
	// TODO: this "logic" is a disaster
	
	// if (scale>6) scale = 6;
	// else
	if (scale>2) scale = 4; // TODO: pillar/letterboxing at 3x produces vertical banding (some kind of alignment issue?)

	// reduce scale if we don't have enough memory to accomodate it
	// scaled width and height can't be greater than our fixed page width or height
	// TODO: some resolutions are getting through here unadjusted? oh maybe because of aspect ratio adjustments below? revisit
	while (src_w * scale * FIXED_BPP * src_h * scale > PAGE_SIZE || src_w * scale > PAGE_WIDTH || src_h * scale > PAGE_HEIGHT) {
		scale -= 1;
	}
	
	int dst_w = src_w * scale;
	int dst_h = src_h * scale;
	int target_w = dst_w;
	int target_h = dst_h;
	
	char scaler_name[8];
	double target_ratio = 4.0f / 3;
	
	// TODO: sotn moon is busted 512x240 ends up 2x but targeting 864x480
	
	if (screen_scaling==1) {
		sprintf(scaler_name, "AR_%iX%s", scale, "R");
		if (core.aspect_ratio==target_ratio) {
			LOG_info("already correct ratio\n");
		}
		else {
			// TODO: is this ignoring the core's desired aspect ratio?
			// see snes or SotN
			// I think it's acting more like PAR
			sprintf(scaler_name, "AR_%iX%sR", scale, "R");
			
			int ratio_left  = 4;
			int ratio_right = 3;
			
			// TODO: why do these use CEIL_DIV?
			if (core.aspect_ratio<target_ratio) {
				// LOG_info("pillarbox\n");
				target_w = CEIL_DIV(src_h, ratio_right) * ratio_left * scale;
				target_h = src_h * scale;
			}
			else if (core.aspect_ratio>target_ratio) {
				// LOG_info("letterbox\n");
				target_w = src_w * scale;
				target_h = CEIL_DIV(src_w, ratio_left) * ratio_right * scale;
			}
			
			// TODO: 
			if (target_w > PAGE_WIDTH) {
				target_w = PAGE_WIDTH;
				target_h = CEIL_DIV(PAGE_WIDTH, ratio_left) * ratio_right;
				
				if (dst_h > target_h) {
					scale -= 1;
					dst_w = src_w * scale;
					dst_h = src_h * scale;
					
					// adjust again... (copy/pasted with modifications from above)
					if (core.aspect_ratio<target_ratio) {
						target_w = CEIL_DIV(dst_h, ratio_right) * ratio_left;
						target_h = src_h * scale;
					}
					else if (core.aspect_ratio>target_ratio) {
						target_w = src_w * scale;
						target_h = CEIL_DIV(dst_w, ratio_left) * ratio_right;
					}
					sprintf(scaler_name, "AR_%iXU", scale);
				}
				else sprintf(scaler_name, "AR_%iXD", scale);

				LOG_warn("target: %ix%i (%i) page: %ix%i (%i) \n", target_w,target_h, target_w*target_h*FIXED_BPP, PAGE_WIDTH,PAGE_HEIGHT,PAGE_SIZE);
			}
		}
	}
	else {
		sprintf(scaler_name, "FS_%iX", scale);
	}
	
	if (target_w%2) target_w += 1;
	if (target_h%2) target_h += 1;

	int dx = (target_w - dst_w) / 2;
	int dy = (target_h - dst_h) / 2;
	
	// TODO: this masks a larger problem above
	if (dx<0) dx = 0;
	if (dy<0) dy = 0;
	
	int target_pitch = target_w * FIXED_BPP;
	
	renderer.dst_w = target_w;
	renderer.dst_h = target_h;
	renderer.dst_p = target_pitch;
	renderer.dst_x = dx;
	renderer.dst_y = dy;
	
	switch (scale) {
		case 6: renderer.blit = scale6x6_n16; break;
		case 5: renderer.blit = scale5x5_n16; break;
		case 4: renderer.blit = scale4x4_n16; break;
		case 3: renderer.blit = scale3x3_n16; break;
		case 2: renderer.blit = scale2x2_n16; break;
		default: renderer.blit = scale1x1_n16; break;
	}
	
	// DEBUG HUD
	if (scaler_surface) SDL_FreeSurface(scaler_surface);
	scaler_surface = TTF_RenderUTF8_Blended(font.tiny, scaler_name, COLOR_WHITE);
	
	screen = GFX_resize(target_w,target_h, target_pitch);
}
static void video_refresh_callback(const void *data, unsigned width, unsigned height, size_t pitch) {
	static uint32_t last_flip_time = 0;
	
	// 10 seems to be the sweet spot that allows 2x in NES and SNES and 8x in GB at 60fps
	// 14 will let GB hit 10x but NES and SNES will drop to 1.5x at 30fps (not sure why)
	// but 10 hurts PS...
	// TODO: 10 was based on rg35xx, probably different results on other supported platforms
	if (fast_forward && SDL_GetTicks()-last_flip_time<10) return;
	
	// FFVII menus 
	// 16: 30/200
	// 15: 30/180
	// 14: 45/180
	// 12: 30/150
	// 10: 30/120 (optimize text off has no effect)
	//  8: 60/210 (with optimize text off)
	// you can squeeze more out of every console by turning prevent tearing off
	// eg. PS@10 60/240
	
	if (!data) return;

	fps_ticks += 1;
	
	if (renderer.dst_p==0 || width!=renderer.src_w || height!=renderer.src_h) {
		
		// TODO: merge selectScaler_* into a single updateRenderer(width,height,pitch,resize_screen)
		(screen_scaling==SCALE_NATIVE ? selectScaler_PAR : selectScaler_AR)(width,height,pitch);
		GFX_clearAll();
	}
	
	static int top_width = 0;
	static int bottom_width = 0;
	
	if (top_width) SDL_FillRect(screen, &(SDL_Rect){0,0,top_width,DIGIT_HEIGHT}, RGB_BLACK);
	if (bottom_width) SDL_FillRect(screen, &(SDL_Rect){0,screen->h-DIGIT_HEIGHT,bottom_width,DIGIT_HEIGHT}, RGB_BLACK);
	
	// uint64_t start = getMicroseconds();
	// renderer.scaler((void*)data,screen->pixels+renderer.dst_offset,width,height,pitch,renderer.dst_p);
	// printf("scale: %ius\n", (int)(getMicroseconds()-start));
	
	renderer.src = (void*)data;
	renderer.dst = screen->pixels;
	GFX_blitRenderer(&renderer);
	
	if (show_debug) {
		int x = 0;
		int y = screen->h - DIGIT_HEIGHT;
		
		if (fps_double) x = MSG_blitDouble(fps_double, x,y);
		
		if (cpu_double) {
			x = MSG_blitChar(DIGIT_SLASH,x,y);
			x = MSG_blitDouble(cpu_double, x,y);
		}
		
		if (use_double) {
			x = MSG_blitChar(DIGIT_SPACE,x,y);
			x = MSG_blitDouble(use_double, x,y);
			x = MSG_blitChar(DIGIT_PERCENT,x,y);
		}
		
		if (x>bottom_width) bottom_width = x; // keep the largest width because triple buffer
		
		x = 0;
		y = 0;
		
		// src res
		x = MSG_blitInt(renderer.src_w,x,y);
		x = MSG_blitChar(DIGIT_X,x,y);
		x = MSG_blitInt(renderer.src_h,x,y);
		
		x = MSG_blitChar(DIGIT_SPACE,x,y);
		
		// dst res
		x = MSG_blitChar(DIGIT_OP,x,y);
		x = MSG_blitInt(renderer.dst_w,x,y);
		x = MSG_blitChar(DIGIT_X,x,y);
		x = MSG_blitInt(renderer.dst_h,x,y);
		x = MSG_blitChar(DIGIT_CP,x,y);
		x = MSG_blitChar(DIGIT_SPACE,x,y);
		
		if (scaler_surface) {
			SDL_FillRect(screen, &(SDL_Rect){x,y,scaler_surface->w,DIGIT_HEIGHT}, RGB_BLACK);
			SDL_BlitSurface(scaler_surface, NULL, screen, &(SDL_Rect){x,y+((DIGIT_HEIGHT - scaler_surface->h)/2)});
			x += DIGIT_WIDTH * 3;
		}
		
		if (x>top_width) top_width = x; // keep the largest width because triple buffer
	}
	
	GFX_flip(screen);
	last_flip_time = SDL_GetTicks();
}

///////////////////////////////

// NOTE: sound must be disabled for fast forward to work...
static void audio_sample_callback(int16_t left, int16_t right) {
	if (!fast_forward) SND_batchSamples(&(const SND_Frame){left,right}, 1);
}
static size_t audio_sample_batch_callback(const int16_t *data, size_t frames) { 
	if (!fast_forward) return SND_batchSamples((const SND_Frame*)data, frames);
	else return frames;
};

///////////////////////////////////////

void Core_getName(char* in_name, char* out_name) {
	strcpy(out_name, basename(in_name));
	char* tmp = strrchr(out_name, '_');
	tmp[0] = '\0';
}
void Core_open(const char* core_path, const char* tag_name) {
	LOG_info("Core_open\n");
	core.handle = dlopen(core_path, RTLD_LAZY);
	
	if (!core.handle) LOG_error("%s\n", dlerror());
	
	core.init = dlsym(core.handle, "retro_init");
	core.deinit = dlsym(core.handle, "retro_deinit");
	core.get_system_info = dlsym(core.handle, "retro_get_system_info");
	core.get_system_av_info = dlsym(core.handle, "retro_get_system_av_info");
	core.set_controller_port_device = dlsym(core.handle, "retro_set_controller_port_device");
	core.reset = dlsym(core.handle, "retro_reset");
	core.run = dlsym(core.handle, "retro_run");
	core.serialize_size = dlsym(core.handle, "retro_serialize_size");
	core.serialize = dlsym(core.handle, "retro_serialize");
	core.unserialize = dlsym(core.handle, "retro_unserialize");
	core.load_game = dlsym(core.handle, "retro_load_game");
	core.load_game_special = dlsym(core.handle, "retro_load_game_special");
	core.unload_game = dlsym(core.handle, "retro_unload_game");
	core.get_region = dlsym(core.handle, "retro_get_region");
	core.get_memory_data = dlsym(core.handle, "retro_get_memory_data");
	core.get_memory_size = dlsym(core.handle, "retro_get_memory_size");
	
	void (*set_environment_callback)(retro_environment_t);
	void (*set_video_refresh_callback)(retro_video_refresh_t);
	void (*set_audio_sample_callback)(retro_audio_sample_t);
	void (*set_audio_sample_batch_callback)(retro_audio_sample_batch_t);
	void (*set_input_poll_callback)(retro_input_poll_t);
	void (*set_input_state_callback)(retro_input_state_t);
	
	set_environment_callback = dlsym(core.handle, "retro_set_environment");
	set_video_refresh_callback = dlsym(core.handle, "retro_set_video_refresh");
	set_audio_sample_callback = dlsym(core.handle, "retro_set_audio_sample");
	set_audio_sample_batch_callback = dlsym(core.handle, "retro_set_audio_sample_batch");
	set_input_poll_callback = dlsym(core.handle, "retro_set_input_poll");
	set_input_state_callback = dlsym(core.handle, "retro_set_input_state");
	
	struct retro_system_info info = {};
	core.get_system_info(&info);
	
	Core_getName((char*)core_path, (char*)core.name);
	sprintf((char*)core.version, "%s (%s)", info.library_name, info.library_version);
	strcpy((char*)core.tag, tag_name);
	strcpy((char*)core.extensions, info.valid_extensions);
	
	core.need_fullpath = info.need_fullpath;
	
	LOG_info("core: %s version: %s tag: %s (valid_extensions: %s need_fullpath: %i)\n", core.name, core.version, core.tag, info.valid_extensions, info.need_fullpath);
	
	sprintf((char*)core.config_dir, USERDATA_PATH "/%s-%s", core.tag, core.name);
	sprintf((char*)core.states_dir, SHARED_USERDATA_PATH "/%s-%s", core.tag, core.name);
	sprintf((char*)core.saves_dir, SDCARD_PATH "/Saves/%s", core.tag);
	sprintf((char*)core.bios_dir, SDCARD_PATH "/Bios/%s", core.tag);
	
	char cmd[512];
	sprintf(cmd, "mkdir -p \"%s\"; mkdir -p \"%s\"", core.config_dir, core.states_dir);
	system(cmd);

	set_environment_callback(environment_callback);
	set_video_refresh_callback(video_refresh_callback);
	set_audio_sample_callback(audio_sample_callback);
	set_audio_sample_batch_callback(audio_sample_batch_callback);
	set_input_poll_callback(input_poll_callback);
	set_input_state_callback(input_state_callback);
}
void Core_init(void) {
	LOG_info("Core_init\n");
	core.init();
	core.initialized = 1;
}
void Core_load(void) {
	LOG_info("Core_load\n");
	struct retro_game_info game_info;
	game_info.path = game.tmp_path[0]?game.tmp_path:game.path;
	game_info.data = game.data;
	game_info.size = game.size;
	
	core.load_game(&game_info);
	
	SRAM_read();
	
	// NOTE: must be called after core.load_game!
	struct retro_system_av_info av_info = {};
	core.get_system_av_info(&av_info);
	
	core.fps = av_info.timing.fps;
	core.sample_rate = av_info.timing.sample_rate;
	double a = av_info.geometry.aspect_ratio;
	if (a<=0) a = (double)av_info.geometry.base_width / av_info.geometry.base_height;
	core.aspect_ratio = a;
	
	LOG_info("aspect_ratio: %f fps: %f\n", a, core.fps);
}
void Core_reset(void) {
	core.reset();
}
void Core_unload(void) {
	SND_quit();
}
void Core_quit(void) {
	if (core.initialized) {
		SRAM_write();
		core.unload_game();
		core.deinit();
		core.initialized = 0;
	}
}
void Core_close(void) {
	if (core.handle) dlclose(core.handle);
}

///////////////////////////////////////

#define MENU_ITEM_COUNT 5
#define MENU_SLOT_COUNT 8

enum {
	ITEM_CONT,
	ITEM_SAVE,
	ITEM_LOAD,
	ITEM_OPTS,
	ITEM_QUIT,
};

enum {
	STATUS_CONT =  0,
	STATUS_SAVE =  1,
	STATUS_LOAD = 11,
	STATUS_OPTS = 23,
	STATUS_DISC = 24,
	STATUS_QUIT = 30
};

static struct {
	int initialized;
	SDL_Surface* overlay;
	char* items[MENU_ITEM_COUNT];
	int slot;
} menu = {
	.items = {
		[ITEM_CONT] = "Continue",
		[ITEM_SAVE] = "Save",
		[ITEM_LOAD] = "Load",
		[ITEM_OPTS] = "Options",
		[ITEM_QUIT] = "Quit",
	}
};

void Menu_init(void) {
	menu.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE,FIXED_WIDTH,FIXED_HEIGHT,FIXED_DEPTH,RGBA_MASK_AUTO);
	SDL_SetAlpha(menu.overlay, SDL_SRCALPHA, 0x80);
	SDL_FillRect(menu.overlay, NULL, 0);
}
void Menu_quit(void) {
	SDL_FreeSurface(menu.overlay);
}
void Menu_beforeSleep(void) {
	SRAM_write();
	State_autosave();
	putFile(AUTO_RESUME_PATH, game.path + strlen(SDCARD_PATH));
	POW_setCPUSpeed(CPU_SPEED_MENU);
}
void Menu_afterSleep(void) {
	unlink(AUTO_RESUME_PATH);
	setOverclock(overclock);
	// POW_setCPUSpeed(CPU_SPEED_NORMAL);
}

typedef struct MenuList MenuList;
typedef struct MenuItem MenuItem;
enum {
	MENU_CALLBACK_NOP,
	MENU_CALLBACK_EXIT,
	MENU_CALLBACK_NEXT_ITEM,
};
typedef int (*MenuList_callback_t)(MenuList* list, int i);
typedef struct MenuItem {
	char* name;
	char* desc;
	char** values;
	char* key; // optional, used by options
	int id; // optional, used by bindings
	int value;
	MenuList* submenu;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
} MenuItem;

enum {
	MENU_LIST, // eg. save and main menu
	MENU_VAR, // eg. frontend
	MENU_FIXED, // eg. emulator
	MENU_INPUT, // eg. renders like but MENU_VAR but handles input differently
};
typedef struct MenuList {
	int type;
	int max_width; // cached on first draw
	char* desc;
	MenuItem* items;
	MenuList_callback_t on_confirm;
	MenuList_callback_t on_change;
} MenuList;

static int Menu_message(char* message, char** pairs) {
	GFX_setMode(MODE_MAIN);
	int dirty = 1;
	while (1) {
		GFX_startFrame();
		PAD_poll();

		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) break;
		
		POW_update(&dirty, NULL, Menu_beforeSleep, Menu_afterSleep);
		
		if (dirty) {
			GFX_clear(screen);
			GFX_blitMessage(font.medium, message, screen, &(SDL_Rect){0,SCALE1(PADDING),screen->w,screen->h-SCALE1(PILL_SIZE+PADDING)});
			GFX_blitButtonGroup(pairs, screen, 1);
			GFX_flip(screen);
			dirty = 0;
		}
		else GFX_sync();
	}
	GFX_setMode(MODE_MENU);
	return MENU_CALLBACK_NOP; // TODO: this should probably be an arg
}

#define OPTION_PADDING 8
#define MAX_VISIBLE_OPTIONS 7
static int Menu_options(MenuList* list);

static int MenuList_freeItems(MenuList* list, int i) {
	// TODO: what calls this? do menu's register for needing it? then call it on quit for each?
	if (list->items) free(list->items);
	return MENU_CALLBACK_NOP;
}

static int OptionFrontend_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Config_syncFrontend(i, item->value);
	return MENU_CALLBACK_NOP;
}
static MenuList OptionFrontend_menu = {
	.type = MENU_VAR,
	.on_change = OptionFrontend_optionChanged,
	.items = NULL,
};
static int OptionFrontend_openMenu(MenuList* list, int i) {
	if (OptionFrontend_menu.items==NULL) {
		// TODO: where do I free this?
		OptionFrontend_menu.items = calloc(config.frontend.count+1, sizeof(MenuItem));
		for (int j=0; j<config.frontend.count; j++) {
			Option* option = &config.frontend.options[j];
			MenuItem* item = &OptionFrontend_menu.items[j];
			item->key = option->key;
			item->name = option->name;
			item->desc = option->desc;
			item->value = option->value;
			item->values = option->labels;
		}
	}
	else {
		// update values
		for (int j=0; j<config.frontend.count; j++) {
			Option* option = &config.frontend.options[j];
			MenuItem* item = &OptionFrontend_menu.items[j];
			item->value = option->value;
		}
		
	}
	Menu_options(&OptionFrontend_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionEmulator_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Option* option = OptionList_getOption(&config.core, item->key);
	LOG_info("%s (%s) changed from `%s` (%s) to `%s` (%s)\n", item->name, item->key, 
		item->values[option->value], option->values[option->value], 
		item->values[item->value], option->values[item->value]
	);
	OptionList_setOptionRawValue(&config.core, item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static int OptionEmulator_optionDetail(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Option* option = OptionList_getOption(&config.core, item->key);
	if (option->full) return Menu_message(option->full, (char*[]){ "B","BACK", NULL });
	else return MENU_CALLBACK_NOP;
}
static MenuList OptionEmulator_menu = {
	.type = MENU_FIXED,
	.on_confirm = OptionEmulator_optionDetail, // TODO: this needs pagination to be truly useful
	.on_change = OptionEmulator_optionChanged,
	.items = NULL,
};
static int OptionEmulator_openMenu(MenuList* list, int i) {
	if (OptionEmulator_menu.items==NULL) {
		// TODO: where do I free this?
		
		if (!config.core.enabled_count) {
			int enabled_count = 0;
			for (int i=0; i<config.core.count; i++) {
				if (!config.core.options[i].lock) enabled_count += 1;
			}
			config.core.enabled_count = enabled_count;
			config.core.enabled_options = calloc(enabled_count+1, sizeof(Option*));
			int j = 0;
			for (int i=0; i<config.core.count; i++) {
				Option* item = &config.core.options[i];
				if (item->lock) continue;
				config.core.enabled_options[j] = item;
				j += 1;
			}
		}
		
		OptionEmulator_menu.items = calloc(config.core.enabled_count+1, sizeof(MenuItem));
		for (int j=0; j<config.core.enabled_count; j++) {
			Option* option = config.core.enabled_options[j];
			MenuItem* item = &OptionEmulator_menu.items[j];
			item->key = option->key;
			item->name = option->name;
			item->desc = option->desc;
			item->value = option->value;
			item->values = option->labels;
		}
	}
	else {
		// update values
		for (int j=0; j<config.core.enabled_count; j++) {
			Option* option = config.core.enabled_options[j];
			MenuItem* item = &OptionEmulator_menu.items[j];
			item->value = option->value;
		}
	}
	
	// try to handle no options
	// TODO: show a message
	if (OptionEmulator_menu.items[0].name) {
		Menu_options(&OptionEmulator_menu);
	}
	else {
		Menu_message("This core has no options.", (char*[]){ "B","BACK", NULL });
	}
	
	return MENU_CALLBACK_NOP;
}

int OptionControls_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.controls[item->id];
	
	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();
		
		// NOTE: off by one because of the initial NONE value
		for (int id=0; id<=LOCAL_BUTTON_COUNT; id++) {
			if (PAD_justPressed(1 << (id-1))) {
				item->value = id;
				button->local = id - 1;
				bound = 1;
				break;
			}
		}
		GFX_sync();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionControls_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.controls[item->id];
	button->local = -1;
	button->mod = 0;
	return MENU_CALLBACK_NOP;
}
static MenuList OptionControls_menu = {
	.type = MENU_INPUT,
	.desc = "Press A to set and X to clear.",
	.on_confirm = OptionControls_bind,
	.on_change = OptionControls_unbind,
	.items = NULL
};
static int OptionControls_openMenu(MenuList* list, int i) {
	LOG_info("OptionControls_openMenu\n");
	if (OptionControls_menu.items==NULL) {
		// TODO: where do I free this?
		OptionControls_menu.items = calloc(RETRO_BUTTON_COUNT+1, sizeof(MenuItem));
		int k = 0;
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
			
			LOG_info("\t%s (%i:%i)\n", button->name, button->local, button->retro);
			
			MenuItem* item = &OptionControls_menu.items[k++];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			item->values = button_labels;
		}
	}
	else {
		// update values
		int k = 0;
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
			
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = button->local + 1;
		}
	}
	Menu_options(&OptionControls_menu);
	return MENU_CALLBACK_NOP;
}

static int OptionShortcuts_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.shortcuts[item->id];
	int bound = 0;
	while (!bound) {
		GFX_startFrame();
		PAD_poll();
		
		// NOTE: off by one because of the initial NONE value
		for (int id=0; id<=LOCAL_BUTTON_COUNT; id++) {
			if (PAD_justPressed(1 << (id-1))) {
				fflush(stdout);
				item->value = id;
				button->local = id - 1;
				if (PAD_isPressed(BTN_MENU)) {
					item->value += LOCAL_BUTTON_COUNT;
					button->mod = 1;
				}
				else {
					button->mod = 0;
				}
				bound = 1;
				break;
			}
		}
		GFX_sync();
	}
	fflush(stdout);
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionShortcuts_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	ButtonMapping* button = &config.shortcuts[item->id];
	button->local = -1;
	button->mod = 0;
	return MENU_CALLBACK_NOP;
}
static MenuList OptionShortcuts_menu = {
	.type = MENU_INPUT,
	.desc = "Press A to set and X to clear.\nSupports single button and MENU+button.",
	.on_confirm = OptionShortcuts_bind,
	.on_change = OptionShortcuts_unbind,
	.items = NULL
};
static char* getSaveDesc(void) {
	switch (config.loaded) {
		case CONFIG_NONE:		return "Using defaults."; break;
		case CONFIG_CONSOLE:	return "Using console config."; break;
		case CONFIG_GAME:		return "Using game config."; break;
	}
	return NULL;
}
static int OptionShortcuts_openMenu(MenuList* list, int i) {
	if (OptionShortcuts_menu.items==NULL) {
		// TODO: where do I free this?
		OptionShortcuts_menu.items = calloc(SHORTCUT_COUNT+1, sizeof(MenuItem));
		for (int j=0; config.shortcuts[j].name; j++) {
			ButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
			item->values = shortcut_labels;
		}
	}
	else {
		// update values
		for (int j=0; config.shortcuts[j].name; j++) {
			ButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
		}
	}
	Menu_options(&OptionShortcuts_menu);
	return MENU_CALLBACK_NOP;
}

static void OptionSaveChanges_updateDesc(void);
static int OptionSaveChanges_onConfirm(MenuList* list, int i) {
	char* message;
	switch (i) {
		case 0: {
			Config_write(CONFIG_WRITE_ALL);
			message = "Saved for console.";
			break;
		}
		case 1: {
			Config_write(CONFIG_WRITE_GAME);
			message = "Saved for game.";
			break;
		}
		default: {
			Config_restore();
			if (config.loaded) message = "Restored console defaults.";
			else message = "Restored defaults.";
			break;
		}
	}
	Menu_message(message, (char*[]){ "A","OKAY", NULL });
	OptionSaveChanges_updateDesc();
	return MENU_CALLBACK_EXIT;
}
static MenuList OptionSaveChanges_menu = {
	.type = MENU_LIST,
	.on_confirm = OptionSaveChanges_onConfirm,
	.items = (MenuItem[]){
		{"Save for console"},
		{"Save for game"},
		{"Restore defaults"},
		{NULL},
	}
};
static int OptionSaveChanges_openMenu(MenuList* list, int i) {
	OptionSaveChanges_updateDesc();
	OptionSaveChanges_menu.desc = getSaveDesc();
	Menu_options(&OptionSaveChanges_menu);
	return MENU_CALLBACK_NOP;
}

static MenuList options_menu = {
	.type = MENU_LIST,
	.items = (MenuItem[]) {
		{"Frontend", "MinUI (" BUILD_DATE " " BUILD_HASH ")",.on_confirm=OptionFrontend_openMenu},
		{"Emulator",.on_confirm=OptionEmulator_openMenu},
		{"Controls",.on_confirm=OptionControls_openMenu},
		{"Shortcuts",.on_confirm=OptionShortcuts_openMenu}, 
		{"Save Changes",.on_confirm=OptionSaveChanges_openMenu},
		{NULL},
	}
};

static void OptionSaveChanges_updateDesc(void) {
	options_menu.items[4].desc = getSaveDesc();
}

static int Menu_options(MenuList* list) {
	MenuItem* items = list->items;
	int type = list->type;

	int dirty = 1;
	int show_options = 1;
	int show_settings = 0;
	int await_input = 0;
	
	int count;
	for (count=0; items[count].name; count++);
	int selected = 0;
	int start = 0;
	int end = MIN(count,MAX_VISIBLE_OPTIONS);
	int visible_rows = end;
	
	OptionSaveChanges_updateDesc();
	
	while (show_options) {
		if (await_input) {
			list->on_confirm(list, selected);
			
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
			await_input = false;
		}
		
		GFX_startFrame();
		PAD_poll();
		
		if (PAD_justRepeated(BTN_UP)) {
			selected -= 1;
			if (selected<0) {
				selected = count - 1;
				start = MAX(0,count - MAX_VISIBLE_OPTIONS);
				end = count;
			}
			else if (selected<start) {
				start -= 1;
				end -= 1;
			}
			dirty = 1;
		}
		else if (PAD_justRepeated(BTN_DOWN)) {
			selected += 1;
			if (selected>=count) {
				selected = 0;
				start = 0;
				end = visible_rows;
			}
			else if (selected>=end) {
				start += 1;
				end += 1;
			}
			dirty = 1;
		}
		else if (type!=MENU_INPUT && type!=MENU_LIST) {
			if (PAD_justRepeated(BTN_LEFT)) {
				MenuItem* item = &items[selected];
				if (item->value>0) item->value -= 1;
				else {
					int j;
					for (j=0; item->values[j]; j++);
					item->value = j - 1;
				}
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				dirty = 1;
			}
			else if (PAD_justRepeated(BTN_RIGHT)) {
				MenuItem* item = &items[selected];
				if (item->values[item->value+1]) item->value += 1;
				else item->value = 0;
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				dirty = 1;
			}
		}
		
		// uint32_t now = SDL_GetTicks();
		if (PAD_justPressed(BTN_B)) { // || PAD_tappedMenu(now)
			show_options = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			MenuItem* item = &items[selected];
			int result = MENU_CALLBACK_NOP;
			if (item->on_confirm) result = item->on_confirm(list, selected); // item-specific action, eg. Save for all games
			else if (item->submenu) result = Menu_options(item->submenu); // drill down, eg. main options menu
			// TODO: is there a way to defer on_confirm for MENU_INPUT so we can clear the currently set value to indicate it is awaiting input? 
			// eg. set a flag to call on_confirm at the beginning of the next frame
			else if (list->on_confirm) {
				if (type==MENU_INPUT) await_input = 1;
				else result = list->on_confirm(list, selected); // list-specific action, eg. show item detail view or input binding
			}
			if (result==MENU_CALLBACK_EXIT) show_options = 0;
			else {
				if (result==MENU_CALLBACK_NEXT_ITEM) {
					// copied from PAD_justRepeated(BTN_DOWN) above
					selected += 1;
					if (selected>=count) {
						selected = 0;
						start = 0;
						end = visible_rows;
					}
					else if (selected>=end) {
						start += 1;
						end += 1;
					}
				}
				dirty = 1;
			}
		}
		else if (type==MENU_INPUT) {
			if (PAD_justPressed(BTN_X)) {
				MenuItem* item = &items[selected];
				item->value = 0;
				
				if (item->on_change) item->on_change(list, selected);
				else if (list->on_change) list->on_change(list, selected);
				
				// copied from PAD_justRepeated(BTN_DOWN) above
				selected += 1;
				if (selected>=count) {
					selected = 0;
					start = 0;
					end = visible_rows;
				}
				else if (selected>=end) {
					start += 1;
					end += 1;
				}
				dirty = 1;
			}
		}
		
		POW_update(&dirty, &show_settings, Menu_beforeSleep, Menu_afterSleep);
		
		if (dirty) {
			GFX_clear(screen);
			GFX_blitHardwareGroup(screen, show_settings);
			
			char* desc = NULL;
			SDL_Surface* text;

			if (type==MENU_LIST) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest item
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						if (w>mw) mw = w;
					}
					// cache the result
					list->max_width = mw = MIN(mw, screen->w - SCALE1(PADDING *2));
				}
				
				int ox = (screen->w - mw) / 2;
				int oy = SCALE1(PADDING + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					// int ox = (screen->w - w) / 2; // if we're centering these (but I don't think we should after seeing it)
					if (j==selected_row) {
						// move out of conditional if centering
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
				}
			}
			else if (type==MENU_FIXED) {
				// NOTE: no need to calculate max width
				int mw = screen->w - SCALE1(PADDING*2);
				// int lw,rw;
				// lw = rw = mw / 2;
				int ox,oy;
				ox = oy = SCALE1(PADDING);
				oy += SCALE1(PILL_SIZE);
				
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
					}
					
					if (item->value>=0) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
					
					// TODO: blit a black pill on unselected rows (to cover longer item->values?) or truncate longer item->values?
					if (j==selected_row) {
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
				}
			}
			else if (type==MENU_VAR || type==MENU_INPUT) {
				int mw = list->max_width;
				if (!mw) {
					// get the width of the widest row
					int mrw = 0;
					for (int i=0; i<count; i++) {
						MenuItem* item = &items[i];
						int w = 0;
						int lw = 0;
						int rw = 0;
						TTF_SizeUTF8(font.small, item->name, &lw, NULL);
						
						// every value list in an input table is the same
						// so only calculate rw for the first item...
						if (!mrw || type!=MENU_INPUT) {
							for (int j=0; item->values[j]; j++) {
								TTF_SizeUTF8(font.tiny, item->values[j], &rw, NULL);
								if (lw+rw>w) w = lw+rw;
								if (rw>mrw) mrw = rw;
							}
						}
						else {
							w = lw + mrw;
						}
						w += SCALE1(OPTION_PADDING*4);
						if (w>mw) mw = w;
					}
					fflush(stdout);
					// cache the result
					list->max_width = mw = MIN(mw, screen->w - SCALE1(PADDING *2));
				}
				
				int ox = (screen->w - mw) / 2;
				int oy = SCALE1(PADDING + PILL_SIZE);
				int selected_row = selected - start;
				for (int i=start,j=0; i<end; i++,j++) {
					MenuItem* item = &items[i];
					SDL_Color text_color = COLOR_WHITE;

					if (j==selected_row) {
						// gray pill
						GFX_blitPill(ASSET_OPTION, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							mw,
							SCALE1(BUTTON_SIZE)
						});
						
						// white pill
						int w = 0;
						TTF_SizeUTF8(font.small, item->name, &w, NULL);
						w += SCALE1(OPTION_PADDING*2);
						GFX_blitPill(ASSET_BUTTON, screen, &(SDL_Rect){
							ox,
							oy+SCALE1(j*BUTTON_SIZE),
							w,
							SCALE1(BUTTON_SIZE)
						});
						text_color = COLOR_BLACK;
						
						if (item->desc) desc = item->desc;
					}
					text = TTF_RenderUTF8_Blended(font.small, item->name, text_color);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox+SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+1)
					});
					SDL_FreeSurface(text);
					
					if (await_input && j==selected_row) {
						// buh
					}
					else if (item->value>=0) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
				}
			}
			
			if (count>MAX_VISIBLE_OPTIONS) {
				#define SCROLL_WIDTH 24
				#define SCROLL_HEIGHT 4
				int ox = (screen->w - SCALE1(SCROLL_WIDTH))/2;
				int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
				if (start>0) GFX_blitAsset(ASSET_SCROLL_UP,   NULL, screen, &(SDL_Rect){ox, SCALE1(PADDING) + oy});
				if (end<count) GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen, &(SDL_Rect){ox, screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + oy});
			}
			
			if (!desc && list->desc) desc = list->desc;
			
			if (desc) {
				int w,h;
				GFX_sizeText(font.tiny, desc, SCALE1(12), &w,&h);
				GFX_blitText(font.tiny, desc, SCALE1(12), COLOR_WHITE, screen, &(SDL_Rect){
					(screen->w - w) / 2,
					screen->h - SCALE1(PADDING) - h,
					w,h
				});
			}
			
			GFX_flip(screen);
			dirty = 0;
		}
		else GFX_sync();
	}
	
	// GFX_clearAll();
	// GFX_flip(screen);
	
	return 0;
}

static void Menu_scale(SDL_Surface* src, SDL_Surface* dst) {
	uint16_t* s = src->pixels;
	uint16_t* d = dst->pixels;

	int sw = src->w;
	int sh = src->h;
	int sp = src->pitch / FIXED_BPP;

	int dw = dst->w;
	int dh = dst->h;
	int dp = dst->pitch / FIXED_BPP;
	
	int rx = 0;
	int ry = 0;
	int rw = dw;
	int rh = dh;
	
	if (screen_scaling==SCALE_NATIVE) {
		// LOG_info("native\n");
		rx = renderer.dst_x;
		ry = renderer.dst_y;
		rw = renderer.dst_w;
		rh = renderer.dst_h;
		
		if (dw==FIXED_WIDTH/2) {
			// LOG_info("adjusted\n");
			rx /=2;
			ry /=2;
			rw /=2;
			rh /=2;
		}
	}
	
	if (screen_scaling==SCALE_ASPECT || rw>dw || rh>dh) {
		// LOG_info("aspect\n");
		rw = dh * core.aspect_ratio;
		if (rw>dw) {
			// LOG_info("adjusted\n");
			rw = dw;
			rh = dw / core.aspect_ratio;
		}
		rx = (dw - rw) / 2;
		ry = (dh - rh) / 2;
	}
	// else {
	// 	LOG_info("full\n");
	// }
	
	// LOG_info("Menu_scale: %i,%i %ix%i\n",rx,ry,rw,rh);
	
	// TODO: apply screen effects/scanline?
	// for (int dy=0; dy<rh; dy++) {
	//         for (int dx=0; dx<rw; dx++) {
	//             int sx = dx * sw / rw;
	//             int sy = dy * sh / rh;
	//             d[(ry+dy) * dp + (rx+dx)] = s[sy * sp + sx];
	//         }
	//     }
	
	int mx = (sw << 16) / rw;
	int my = (sh << 16) / rh;
	int sx = 0;
	int sy = 0;
	int lr = -1;
	int sr = 0;
	int dr = ry * dp;
	int cp = dp * FIXED_BPP;
	for (int dy=0; dy<rh; dy++) {
		sx = 0;
		sr = (sy >> 16) * sp;
		if (sr==lr) {
			memcpy(d+dr,d+dr-dp,cp);
		}
		else {
	        for (int dx=0; dx<rw; dx++) {
	            d[dr + rx + dx] = s[sr + (sx >> 16)];
				sx += mx;
	        }
		}
		lr = sr;
		sy += my;
		dr += dp;
    }
	
	// LOG_info("successful\n");
}

static void Menu_loop(void) {
	SDL_Surface* bitmap = SDL_CreateRGBSurfaceFrom(renderer.src, renderer.src_w, renderer.src_h, FIXED_DEPTH, renderer.src_p, RGBA_MASK_565);
	
	// TODO: update backing if screen_scaling changes while in options menu
	// TODO: create this from a persistent FIXED_WIDTH,FIXED_HEIGHT buffer	
	SDL_Surface* backing = SDL_CreateRGBSurface(SDL_SWSURFACE,FIXED_WIDTH,FIXED_HEIGHT,FIXED_DEPTH,RGBA_MASK_565); 
	// LOG_info("intial backing\n");
	Menu_scale(bitmap, backing);
	
	int restore_w = screen->w;
	int restore_h = screen->h;
	int restore_p = screen->pitch;
	if (restore_w!=FIXED_WIDTH || restore_h!=FIXED_HEIGHT) {
		screen = GFX_resize(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH);
	}
	
	SRAM_write();
	POW_warn(0);
	POW_setCPUSpeed(CPU_SPEED_MENU); // set Hz directly
	GFX_setVsync(VSYNC_STRICT);
	
	int rumble_strength = VIB_getStrength();
	VIB_setStrength(0);
	
	fast_forward = 0; // TODO: I can't remember why I do this but I find it kinda annoying...
	POW_enableAutosleep();
	PAD_reset();
	
	// path and string things
	char* tmp;
	char rom_name[256]; // without extension or cruft
	char slot_path[256];
	char emu_name[256];
	char minui_dir[256];
		
	getEmuName(game.path, emu_name);
	sprintf(minui_dir, SHARED_USERDATA_PATH "/.minui/%s", emu_name);
	mkdir(minui_dir, 0755);
	
	int rom_disc = -1;
	int disc = rom_disc;
	int total_discs = 0;
	char disc_name[16];
	char* disc_paths[9]; // up to 9 paths, Arc the Lad Collection is 7 discs
	char base_path[256]; // used below too when status==kItemSave
	
	if (game.m3u_path[0]) {
		strcpy(base_path, game.m3u_path);
		tmp = strrchr(base_path, '/') + 1;
		tmp[0] = '\0';
		
		//read m3u file
		FILE* file = fopen(game.m3u_path, "r");
		if (file) {
			char line[256];
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines
		
				char disc_path[256];
				strcpy(disc_path, base_path);
				tmp = disc_path + strlen(disc_path);
				strcpy(tmp, line);
				
				// found a valid disc path
				if (exists(disc_path)) {
					disc_paths[total_discs] = strdup(disc_path);
					// matched our current disc
					if (exactMatch(disc_path, game.path)) {
						rom_disc = total_discs;
						disc = rom_disc;
						sprintf(disc_name, "Disc %i", disc+1);
					}
					total_discs += 1;
				}
			}
			fclose(file);
		}
	}
	
	// shares saves across multi-disc games too
	sprintf(slot_path, "%s/%s.txt", minui_dir, game.name);
	getDisplayName(game.name, rom_name);
	
	int selected = 0; // resets every launch
	if (exists(slot_path)) menu.slot = getInt(slot_path);
	if (menu.slot==8) menu.slot = 0;
	
	char save_path[256];
	char bmp_path[256];
	char txt_path[256];
	int save_exists = 0;
	int preview_exists = 0;
	
	int status = STATUS_CONT; // TODO: tmp?
	int show_setting = 0;
	int dirty = 1;
	int ignore_menu = 0;
	int menu_start = 0;
	
	SDL_Surface* preview = SDL_CreateRGBSurface(SDL_SWSURFACE,FIXED_WIDTH/2,FIXED_HEIGHT/2,FIXED_DEPTH,RGBA_MASK_565); // TODO: retain until changed?
	
	while (show_menu) {
		GFX_startFrame();
		uint32_t now = SDL_GetTicks();

		PAD_poll();
		
		if (PAD_justPressed(BTN_UP)) {
			selected -= 1;
			if (selected<0) selected += MENU_ITEM_COUNT;
			dirty = 1;
		}
		else if (PAD_justPressed(BTN_DOWN)) {
			selected += 1;
			if (selected>=MENU_ITEM_COUNT) selected -= MENU_ITEM_COUNT;
			dirty = 1;
		}
		else if (PAD_justPressed(BTN_LEFT)) {
			if (total_discs>1 && selected==ITEM_CONT) {
				disc -= 1;
				if (disc<0) disc += total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot -= 1;
				if (menu.slot<0) menu.slot += MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		else if (PAD_justPressed(BTN_RIGHT)) {
			if (total_discs>1 && selected==ITEM_CONT) {
				disc += 1;
				if (disc==total_discs) disc -= total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot += 1;
				if (menu.slot>=MENU_SLOT_COUNT) menu.slot -= MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		
		if (dirty && (selected==ITEM_SAVE || selected==ITEM_LOAD)) {
			int last_slot = state_slot;
			state_slot = menu.slot;
			State_getPath(save_path);
			state_slot = last_slot;
			sprintf(bmp_path, "%s/%s.%d.bmp", minui_dir, game.name, menu.slot);
			sprintf(txt_path, "%s/%s.%d.txt", minui_dir, game.name, menu.slot);
		
			save_exists = exists(save_path);
			preview_exists = save_exists && exists(bmp_path);
			// printf("save_path: %s (%i)\n", save_path, save_exists);
			// printf("bmp_path: %s (%i)\n", bmp_path, preview_exists);
		}
		
		
		if (PAD_justPressed(BTN_B) || PAD_tappedMenu(now)) {
			status = STATUS_CONT;
			show_menu = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			switch(selected) {
				case ITEM_CONT:
				if (total_discs && rom_disc!=disc) {
						status = STATUS_DISC;
						char* disc_path = disc_paths[disc];
						Game_changeDisc(disc_path);
					}
					else {
						status = STATUS_CONT;
					}
					show_menu = 0;
				break;
				
				case ITEM_SAVE: {
					state_slot = menu.slot;
					State_write();
					status = STATUS_SAVE;
					SDL_RWops* out = SDL_RWFromFile(bmp_path, "wb");
					if (total_discs) {
						char* disc_path = disc_paths[disc];
						putFile(txt_path, disc_path + strlen(base_path));
						sprintf(bmp_path, "%s/%s.%d.bmp", minui_dir, game.name, menu.slot);
					}
					SDL_SaveBMP_RW(bitmap, out, 1);
					putInt(slot_path, menu.slot);
					show_menu = 0;
				}
				break;
				case ITEM_LOAD: {
					if (save_exists && total_discs) {
						char slot_disc_name[256];
						getFile(txt_path, slot_disc_name, 256);
						char slot_disc_path[256];
						if (slot_disc_name[0]=='/') strcpy(slot_disc_path, slot_disc_name);
						else sprintf(slot_disc_path, "%s%s", base_path, slot_disc_name);
						char* disc_path = disc_paths[disc];
						if (!exactMatch(slot_disc_path, disc_path)) {
							Game_changeDisc(slot_disc_path);
						}
					}
					state_slot = menu.slot;
					State_read();
					status = STATUS_LOAD;
					putInt(slot_path, menu.slot);
					show_menu = 0;
				}
				break;
				case ITEM_OPTS: {
					int old_scaling = screen_scaling;
					Menu_options(&options_menu);
					if (screen_scaling!=old_scaling) {
						// update renderer data
						// TODO: src_w is set to 0 to trigger reselect so this will be invalid
						// LOG_info("update scaling %ix%i (%i)\n", renderer.src_w,renderer.src_h,renderer.src_p);
						(screen_scaling==SCALE_NATIVE ? selectScaler_PAR : selectScaler_AR)(renderer.src_w,renderer.src_h,renderer.src_p);
						
						restore_w = screen->w;
						restore_h = screen->h;
						restore_p = screen->pitch;
						screen = GFX_resize(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH);
						
						SDL_FillRect(backing, NULL, 0);
						Menu_scale(bitmap, backing);
					}
					dirty = 1;
				}
				break;
				case ITEM_QUIT:
					status = STATUS_QUIT;
					show_menu = 0;
					quit = 1; // TODO: tmp?
				break;
			}
			if (!show_menu) break;
		}

		POW_update(&dirty, &show_setting, Menu_beforeSleep, Menu_afterSleep);
		
		if (dirty) {
			GFX_clear(screen);
			
			SDL_BlitSurface(backing, NULL, screen, NULL);
			SDL_BlitSurface(menu.overlay, NULL, screen, NULL);

			int ox, oy;
			int ow = GFX_blitHardwareGroup(screen, show_setting);
			int max_width = screen->w - SCALE1(PADDING * 2) - ow;
			
			char display_name[256];
			int text_width = GFX_truncateText(font.large, rom_name, display_name, max_width, SCALE1(BUTTON_PADDING*2));
			max_width = MIN(max_width, text_width);

			SDL_Surface* text;
			text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);
			GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){
				SCALE1(PADDING),
				SCALE1(PADDING),
				max_width,
				SCALE1(PILL_SIZE)
			});
			SDL_BlitSurface(text, &(SDL_Rect){
				0,
				0,
				max_width-SCALE1(BUTTON_PADDING*2),
				text->h
			}, screen, &(SDL_Rect){
				SCALE1(PADDING+BUTTON_PADDING),
				SCALE1(PADDING+4)
			});
			SDL_FreeSurface(text);
			
			if (show_setting) GFX_blitHardwareHints(screen, show_setting);
			else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"COMBO","SLEEP", NULL }, screen, 0);
			GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OKAY", NULL }, screen, 1);
			
			// list
			oy = 35;
			for (int i=0; i<MENU_ITEM_COUNT; i++) {
				char* item = menu.items[i];
				SDL_Color text_color = COLOR_WHITE;
				
				if (i==selected) {
					// disc change
					if (total_discs>1 && i==ITEM_CONT) {				
						GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){
							SCALE1(PADDING),
							SCALE1(oy + PADDING),
							screen->w - SCALE1(PADDING * 2),
							SCALE1(PILL_SIZE)
						});
						text = TTF_RenderUTF8_Blended(font.large, disc_name, COLOR_WHITE);
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							screen->w - SCALE1(PADDING + BUTTON_PADDING) - text->w,
							SCALE1(oy + PADDING + 4)
						});
						SDL_FreeSurface(text);
					}
					
					TTF_SizeUTF8(font.large, item, &ow, NULL);
					ow += SCALE1(BUTTON_PADDING*2);
					
					// pill
					GFX_blitPill(ASSET_WHITE_PILL, screen, &(SDL_Rect){
						SCALE1(PADDING),
						SCALE1(oy + PADDING + (i * PILL_SIZE)),
						ow,
						SCALE1(PILL_SIZE)
					});
					text_color = COLOR_BLACK;
				}
				else {
					// shadow
					text = TTF_RenderUTF8_Blended(font.large, item, COLOR_BLACK);
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						SCALE1(2 + PADDING + BUTTON_PADDING),
						SCALE1(1 + PADDING + oy + (i * PILL_SIZE) + 4)
					});
					SDL_FreeSurface(text);
				}
				
				// text
				text = TTF_RenderUTF8_Blended(font.large, item, text_color);
				SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
					SCALE1(PADDING + BUTTON_PADDING),
					SCALE1(oy + PADDING + (i * PILL_SIZE) + 4)
				});
				SDL_FreeSurface(text);
			}
			
			// slot preview
			if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				#define WINDOW_RADIUS 4 // TODO: this logic belongs in blitRect?
				// unscaled
				ox = 146;
				oy = 54;
				int hw = FIXED_WIDTH / 2;
				int hh = FIXED_HEIGHT / 2;
				
				// window
				GFX_blitRect(ASSET_STATE_BG, screen, &(SDL_Rect){SCALE2(ox-WINDOW_RADIUS,oy-WINDOW_RADIUS),hw+SCALE1(WINDOW_RADIUS*2),hh+SCALE1(WINDOW_RADIUS*3+6)});
				
				if (preview_exists) { // has save, has preview
					// lotta memory churn here
					SDL_Surface* bmp = IMG_Load(bmp_path);
					SDL_Surface* raw_preview = SDL_ConvertSurface(bmp, screen->format, SDL_SWSURFACE);
					
					// LOG_info("preview\n");
					
					SDL_FillRect(preview, NULL, 0);
					Menu_scale(raw_preview, preview);
					SDL_BlitSurface(preview, NULL, screen, &(SDL_Rect){SCALE2(ox,oy)});
					SDL_FreeSurface(raw_preview);
					SDL_FreeSurface(bmp);
				}
				else {
					SDL_Rect preview_rect = {SCALE2(ox,oy),hw,hh};
					SDL_FillRect(screen, &preview_rect, 0);
					if (save_exists) GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					else GFX_blitMessage(font.large, "Empty Slot", screen, &preview_rect);
				}
				
				// pagination
				ox += 24;
				oy += 124;
				for (int i=0; i<MENU_SLOT_COUNT; i++) {
					if (i==menu.slot)GFX_blitAsset(ASSET_PAGE, NULL, screen, &(SDL_Rect){SCALE2(ox+(i*15),oy)});
					else GFX_blitAsset(ASSET_DOT, NULL, screen, &(SDL_Rect){SCALE2(ox+(i*15)+4,oy+2)});
				}
			}
	
			GFX_flip(screen);
			dirty = 0;
		}
		else GFX_sync();
	}
	
	SDL_FreeSurface(preview);
	
	PAD_reset();

	GFX_clearAll();
	POW_warn(1);
	
	if (!quit) {
		if (restore_w!=FIXED_WIDTH || restore_h!=FIXED_HEIGHT) {
			screen = GFX_resize(restore_w,restore_h,restore_p);
		}
		GFX_clear(screen);
		video_refresh_callback(renderer.src, renderer.src_w, renderer.src_h, renderer.src_p);

		setOverclock(overclock); // restore overclock value
		if (rumble_strength) VIB_setStrength(rumble_strength);
		
		GFX_setVsync(prevent_tearing);
	}
	
	SDL_FreeSurface(bitmap);
	SDL_FreeSurface(backing);
	POW_disableAutosleep();
}

// TODO: move to POW_*?
static unsigned getUsage(void) { // from picoarch
	long unsigned ticks = 0;
	long ticksps = 0;
	FILE *file = NULL;

	file = fopen("/proc/self/stat", "r");
	if (!file)
		goto finish;

	if (!fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &ticks))
		goto finish;

	ticksps = sysconf(_SC_CLK_TCK);

	if (ticksps)
		ticks = ticks * 100 / ticksps;

finish:
	if (file)
		fclose(file);

	return ticks;
}

static void trackFPS(void) {
	cpu_ticks += 1;
	static int last_use_ticks = 0;
	uint32_t now = SDL_GetTicks();
	if (now - sec_start>=1000) {
		double last_time = (double)(now - sec_start) / 1000;
		fps_double = fps_ticks / last_time;
		cpu_double = cpu_ticks / last_time;
		use_ticks = getUsage();
		if (use_ticks && last_use_ticks) {
			use_double = (use_ticks - last_use_ticks) / last_time;
		}
		last_use_ticks = use_ticks;
		sec_start = now;
		cpu_ticks = 0;
		fps_ticks = 0;
		
		// LOG_info("fps: %f cpu: %f\n", fps_double, cpu_double);
	}
}

static void limitFF(void) {
	static uint64_t last_time = 0;
	const uint64_t now = getMicroseconds();

	if (fast_forward && max_ff_speed) {
		if (last_time == 0) last_time = now;
		int elapsed = now - last_time;
		if (elapsed>0 && elapsed<0x80000) {
			uint64_t ff_frame_time = 1000000 / (core.fps * (max_ff_speed + 1)); // TODO: define this only when max_ff_speed changes
			if (elapsed<ff_frame_time) {
				int delay = (ff_frame_time - elapsed) / 1000;
				if (delay>0) {
					// TODO: huh, this isn't causing the Tekken 3 hangs...
					// printf("limitFF delay: %i\n", delay); fflush(stdout);
					SDL_Delay(delay);
				}
			}
			last_time += ff_frame_time;
			return;
		}
	}
	last_time = now;
}

int main(int argc , char* argv[]) {
	LOG_info("MinArch\n");
	InitSettings();

	setOverclock(overclock); // default to normal
	// force a stack overflow to ensure asan is linked and actually working
	// char tmp[2];
	// tmp[2] = 'a';
	
	char core_path[MAX_PATH];
	char rom_path[MAX_PATH]; 
	char tag_name[MAX_PATH];
	
	strcpy(core_path, argv[1]);
	strcpy(rom_path, argv[2]);
	getEmuName(rom_path, tag_name);
	
	screen = GFX_init(MODE_MENU);
	VIB_init();
	POW_init();
	
	MSG_init();
	
	// Overrides_init();
	
	Core_open(core_path, tag_name);
	Game_open(rom_path);
	if (!game.is_open) goto finish;
	
	// restore options
	Config_load();
	Config_init();
	Config_readOptions(); // cores with boot logo option (eg. gb) need to load options early
	setOverclock(overclock);
	GFX_setVsync(prevent_tearing);
	
	Core_init();
	
	// TODO: find a better place to do this
	// mixing static and loaded data is messy
	// why not move to Core_init()?
	// ah, because it's defined before options_menu...
	options_menu.items[1].desc = (char*)core.version;
	
	Core_load();
	Config_readOptions(); // but others load and report options later (eg. nes)
	Config_readControls(); // restore controls (after the core has reported its defaults)
	Config_free();
	Input_init(NULL);
		
	SND_init(core.sample_rate, core.fps);
	
	Menu_init();
	
	State_resume();
	
	POW_warn(1);
	POW_disableAutosleep();
	sec_start = SDL_GetTicks();
	while (!quit) {
		GFX_startFrame();
		
		core.run();
		limitFF();
		
		if (show_menu) Menu_loop();
		
		trackFPS();
	}
	
	Menu_quit();
	
finish:

	Game_close();
	Core_unload();
	
	Core_quit();
	Core_close();
	
	Config_quit();
	
	MSG_quit();
	QuitSettings();
	POW_quit();
	VIB_quit();
	GFX_quit();
	
	return EXIT_SUCCESS;
}
