#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <msettings.h>

#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <zip.h> 
#include <pthread.h>

// libretro-common
#include "libretro.h"
#include "streams/rzip_stream.h"
#include "streams/file_stream.h"

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "scaler.h"
#include <dirent.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL.h>

///////////////////////////////////////

static SDL_Surface* screen;
static int quit = 0;
static int newScreenshot = 0;
static int show_menu = 0;
static int simple_mode = 0;
static int was_threaded = 0;
static int should_run_core = 1; // used by threaded video
enum retro_pixel_format fmt;

static pthread_t		core_pt;
static pthread_mutex_t	core_mx;
static pthread_cond_t	core_rq; // not sure this is required


enum {
	SCALE_NATIVE,
	SCALE_ASPECT,
	SCALE_ASPECT_SCREEN,
	SCALE_FULLSCREEN,
	SCALE_CROPPED,
	SCALE_COUNT,
};

// default frontend options
static int screen_scaling = SCALE_ASPECT;
static int resampling_quality = 2;
static int ambient_mode = 0;
static int screen_sharpness = SHARPNESS_SOFT;
static int screen_effect = EFFECT_NONE;
static int screenx = 64;
static int screeny = 64;
static int overlay = 0; 
static int prevent_tearing = 1; // lenient
static int use_core_fps = 0;
static int sync_ref = 0;
static int show_debug = 0;
static int max_ff_speed = 3; // 4x
static int ff_audio = 0;
static int fast_forward = 0;
static int overclock = 3; // auto
static int has_custom_controllers = 0;
static int gamepad_type = 0; // index in gamepad_labels/gamepad_values
static int downsample = 0; // set to 1 to convert from 8888 to 565
// these are no longer constants as of the RG CubeXX (even though they look like it)
static int DEVICE_WIDTH = 0; // FIXED_WIDTH;
static int DEVICE_HEIGHT = 0; // FIXED_HEIGHT;
static int DEVICE_PITCH = 0; // FIXED_PITCH;

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
	const char cheats_dir[MAX_PATH]; // eg. /mnt/sdcard/Cheats/GB
	const char overlays_dir[MAX_PATH]; // eg. /mnt/sdcard/Cheats/GB
	
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
	void (*cheat_reset)(void);
	void (*cheat_set)(unsigned id, bool enabled, const char*);
	bool (*load_game)(const struct retro_game_info *game);
	bool (*load_game_special)(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*unload_game)(void);
	unsigned (*get_region)(void);
	void *(*get_memory_data)(unsigned id);
	size_t (*get_memory_size)(unsigned id);
	
	retro_core_options_update_display_callback_t update_visibility_callback;
	// retro_audio_buffer_status_callback_t audio_buffer_status;
} core;

int extract_zip(char** extensions);

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
	int skipzip = 0;
	memset(&game, 0, sizeof(game));
	
	strcpy((char*)game.path, path);
	strcpy((char*)game.name, strrchr(path, '/')+1);

	// check first if the rom already is alive in tmp folder if so skip unzipping shit
	char tmpfldr[255];
	snprintf(tmpfldr, sizeof(tmpfldr), "/tmp/nextarch/%s", core.tag);
	char *tmppath = PLAT_findFileInDir(tmpfldr, game.name);
	if (tmppath) {
		printf("File exists skipping unzipping and setting game.tmp_path: %s\n", tmppath);
		strcpy((char*)game.tmp_path, tmppath);
		skipzip = 1;
		free(tmppath);
	} else {
		printf("File does not exist in %s\n",tmpfldr);
	}
		
	// if we have a zip file
	if (suffixMatch(".zip", game.path) && !skipzip) {
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
			// extract zip file located at game.path to game.tmp_path
			// game.tmp_path is temp dir generated by mkdtemp
			LOG_info("Extracting zip file manually: %s\n", game.path);
			if(!extract_zip(extensions))
				return;
		}
		else {
			LOG_info("Core can handle zip file: %s\n", game.path);
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
	// why delete tempfile? keep it for next time when loading the game its much faster from /tmp ram folder
	// if (game.tmp_path[0]) remove(game.tmp_path);
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
	putFile(CHANGE_DISC_PATH, path); // NextUI still needs to know this to update recents.txt
}

int extract_zip(char** extensions)
{
	char buf[100];
	struct zip *za;
	int ze;
	if ((za = zip_open(game.path, 0, &ze)) == NULL) {
		zip_error_t error;
		zip_error_init_with_code(&error, ze);
		LOG_error("can't open zip archive `%s': %s\n", game.path, zip_error_strerror(&error));
		return 0;
	}

	// char tmp_template[MAX_PATH];
	// strcpy(tmp_template, "/tmp/minarch-XXXXXX");
	
	mkdir("/tmp/nextarch",0777);
	char tmp_dirname[255];
	snprintf(tmp_dirname, sizeof(tmp_dirname), "%s/%s", "/tmp/nextarch",core.tag);
	mkdir(tmp_dirname,0777);

	int i, len;
	int fd;
	struct zip_file *zf;
	struct zip_stat sb;
	long long sum;
	for (i = 0; i < zip_get_num_entries(za, 0); i++) {
		if (zip_stat_index(za, i, 0, &sb) == 0) {
			len = strlen(sb.name);
			//LOG_info("Name: [%s], ", sb.name);
			//LOG_info("Size: [%llu], ", sb.size);
			//LOG_info("mtime: [%u]\n", (unsigned int)sb.mtime);
			if (sb.name[len - 1] == '/') {
				sprintf(game.tmp_path, "%s/%s", tmp_dirname, basename((char*)sb.name));
			} else {
				int found = 0;
				char extension[8];
				for (int e=0; extensions[e]; e++) {
					sprintf(extension, ".%s", extensions[e]);
					if (suffixMatch(extension, sb.name)) {
						found = 1;
						break;
					}
				}
				if (!found) continue;

				zf = zip_fopen_index(za, i, 0);
				if (!zf) {
					LOG_error( "zip_fopen_index failed\n");
					return 0;
				}

				sprintf(game.tmp_path, "%s/%s", tmp_dirname, basename((char*)sb.name));
				fd = open(game.tmp_path, O_RDWR | O_TRUNC | O_CREAT, 0644);
				if (fd < 0) {
					LOG_error( "open failed\n");
					zip_fclose(zf);
					return 0;
				}
				//LOG_info("Writing: %s\n", game.tmp_path);

				sum = 0;
				while (sum != sb.size) {
					len = zip_fread(zf, buf, 100);
					if (len < 0) {
						LOG_error( "zip_fread failed\n");
						close(fd);
						zip_fclose(zf);
						return 0;
					}
					write(fd, buf, len);
					sum += len;
				}
				close(fd);
				zip_fclose(zf);
				return 1;
			}
		}
	}
	
	if (zip_close(za) == -1) {
		LOG_error("can't close zip archive `%s'\n", game.path);
		return 0;
	}

	return 0;
}

///////////////////////////////////////
// based on picoarch/cheat.c

struct Cheat {
	const char *name;
	const char *info;
	int enabled;
	const char *code;
};

static struct Cheats {
	int enabled;
	size_t count;
	struct Cheat *cheats;
} cheatcodes;

#define CHEAT_MAX_DESC_LEN 27
#define CHEAT_MAX_LINE_LEN 52
#define CHEAT_MAX_LINES 3

static size_t parse_count(FILE *file) {
	size_t count = 0;
	fscanf(file, " cheats = %lu\n", (unsigned long *)&count);
	return count;
}

static const char *find_val(const char *start) {
	start--;
	while(!isspace(*++start))
		;

	while(isspace(*++start))
		;

	if (*start != '=')
		return NULL;

	while(isspace(*++start))
		;

	return start;
}

static int parse_bool(const char *ptr, int *out) {
	if (!strncasecmp(ptr, "true", 4)) {
		*out = 1;
	} else if (!strncasecmp(ptr, "false", 5)) {
		*out = 0;
	} else {
		return -1;
	}
	return 0;
}

static int parse_string(const char *ptr, char *buf, size_t len) {
	int index = 0;
	size_t input_len = strlen(ptr);

	buf[0] = '\0';

	if (*ptr++ != '"')
		return -1;

	while (*ptr != '\0' && *ptr != '"' && index < len - 1) {
		if (*ptr == '\\' && index < input_len - 1) {
			ptr++;
			buf[index++] = *ptr++;
		} else if (*ptr == '&' && !strncmp(ptr, "&quot;", 6)) {
			buf[index++] = '"';
			ptr += 6;
		} else {
			buf[index++] = *ptr++;
		}
	}

	if (*ptr != '"') {
		buf[0] = '\0';
		return -1;
	}

	buf[index] = '\0';
	return 0;
}

static int parse_cheats(struct Cheats *cheats, FILE *file) {
	int ret = -1;
	char line[512];
	char buf[512];
	const char *ptr;

	do {
		if (!fgets(line, sizeof(line), file)) {
			ret = 0;
			break;
		}

		if (line[strlen(line) - 1] != '\n' && !feof(file)) {
			LOG_warn("Cheat line too long\n");
			continue;
		}

		if ((ptr = strstr(line, "cheat"))) {
			int index = -1;
			struct Cheat *cheat;
			size_t len;
			sscanf(ptr, "cheat%d", &index);

			if (index >= cheats->count)
				continue;
			cheat = &cheats->cheats[index];

			if (strstr(ptr, "_desc")) {
				ptr = find_val(ptr);
				if (!ptr || parse_string(ptr, buf, sizeof(buf))) {
					LOG_warn("Couldn't parse cheat %d description\n", index);
					continue;
				}

				len = strlen(buf);
				if (len == 0)
					continue;

				cheat->name = calloc(len+1, sizeof(char));
				if (!cheat->name)
					goto finish;

				strncpy((char *)cheat->name, buf, len);
				truncateString((char *)cheat->name, CHEAT_MAX_DESC_LEN);

				if (len >= CHEAT_MAX_DESC_LEN) {
					cheat->info = calloc(len+1, sizeof(char));
					if (!cheat->info)
						goto finish;

					strncpy((char *)cheat->info, buf, len);
					wrapString((char *)cheat->info, CHEAT_MAX_LINE_LEN, CHEAT_MAX_LINES);
				}
			} else if (strstr(ptr, "_code")) {
				ptr = find_val(ptr);
				if (!ptr || parse_string(ptr, buf, sizeof(buf))) {
					LOG_warn("Couldn't parse cheat %d code\n", index);
					continue;
				}

				len = strlen(buf);
				if (len == 0)
					continue;

				cheat->code = calloc(len+1, sizeof(char));
				if (!cheat->code)
					goto finish;

				strncpy((char *)cheat->code, buf, len);
			} else if (strstr(ptr, "_enable")) {
				ptr = find_val(ptr);
				if (!ptr || parse_bool(ptr, &cheat->enabled)) {
					LOG_warn("Couldn't parse cheat %d enabled\n", index);
					continue;
				}
			}
		}
	} while(1);

finish:
	return ret;
}

void Cheats_free() {
	size_t i;
	for (i = 0; i < cheatcodes.count; i++) {
		struct Cheat *cheat = &cheatcodes.cheats[i];
		if (cheat) {
			free((char *)cheat->name);
			free((char *)cheat->info);
			free((char *)cheat->code);
		}
	}
	free(cheatcodes.cheats);
	cheatcodes.count = 0;
}

void Cheats_load(const char *filename) {
	int success = 0;
	struct Cheats *cheats = &cheatcodes;
	FILE *file = NULL;
	size_t i;

	file = fopen(filename, "r");
	if (!file) {
		LOG_error("Error opening cheat file: %s\n\t%s\n", filename, strerror(errno));
		goto finish;
	}

	LOG_info("Loading cheats from %s\n", filename);

	//cheats = calloc(1, sizeof(struct Cheats));
	//if (!cheats) {
	//	LOG_error("Couldn't allocate memory for cheats\n");
	//	goto finish;
	//}

	cheatcodes.count = parse_count(file);
	if (cheatcodes.count <= 0) {
		LOG_error("Couldn't read cheat count\n");
		goto finish;
	}

	cheatcodes.cheats = calloc(cheatcodes.count, sizeof(struct Cheat));
	if (!cheatcodes.cheats) {
		LOG_error("Couldn't allocate memory for cheats\n");
		goto finish;
	}

	if (parse_cheats(&cheatcodes, file)) {
		LOG_error("Error reading cheat %d\n", i);
		goto finish;
	}

	LOG_info("Found %i cheats for the current game.\n", cheatcodes.count);

	success = 1;
finish:
	if (!success) {
		Cheats_free();
	}

	if (file)
		fclose(file);
}

static void Cheat_getPath(char* filename) {
	sprintf(filename, "%s/%s.cht", core.cheats_dir, game.name);
	LOG_info("Cheat_getPath %s\n", filename);
}

///////////////////////////////////////
static void formatSavePath(char* work_name, char* filename, const char* suffix) {
	char* tmp = strrchr(work_name, '.');
	if (tmp != NULL && strlen(tmp) > 2 && strlen(tmp) <= 5) {
		tmp[0] = '\0';
	}
	sprintf(filename, "%s/%s%s", core.saves_dir, work_name, suffix);
}

static void SRAM_getPath(char* filename) {
	char work_name[MAX_PATH];

	if (CFG_getSaveFormat() == SAVE_FORMAT_SRM) {
		strcpy(work_name, game.name);
		formatSavePath(work_name, filename, ".srm");
	}
	else if (CFG_getSaveFormat() == SAVE_FORMAT_GEN) {
		strcpy(work_name, game.name);
		formatSavePath(work_name, filename, ".sav");
	}
	else {
		sprintf(filename, "%s/%s.sav", core.saves_dir, game.name);
	}

	LOG_info("SRAM_getPath %s\n", filename);
}

static void SRAM_read(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (read): %s\n", filename);

	void* sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	// TODO: rzipstream_open can also handle uncompressed, else branch is probably unnecessary
	// srm, potentially compressed
	if (CFG_getSaveFormat() == SAVE_FORMAT_SRM) {
		rzipstream_t* sram_file = rzipstream_open(filename, RETRO_VFS_FILE_ACCESS_READ);
		if(!sram_file) return;

		if (!sram || rzipstream_read(sram_file, sram, sram_size) < 0)
			LOG_error("rzipstream: Error reading SRAM data\n");
		
		rzipstream_close(sram_file);
	}
	// uncompressed
	else {
		RFILE* sram_file = filestream_open(filename, RETRO_VFS_FILE_ACCESS_READ, 0);
		if(!sram_file) return;

		if (!sram || filestream_read(sram_file, sram, sram_size) < 0)
			LOG_error("filestream: Error reading SRAM data\n");
		
		filestream_close(sram_file);
	}
}

static void SRAM_write(void) {
	size_t sram_size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) return;
	
	char filename[MAX_PATH];
	SRAM_getPath(filename);
	printf("sav path (write): %s\n", filename);
	
	void *sram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);

	// srm, compressed
	if (CFG_getSaveFormat() == SAVE_FORMAT_SRM) {
		if(!rzipstream_write_file(filename, sram, sram_size))
			LOG_error("rzipstream: Error writing SRAM data to file\n");
	}
	else {
		if(!filestream_write_file(filename, sram, sram_size))
			LOG_error("filestream: Error writing SRAM data to file\n");
	}

	sync();
}

///////////////////////////////////////

static void RTC_getPath(char* filename) {
	sprintf(filename, "%s/%s.rtc", core.saves_dir, game.name);
}
static void RTC_read(void) {
	size_t rtc_size = core.get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) return;
	
	char filename[MAX_PATH];
	RTC_getPath(filename);
	printf("rtc path (read): %s\n", filename);
	
	FILE *rtc_file = fopen(filename, "r");
	if (!rtc_file) return;

	void* rtc = core.get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || !fread(rtc, 1, rtc_size, rtc_file)) {
		LOG_error("Error reading RTC data\n");
	}

	fclose(rtc_file);
}
static void RTC_write(void) {
	size_t rtc_size = core.get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) return;
	
	char filename[MAX_PATH];
	RTC_getPath(filename);
	printf("rtc path (write) size(%u): %s\n", rtc_size, filename);
		
	FILE *rtc_file = fopen(filename, "w");
	if (!rtc_file) {
		LOG_error("Error opening RTC file: %s\n", strerror(errno));
		return;
	}

	void *rtc = core.get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || rtc_size != fwrite(rtc, 1, rtc_size, rtc_file)) {
		LOG_error("Error writing RTC data to file\n");
	}

	fclose(rtc_file);

	sync();
}

///////////////////////////////////////

static int state_slot = 0;

static void formatStatePath(char* work_name, char* filename, const char* suffix) {
	char* tmp = strrchr(work_name, '.');
	if (tmp != NULL && strlen(tmp) > 2 && strlen(tmp) <= 5) {
		tmp[0] = '\0';
	}
	sprintf(filename, "%s/%s%s", core.saves_dir, work_name, suffix);
}

static void State_getPath(char* filename) {
	char work_name[MAX_PATH];

	if (CFG_getStateFormat() == STATE_FORMAT_SRM) {
		strcpy(work_name, game.name);
		char* tmp = strrchr(work_name, '.');
		if (tmp != NULL && strlen(tmp) > 2 && strlen(tmp) <= 5) {
			tmp[0] = '\0';
		}

		if(state_slot == AUTO_RESUME_SLOT)
			sprintf(filename, "%s/%s.state.auto", core.states_dir, work_name);
		else 
			sprintf(filename, "%s/%s.state.%i", core.states_dir, work_name, state_slot);
	}
	else {
		sprintf(filename, "%s/%s.st%i", core.states_dir, game.name, state_slot);
	}

	LOG_info("State_getPath %s\n", filename);
}

static void State_read(void) { // from picoarch
	size_t state_size = core.serialize_size();
	if (!state_size) return;

	int was_ff = fast_forward;
	fast_forward = 0;

	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);

	RFILE *state_rfile = NULL;
	rzipstream_t *state_rzfile = NULL;

	// TODO: rzipstream_open can also handle uncompressed, else branch is probably unnecessary
	// srm, potentially compressed
	if (CFG_getStateFormat() == STATE_FORMAT_SRM) {
		state_rzfile = rzipstream_open(filename, RETRO_VFS_FILE_ACCESS_READ);
		if(!state_rzfile) {
			if (state_slot!=8) { // st8 is a default state in MiniUI and may not exist, that's okay
				LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
			}
			goto error;
		}

		// some cores report the wrong serialize size initially for some games, eg. mgba: Wario Land 4
		// so we allow a size mismatch as long as the actual size fits in the buffer we've allocated
		if (state_size < rzipstream_read(state_rzfile, state, state_size)) {
			LOG_error("Error reading state data from file: %s (%s)\n", filename, strerror(errno));
			goto error;
		}

		if (!core.unserialize(state, state_size)) {
			LOG_error("Error restoring save state: %s (%s)\n", filename, strerror(errno));
			goto error;
		}
	}
	else {
		state_rfile = filestream_open(filename, RETRO_VFS_FILE_ACCESS_READ, 0);
		if (!state_rfile) {
			if (state_slot!=8) { // st8 is a default state in MiniUI and may not exist, that's okay
				LOG_error("Error opening state file: %s (%s)\n", filename, strerror(errno));
			}
			goto error;
		}
		
		// some cores report the wrong serialize size initially for some games, eg. mgba: Wario Land 4
		// so we allow a size mismatch as long as the actual size fits in the buffer we've allocated
		if (state_size < filestream_read(state_rfile, state, state_size)) {
			LOG_error("Error reading state data from file: %s (%s)\n", filename, strerror(errno));
			goto error;
		}
	
		if (!core.unserialize(state, state_size)) {
			LOG_error("Error restoring save state: %s (%s)\n", filename, strerror(errno));
			goto error;
		}
	}

error:
	if (state) free(state);
	if (state_rfile) filestream_close(state_rfile);
	if (state_rzfile) rzipstream_close(state_rzfile);
	
	fast_forward = was_ff;
}

static void State_write(void) { // from picoarch
	size_t state_size = core.serialize_size();
	if (!state_size) return;
	
	int was_ff = fast_forward;
	fast_forward = 0;

	void *state = calloc(1, state_size);
	if (!state) {
		LOG_error("Couldn't allocate memory for state\n");
		goto error;
	}

	if (!core.serialize(state, state_size)) {
		LOG_error("Error serializing save state\n");
		goto error;
	}

	char filename[MAX_PATH];
	State_getPath(filename);

	if (CFG_getStateFormat() == STATE_FORMAT_SRM) {
		if(!rzipstream_write_file(filename, state, state_size)) {
			LOG_error("rzipstream: Error writing state data to file: %s\n", filename);
			goto error;
		}
	}
	else {
		if(!filestream_write_file(filename, state, state_size)) {
			LOG_error("filestream: Error writing state data to file: %s\n", filename);
			goto error;
		}
	}

error:
	if (state) free(state);

	sync();
	
	fast_forward = was_ff;
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
	char* full; // info, longer but possibly still truncated
	char *category;
	char* var;
	int default_value;
	int value;
	int count; // TODO: drop this?
	int lock;
	int hidden;
	char** values;
	char** labels;
} Option;
typedef struct OptionCategory {
	char *key;
	char *desc;
	char *info;
} OptionCategory;
typedef struct OptionList {
	int count;
	int changed;
	Option* options;
	
	int enabled_count;
	Option** enabled_options;

	OptionCategory *categories;
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
	"Aspect Screen",
	"Fullscreen",
	"Cropped",
	NULL
};
static char* resample_labels[] = {
	"Low",
	"Medium",
	"High",
	"Max",
	NULL
};
static char* ambient_labels[] = {
	"Off",
	"All",
	"Top",
	"FN",
	"LR",
	"Top/LR",
	NULL
};

static char* effect_labels[] = {
	"None",
	"Line",
	"Grid",
	NULL
};
static char* overlay_labels[] = {
	"None",
	NULL
};
// static char* sharpness_labels[] = {
// 	"Sharp",
// 	"Crisp",
// 	"Soft",
// 	NULL
// };
static char* sharpness_labels[] = {
	"NEAREST",
	"LINEAR",
	NULL
};
static char* tearing_labels[] = {
	"Off",
	"Lenient",
	"Strict",
	NULL
};
static char* sync_ref_labels[] = {
	"Auto",
	"Screen",
	"Native",
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
static char* offset_labels[] = {
	"-64",
	"-63",
	"-62",
	"-61",
	"-60",
	"-59",
	"-58",
	"-57",
	"-56",
	"-55",
	"-54",
	"-53",
	"-52",
	"-51",
	"-50",
	"-49",
	"-48",
	"-47",
	"-46",
	"-45",
	"-44",
	"-43",
	"-42",
	"-41",
	"-40",
	"-39",
	"-38",
	"-37",
	"-36",
	"-35",
	"-34",
	"-33",
	"-32",
	"-31",
	"-30",
	"-29",
	"-28",
	"-27",
	"-26",
	"-25",
	"-24",
	"-23",
	"-22",
	"-21",
	"-20",
	"-19",
	"-18",
	"-17",
	"-16",
	"-15",
	"-14",
	"-13",
	"-12",
	"-11",
	"-10",
	"-9",
	"-8",
	"-7",
	"-6",
	"-5",
	"-4",
	"-3",
	"-2",
	"-1",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"15",
	"16",
	"17",
	"18",
	"19",
	"20",
	"21",
	"22",
	"23",
	"24",
	"25",
	"26",
	"27",
	"28",
	"29",
	"30",
	"31",
	"32",
	"33",
	"34",
	"35",
	"36",
	"37",
	"38",
	"39",
	"40",
	"41",
	"42",
	"43",
	"44",
	"45",
	"46",
	"47",
	"48",
	"49",
	"50",
	"51",
	"52",
	"53",
	"54",
	"55",
	"56",
	"57",
	"58",
	"59",
	"60",
	"61",
	"62",
	"63",
	"64",
	NULL,
};
static char* nrofshaders_labels[] = {
	"off",
	"1",
	"2",
	"3",
	NULL
};
static char* shupscale_labels[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"screen",
	NULL
};
static char* shfilter_labels[] = {
	"NEAREST",
	"LINEAR",
	NULL
};
static char* shscaletype_labels[] = {
	"source",
	"relative",
	"screen",
	NULL
};

///////////////////////////////

enum {
	FE_OPT_SCALING,
	FE_OPT_RESAMPLING,
	FE_OPT_AMBIENT,
	FE_OPT_EFFECT,
	FE_OPT_OVERLAY,
	FE_OPT_SCREENX,
	FE_OPT_SCREENY,
	FE_OPT_SHARPNESS,
	FE_OPT_TEARING,
	FE_OPT_SYNC_REFERENCE,
	FE_OPT_OVERCLOCK,
	FE_OPT_DEBUG,
	FE_OPT_MAXFF,
	FE_OPT_FF_AUDIO,
	FE_OPT_COUNT,
};

enum {
	SHORTCUT_SAVE_STATE,
	SHORTCUT_LOAD_STATE,
	SHORTCUT_RESET_GAME,
	SHORTCUT_SAVE_QUIT,
	SHORTCUT_CYCLE_SCALE,
	SHORTCUT_CYCLE_EFFECT,
	SHORTCUT_TOGGLE_FF,
	SHORTCUT_HOLD_FF,
	SHORTCUT_GAMESWITCHER,
	SHORTCUT_COUNT,
};

enum {
	SYNC_SRC_AUTO,
	SYNC_SRC_SCREEN,
	SYNC_SRC_CORE
};
enum {
	SH_EXTRASETTINGS,
	SH_SHADERS_PRESET,
	SH_NROFSHADERS,
	SH_SHADER1,
	SH_SHADER1_FILTER,
	SH_SRCTYPE1,
	SH_SCALETYPE1,
	SH_UPSCALE1,
	SH_SHADER2,
	SH_SHADER2_FILTER,
	SH_SRCTYPE2,
	SH_SCALETYPE2,
	SH_UPSCALE2,
	SH_SHADER3,
	SH_SHADER3_FILTER,
	SH_SRCTYPE3,
	SH_SCALETYPE3,
	SH_UPSCALE3,
	SH_NONE
};

#define LOCAL_BUTTON_COUNT 16 // depends on device
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
	{"Up",			RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_DPAD_UP},
	{"Down",		RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DPAD_DOWN},
	{"Left",		RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_DPAD_LEFT},
	{"Right",		RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_DPAD_RIGHT},
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
	{"L3 Button",	RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_L3},
	{"R3 Button",	RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_R3},
	{NULL,0,0}
};
static ButtonMapping button_label_mapping[] = { // used to lookup the retro_id and local btn_id from button name
	{"NONE",	-1,								BTN_ID_NONE},
	{"UP",		RETRO_DEVICE_ID_JOYPAD_UP,		BTN_ID_DPAD_UP},
	{"DOWN",	RETRO_DEVICE_ID_JOYPAD_DOWN,	BTN_ID_DPAD_DOWN},
	{"LEFT",	RETRO_DEVICE_ID_JOYPAD_LEFT,	BTN_ID_DPAD_LEFT},
	{"RIGHT",	RETRO_DEVICE_ID_JOYPAD_RIGHT,	BTN_ID_DPAD_RIGHT},
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
	{"L3",		RETRO_DEVICE_ID_JOYPAD_L3,		BTN_ID_L3},
	{"R3",		RETRO_DEVICE_ID_JOYPAD_R3,		BTN_ID_R3},
	{NULL,0,0}
};
static ButtonMapping core_button_mapping[RETRO_BUTTON_COUNT+1] = {0};

static const char* device_button_names[LOCAL_BUTTON_COUNT] = {
	[BTN_ID_DPAD_UP]	= "UP",
	[BTN_ID_DPAD_DOWN]	= "DOWN",
	[BTN_ID_DPAD_LEFT]	= "LEFT",
	[BTN_ID_DPAD_RIGHT]	= "RIGHT",
	[BTN_ID_SELECT]		= "SELECT",
	[BTN_ID_START]		= "START",
	[BTN_ID_Y]			= "Y",
	[BTN_ID_X]			= "X",
	[BTN_ID_B]			= "B",
	[BTN_ID_A]			= "A",
	[BTN_ID_L1]			= "L1",
	[BTN_ID_R1]			= "R1",
	[BTN_ID_L2]			= "L2",
	[BTN_ID_R2]			= "R2",
	[BTN_ID_L3]			= "L3",
	[BTN_ID_R3]			= "R3",
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
	"L3",
	"R3",
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
	"MENU+L3",
	"MENU+R3",
	NULL,
};
static char* overclock_labels[] = {
	"Powersave",
	"Normal",
	"Performance",
	"Auto",
	NULL,
};

// TODO: this should be provided by the core
static char* gamepad_labels[] = {
	"Standard",
	"DualShock",
	NULL,
};
static char* gamepad_values[] = {
	"1",
	"517",
	NULL,
};

enum {
	CONFIG_NONE,
	CONFIG_CONSOLE,
	CONFIG_GAME,
};

static inline char* getScreenScalingDesc(void) {
	if (GFX_supportsOverscan()) {
		return "Native uses integer scaling. Aspect uses core nreported aspect ratio.\nAspect screen uses screen aspect ratio\n Fullscreen has non-square\npixels. Cropped is integer scaled then cropped.";
	}
	else {
		return "Native uses integer scaling.\nAspect uses core reported aspect ratio.\nAspect screen uses screen aspect ratio\nFullscreen has non-square pixels.";
	}
}
static inline int getScreenScalingCount(void) {
	return GFX_supportsOverscan() ? 5 : 4;
}
	

static struct Config {
	char* system_cfg; // system.cfg based on system limitations
	char* default_cfg; // pak.cfg based on platform limitations
	char* user_cfg; // minarch.cfg or game.cfg based on user preference
	char* shaders_preset; // minarch.cfg or game.cfg based on user preference
	char* device_tag;
	OptionList frontend;
	OptionList core;
	OptionList shaders;
	OptionList shaderpragmas;
	ButtonMapping* controls;
	ButtonMapping* shortcuts;
	int loaded;
	int initialized;
} config = {
	.frontend = { // (OptionList)
		.count = FE_OPT_COUNT,
		.options = (Option[]){
			[FE_OPT_SCALING] = {
				.key	= "minarch_screen_scaling", 
				.name	= "Screen Scaling",
				.desc	= NULL, // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 3, // will call getScreenScalingCount()
				.values = scaling_labels,
				.labels = scaling_labels,
			},
			[FE_OPT_RESAMPLING] = {
				.key	= "minarch__resampling_quality", 
				.name	= "Audio Resampling Quality",
				.desc	= "Resampling quality higher takes more CPU", // will call getScreenScalingDesc()
				.default_value = 2,
				.value = 2,
				.count = 4,
				.values = resample_labels,
				.labels = resample_labels,
			},
			[FE_OPT_AMBIENT] = {
				.key	= "minarch_ambient", 
				.name	= "Ambient Mode",
				.desc	= "Makes your leds follow on screen colors", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 6,
				.values = ambient_labels,
				.labels = ambient_labels,
			},
			[FE_OPT_EFFECT] = {
				.key	= "minarch_screen_effect",
				.name	= "Screen Effect",
				.desc	= "Grid simulates an LCD grid.\nLine simulates CRT scanlines.\nEffects usually look best at native scaling.",
				.default_value = 0,
				.value = 0,
				.count = 3,
				.values = effect_labels,
				.labels = effect_labels,
			},
			[FE_OPT_OVERLAY] = {
				.key	= "minarch_overlay",
				.name	= "Overlay",
				.desc	= "Choose a custom overlay png from the Overlays folder",
				.default_value = 0,
				.value = 0,
				.count = 1,
				.values = overlay_labels,
				.labels = overlay_labels,
			},
			[FE_OPT_SCREENX] = {
				.key	= "minarch_screen_offsetx",
				.name	= "Offset screen X",
				.desc	= "Offset X pixels",
				.default_value = 64,
				.value = 64,
				.count = 129,
				.values = offset_labels,
				.labels = offset_labels,
			},
			[FE_OPT_SCREENY] = {
				.key	= "minarch_screen_offsety",
				.name	= "Offset screen Y",
				.desc	= "Offset Y pixels",
				.default_value = 64,
				.value = 64,
				.count = 129,
				.values = offset_labels,
				.labels = offset_labels,
			},
			[FE_OPT_SHARPNESS] = {
				// 	.key	= "minarch_screen_sharpness",
				.key	= "minarch_scale_filter",
				.name	= "Screen Sharpness",
				.desc	= "LINEAR smooths lines, but works better when final image is at higher resolution, so either core that outputs higher resolution or upscaling with shaders",
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = sharpness_labels,
				.labels = sharpness_labels,
			},
			[FE_OPT_TEARING] = {
				.key	= "minarch_prevent_tearing",
				.name	= "VSync",
				.desc	= "Wait for vsync before drawing the next frame.\nLenient only waits when within frame budget.\nStrict always waits.",
				.default_value = VSYNC_LENIENT,
				.value = VSYNC_LENIENT,
				.count = 3,
				.values = tearing_labels,
				.labels = tearing_labels,
			},
			[FE_OPT_SYNC_REFERENCE] = {
				.key	= "minarch_sync_reference",
				.name	= "Core Sync",
				.desc	= "Choose what should be used as a\nreference for the frame rate.\n\"Native\" uses the emulator frame rate,\n\"Screen\" uses the frame rate of the screen.",
				.default_value = SYNC_SRC_AUTO,
				.value = SYNC_SRC_AUTO,
				.count = 3,
				.values = sync_ref_labels,
				.labels = sync_ref_labels,
			},
			[FE_OPT_OVERCLOCK] = {
				.key	= "minarch_cpu_speed",
				.name	= "CPU Speed",
				.desc	= "Over- or underclock the CPU to prioritize\npure performance or power savings.",
				.default_value = 3,
				.value = 3,
				.count = 4,
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
				.desc	= "Fast forward will not exceed the\nselected speed (but may be less\ndepending on game and emulator).",
				.default_value = 3, // 4x
				.value = 3, // 4x
				.count = 8,
				.values = max_ff_labels,
				.labels = max_ff_labels,
			},
			[FE_OPT_FF_AUDIO] = {
				.key	= "minarch__ff_audio", 
				.name	= "Fast forward audio",
				.desc	= "Play or mute audio when fast forwarding.",
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = onoff_labels,
				.labels = onoff_labels,
			},
			[FE_OPT_COUNT] = {NULL}
		}
	},
	.core = { // (OptionList)
		.count = 0,
		.options = (Option[]){
			{NULL},
		},
	},
	.shaders = { // (OptionList)
		.count = 18,
		.options = (Option[]){
			[SH_EXTRASETTINGS] = {
				.key	= "minarch_shaders_settings", 
				.name	= "Optional Shaders Settings",
				.desc	= "If shaders have extra settings they will show up in this settings menu", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 0,
				.values = NULL,
				.labels = NULL,
			},
			[SH_SHADERS_PRESET] = {
				.key	= "minarch_shaders_preset", 
				.name	= "Shader / Emulator Settings Preset",
				.desc	= "Load a premade shaders/emulators config, to try out a preset but not permantly overwite your current settings, exit the game without saving settings!", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 0,
				.values = NULL,
				.labels = NULL,
			},
			[SH_NROFSHADERS] = {
				.key	= "minarch_nrofshaders", 
				.name	= "Number of Shaders",
				.desc	= "Number of shaders 1 to 3", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 4,
				.values = nrofshaders_labels,
				.labels = nrofshaders_labels,
			},
			
			[SH_SHADER1] = {
				.key	= "minarch_shader1", 
				.name	= "Shader 1",
				.desc	= "Shader 1 program to run", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 0,
				.values = NULL,
				.labels = NULL,
			},
			[SH_SHADER1_FILTER] = {
				.key	= "minarch_shader1_filter", 
				.name	= "Shader 1 Filter",
				.desc	= "Method of upscaling, NEAREST or LINEAR", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 2,
				.values = shfilter_labels,
				.labels = shfilter_labels,
			},
			[SH_SRCTYPE1] = {
				.key	= "minarch_shader1_srctype", 
				.name	= "Shader 1 Source type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_SCALETYPE1] = {
				.key	= "minarch_shader1_scaletype", 
				.name	= "Shader 1 Texture Type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_UPSCALE1] = {
				.key	= "minarch_shader1_upscale", 
				.name	= "Shader 1 Scale",
				.desc	= "This will scale images x times, screen scales to screens resolution (can hit performance)", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 9,
				.values = shupscale_labels,
				.labels = shupscale_labels,
			},
			[SH_SHADER2] = {
				.key	= "minarch_shader2", 
				.name	= "Shader 2",
				.desc	= "Shader 2 program to run", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 0,
				.values = NULL,
				.labels = NULL,

			},
			[SH_SHADER2_FILTER] = {
				.key	= "minarch_shader2_filter", 
				.name	= "Shader 2 Filter",
				.desc	= "Method of upscaling, NEAREST or LINEAR", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = shfilter_labels,
				.labels = shfilter_labels,
			},
			[SH_SRCTYPE2] = {
				.key	= "minarch_shader2_srctype", 
				.name	= "Shader 2 Source type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_SCALETYPE2] = {
				.key	= "minarch_shader2_scaletype", 
				.name	= "Shader 2 Texture Type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_UPSCALE2] = {
				.key	= "minarch_shader2_upscale", 
				.name	= "Shader 2 Scale",
				.desc	= "This will scale images x times, screen scales to screens resolution (can hit performance)", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 9,
				.values = shupscale_labels,
				.labels = shupscale_labels,
			},
			[SH_SHADER3] = {
				.key	= "minarch_shader3", 
				.name	= "Shader 3",
				.desc	= "Shader 3 program to run", // will call getScreenScalingDesc()
				.default_value = 2,
				.value = 2,
				.count = 0,
				.values = NULL,
				.labels = NULL,

			},
			[SH_SHADER3_FILTER] = {
				.key	= "minarch_shader3_filter", 
				.name	= "Shader 3 Filter",
				.desc	= "Method of upscaling, NEAREST or LINEAR", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 2,
				.values = shfilter_labels,
				.labels = shfilter_labels,
			},
			[SH_SRCTYPE3] = {
				.key	= "minarch_shader3_srctype", 
				.name	= "Shader 3 Source type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_SCALETYPE3] = {
				.key	= "minarch_shader3_scaletype", 
				.name	= "Shader 3 Texture Type",
				.desc	= "This will choose resolution source to scale from", // will call getScreenScalingDesc()
				.default_value = 1,
				.value = 1,
				.count = 3,
				.values = shscaletype_labels,
				.labels = shscaletype_labels,
			},
			[SH_UPSCALE3] = {
				.key	= "minarch_shader3_upscale", 
				.name	= "Shader 3 Scale",
				.desc	= "This will scale images x times, screen scales to screens resolution (can hit performance)", // will call getScreenScalingDesc()
				.default_value = 0,
				.value = 0,
				.count = 9,
				.values = shupscale_labels,
				.labels = shupscale_labels,
			},
			{NULL}
		},
	},
	.shaderpragmas = {
		.count = 0,
		.options = (Option[]) {
			{NULL},
		}
	},
	.controls = default_button_mapping,
	.shortcuts = (ButtonMapping[]){
		[SHORTCUT_SAVE_STATE]			= {"Save State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_LOAD_STATE]			= {"Load State",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_RESET_GAME]			= {"Reset Game",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_SAVE_QUIT]			= {"Save & Quit",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_CYCLE_SCALE]			= {"Cycle Scaling",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_CYCLE_EFFECT]			= {"Cycle Effect",		-1, BTN_ID_NONE, 0},
		[SHORTCUT_TOGGLE_FF]			= {"Toggle FF",			-1, BTN_ID_NONE, 0},
		[SHORTCUT_HOLD_FF]				= {"Hold FF",			-1, BTN_ID_NONE, 0},
		[SHORTCUT_GAMESWITCHER]			= {"Game Switcher",		-1, BTN_ID_NONE, 0},
		{NULL}
	},
};
static int Config_getValue(char* cfg, const char* key, char* out_value, int* lock) { // gets value from string
	char* tmp = cfg;
	while ((tmp = strstr(tmp, key))) {
		if (lock!=NULL && tmp>cfg && *(tmp-1)=='-') *lock = 1; // prefixed with a `-` means lock
		tmp += strlen(key);
		if (!strncmp(tmp, " = ", 3)) break; // matched
	};
	if (!tmp) return 0;
	tmp += 3;
	
	strncpy(out_value, tmp, 256);
	out_value[256 - 1] = '\0';
	tmp = strchr(out_value, '\n');
	if (!tmp) tmp = strchr(out_value, '\r');
	if (tmp) *tmp = '\0';

	// LOG_info("\t%s = %s (%s)\n", key, out_value, (lock && *lock) ? "hidden":"shown");
	return 1;
}





static void setOverclock(int i) {
    overclock = i;
    switch (i) {
        case 0: {
			useAutoCpu = 0;
            PWR_setCPUSpeed(CPU_SPEED_POWERSAVE);
            break;
		}
        case 1:  {
			useAutoCpu = 0;
            PWR_setCPUSpeed(CPU_SPEED_NORMAL);
            break;
		}
        case 2:  {
			useAutoCpu = 0;
            PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE);
            break;
		}
        case 3:  {
            PWR_setCPUSpeed(CPU_SPEED_NORMAL);
			useAutoCpu = 1;
            break;
		}
    }
}
static int toggle_thread = 0;
static int shadersreload = 0;
static void Config_syncFrontend(char* key, int value) {
	int i = -1;
	if (exactMatch(key,config.frontend.options[FE_OPT_SCALING].key)) {
		screen_scaling 	= value;
		
		renderer.dst_p = 0;
		i = FE_OPT_SCALING;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_RESAMPLING].key)) {
		resampling_quality = value;
		SND_setQuality(resampling_quality);
		i = FE_OPT_RESAMPLING;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_AMBIENT].key)) {
		ambient_mode = value;
		i = FE_OPT_AMBIENT;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_EFFECT].key)) {
		screen_effect = value;
		GFX_setEffect(value);
		renderer.dst_p = 0;
		i = FE_OPT_EFFECT;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_OVERLAY].key)) {
		char** overlayList = config.frontend.options[FE_OPT_OVERLAY].values;
		if (overlayList) {
			
			int count = 0;
			while (overlayList && overlayList[count]) count++;
			if (value >= 0 && value < count) {
				LOG_info("minarch: updating overlay - %s\n", overlayList[value]);
				GFX_setOverlay(overlayList[value], core.tag);
				overlay = value;
				renderer.dst_p = 0;
				i = FE_OPT_OVERLAY;
			}
		}
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SCREENX].key)) {
		screenx = value;
		GFX_setOffsetX(value);
		i = FE_OPT_SCREENX;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SCREENY].key)) {
		screeny = value;
		GFX_setOffsetY(value);
		i = FE_OPT_SCREENY;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SHARPNESS].key)) {
		GFX_setSharpness(value);
		i = FE_OPT_SHARPNESS;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_TEARING].key)) {
		prevent_tearing = value;
		i = FE_OPT_TEARING;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_SYNC_REFERENCE].key)) {
		sync_ref = value;
		i = FE_OPT_SYNC_REFERENCE;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_OVERCLOCK].key)) {
		overclock = value;
		i = FE_OPT_OVERCLOCK;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_DEBUG].key)) {
		show_debug = value;
		i = FE_OPT_DEBUG;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_MAXFF].key)) {
		max_ff_speed = value;
		i = FE_OPT_MAXFF;
	}
	else if (exactMatch(key,config.frontend.options[FE_OPT_FF_AUDIO].key)) {
		ff_audio = value;
		i = FE_OPT_FF_AUDIO;
	}
	if (i==-1) return;
	Option* option = &config.frontend.options[i];
	option->value = value;
}

char** list_files_in_folder(const char* folderPath, int* fileCount, const char* extensionFilter) {
    DIR* dir = opendir(folderPath);
    if (!dir) {
        perror("opendir");
        return NULL;
    }

    struct dirent* entry;
    struct stat fileStat;
    char** fileList = NULL;
    *fileCount = 0;

    while ((entry = readdir(dir)) != NULL) {
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", folderPath, entry->d_name);

        if (stat(fullPath, &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
            if (extensionFilter) {
                const char* ext = strrchr(entry->d_name, '.');
                if (!ext || strcmp(ext, extensionFilter) != 0) {
                    continue;
                }
            }

            char** temp = realloc(fileList, sizeof(char*) * (*fileCount + 2)); 
            if (!temp) {
                perror("realloc");
                for (int i = 0; i < *fileCount; ++i) {
                    free(fileList[i]);
                }
                free(fileList);
                closedir(dir);
                return NULL;
            }
            fileList = temp;
            fileList[*fileCount] = strdup(entry->d_name);
            fileList[*fileCount + 1] = NULL;
            (*fileCount)++;
        }
    }

    closedir(dir);

    // Alphabetical sort
    for (int i = 0; i < *fileCount - 1; ++i) {
        for (int j = i + 1; j < *fileCount; ++j) {
            if (strcmp(fileList[i], fileList[j]) > 0) {
                char* temp = fileList[i];
                fileList[i] = fileList[j];
                fileList[j] = temp;
            }
        }
    }

    return fileList;
}




static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value);
enum {
	CONFIG_WRITE_ALL,
	CONFIG_WRITE_GAME,
};
static void Config_getPath(char* filename, int override) {
	char device_tag[64] = {0};
	if (config.device_tag) sprintf(device_tag,"-%s",config.device_tag);
	if (override) sprintf(filename, "%s/%s%s.cfg", core.config_dir, game.name, device_tag);
	else sprintf(filename, "%s/minarch%s.cfg", core.config_dir, device_tag);
	LOG_info("Config_getPath %s\n", filename);
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
	
	// populate shader options
	int filecount;
	char** filelist = list_files_in_folder(SHADERS_FOLDER "/glsl", &filecount,NULL);
	int preset_filecount;
	char** preset_filelist = list_files_in_folder(SHADERS_FOLDER, &preset_filecount,".cfg");
	
	config.shaders.options[SH_SHADER1].values = filelist;
	config.shaders.options[SH_SHADER2].values = filelist;
	config.shaders.options[SH_SHADER3].values = filelist;
	config.shaders.options[SH_SHADERS_PRESET].values = preset_filelist;

	config.shaders.options[SH_SHADER1].labels = filelist;
	config.shaders.options[SH_SHADER2].labels = filelist;
	config.shaders.options[SH_SHADER3].labels = filelist;
	config.shaders.options[SH_SHADERS_PRESET].labels = preset_filelist;

	config.shaders.options[SH_SHADER1].count = filecount;
	config.shaders.options[SH_SHADER2].count = filecount;
	config.shaders.options[SH_SHADER3].count = filecount;
	config.shaders.options[SH_SHADERS_PRESET].count = preset_filecount;
	
	char overlaypath[255];
	snprintf(overlaypath, sizeof(overlaypath), "%s/%s", OVERLAYS_FOLDER, core.tag);
	char** overlaylist = list_files_in_folder(overlaypath, &filecount,NULL);

	if (overlaylist) {
		int newcount = filecount + 1;
		char** newlist = malloc(sizeof(char*) * (newcount + 1)); // +1 for NULL terminator
		if (!newlist) {
			LOG_info("failed to make newlist");
			return;
		}
		for (int i = 0; i < filecount; i++) {
			newlist[i + 1] = overlaylist[i];
		}

		newlist[0] = strdup("None");  
		newlist[newcount] = NULL;  
		
		free(overlaylist);

		overlaylist = newlist;
		filecount = newcount;

		config.frontend.options[FE_OPT_OVERLAY].labels = overlaylist;
		config.frontend.options[FE_OPT_OVERLAY].values = overlaylist;
		config.frontend.options[FE_OPT_OVERLAY].count = filecount;
	}
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
		Config_syncFrontend(option->key, option->value);
	}
	
	if (has_custom_controllers && Config_getValue(cfg,"minarch_gamepad_type",value,NULL)) {
		gamepad_type = strtol(value, NULL, 0);
		int device = strtol(gamepad_values[gamepad_type], NULL, 0);
		core.set_controller_port_device(0, device);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		// LOG_info("%s\n",option->key);
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.core, option->key, value);
	}
	for (int i=0; config.shaders.options[i].key; i++) {
		Option* option = &config.shaders.options[i];
		if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
		OptionList_setOptionValue(&config.shaders, option->key, value);
	}
	if(config.shaderpragmas.count > 0) {
		for (int i=0; config.shaderpragmas.options[i].key; i++) {
			Option* option = &config.shaderpragmas.options[i];
			if (!Config_getValue(cfg, option->key, value, &option->lock)) continue;
			OptionList_setOptionValue(&config.shaderpragmas, option->key, value);
		}
	}
}
static void Config_readControlsString(char* cfg) {
	if (!cfg) return;

	LOG_info("Config_readControlsString\n");
	
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
		
		int mod = 0;
		if (id>=LOCAL_BUTTON_COUNT) {
			id -= LOCAL_BUTTON_COUNT;
			mod = 1;
		}
		
		mapping->local = id;
		mapping->mod = mod;
	}
	
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		sprintf(key, "bind %s", mapping->name);
		sprintf(value, "NONE");

		if (!Config_getValue(cfg, key, value, NULL)) continue;
		
		int id = -1;
		for (int j=0; button_labels[j]; j++) {
			if (!strcmp(button_labels[j],value)) {
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
	
	config.device_tag = getenv("DEVICE");
	LOG_info("config.device_tag %s\n", config.device_tag);
	
	// update for crop overscan support
	Option* scaling_option = &config.frontend.options[FE_OPT_SCALING];
	scaling_option->desc = getScreenScalingDesc();
	scaling_option->count = getScreenScalingCount();
	if (!GFX_supportsOverscan()) {
		scaling_labels[4] = NULL;
	}
	
	char* system_path = SYSTEM_PATH "/system.cfg";
	
	char device_system_path[MAX_PATH] = {0};
	if (config.device_tag) sprintf(device_system_path, SYSTEM_PATH "/system-%s.cfg", config.device_tag);
	
	if (config.device_tag && exists(device_system_path)) {
		LOG_info("usng device_system_path: %s\n", device_system_path);
		config.system_cfg = allocFile(device_system_path);
	}
	else if (exists(system_path)) config.system_cfg = allocFile(system_path);
	else config.system_cfg = NULL;
	
	
	
	// LOG_info("config.system_cfg: %s\n", config.system_cfg);
	
	char default_path[MAX_PATH];
	getEmuPath((char *)core.tag, default_path);
	char* tmp = strrchr(default_path, '/');
	strcpy(tmp,"/default.cfg");

	char device_default_path[MAX_PATH] = {0};
	if (config.device_tag) {
		getEmuPath((char *)core.tag, device_default_path);
		tmp = strrchr(device_default_path, '/');
		char filename[64];
		sprintf(filename,"/default-%s.cfg", config.device_tag);
		strcpy(tmp,filename);
	}
	
	if (config.device_tag && exists(device_default_path)) {
		LOG_info("usng device_default_path: %s\n", device_default_path);
		config.default_cfg = allocFile(device_default_path);
	}
	else if (exists(default_path)) config.default_cfg = allocFile(default_path);
	else config.default_cfg = NULL;
	
	// LOG_info("config.default_cfg: %s\n", config.default_cfg);
	
	char path[MAX_PATH];
	config.loaded = CONFIG_NONE;
	int override = 0;
	Config_getPath(path, CONFIG_WRITE_GAME);
	if (exists(path)) override = 1; 
	if (!override) Config_getPath(path, CONFIG_WRITE_ALL);
	
	config.user_cfg = allocFile(path);
	if (!config.user_cfg) return;
	
	LOG_info("using user config: %s\n", path);
	
	config.loaded = override ? CONFIG_GAME : CONFIG_CONSOLE;
}
static void Config_free(void) {
	if (config.system_cfg) free(config.system_cfg);
	if (config.default_cfg) free(config.default_cfg);
	if (config.user_cfg) free(config.user_cfg);
}
static void Config_readOptions(void) {
	Config_readOptionsString(config.system_cfg);
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
		int count = 0;
		while ( option->values &&  option->values[count]) count++;
		if (option->value >= 0 && option->value < count) {
			fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
		}
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
	}
	for (int i=0; config.shaders.options[i].key; i++) {
		Option* option = &config.shaders.options[i];
		int count = 0;
		while ( option->values &&  option->values[count]) count++;
		if (option->value >= 0 && option->value < count) {
			fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
		}
	}
	for (int i=0; config.shaderpragmas.options[i].key; i++) {
		Option* option = &config.shaderpragmas.options[i];
		int count = 0;
		while ( option->values &&  option->values[count]) count++;
		if (option->value >= 0 && option->value < count) {
			fprintf(file, "%s = %s\n", option->key, option->values[option->value]);
		}
	}
	
	if (has_custom_controllers) fprintf(file, "%s = %i\n", "minarch_gamepad_type", gamepad_type);
	
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, button_labels[j]);
	}
	for (int i=0; config.shortcuts[i].name; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int j = mapping->local + 1;
		if (mapping->mod) j += LOCAL_BUTTON_COUNT;
		fprintf(file, "bind %s = %s\n", mapping->name, button_labels[j]);
	}
	
	fclose(file);
	sync();
}
static void Config_restore(void) {
	char path[MAX_PATH];
	if (config.loaded==CONFIG_GAME) {
		if (config.device_tag) sprintf(path, "%s/%s-%s.cfg", core.config_dir, game.name, config.device_tag);
		else sprintf(path, "%s/%s.cfg", core.config_dir, game.name);
		unlink(path);
		LOG_info("deleted game config: %s\n", path);
	}
	else if (config.loaded==CONFIG_CONSOLE) {
		if (config.device_tag) sprintf(path, "%s/minarch-%s.cfg", core.config_dir, config.device_tag);
		else sprintf(path, "%s/minarch.cfg", core.config_dir);
		unlink(path);
		LOG_info("deleted console config: %s\n", path);
	}
	config.loaded = CONFIG_NONE;
	
	for (int i=0; config.frontend.options[i].key; i++) {
		Option* option = &config.frontend.options[i];
		option->value = option->default_value;
		Config_syncFrontend(option->key, option->value);
	}
	for (int i=0; config.core.options[i].key; i++) {
		Option* option = &config.core.options[i];
		option->value = option->default_value;
	}
	for (int i=0; config.shaders.options[i].key; i++) {
		Option* option = &config.shaders.options[i];
		option->value = option->default_value;
	}
	config.core.changed = 1; // let the core know
	
	if (has_custom_controllers) {
		gamepad_type = 0;
		core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	}

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

void readShadersPreset(int i) {
		char shaderspath[MAX_PATH] = {0};
		sprintf(shaderspath, SHADERS_FOLDER "/%s", config.shaders.options[SH_SHADERS_PRESET].values[i]);
		LOG_info("read shaders preset %s\n",shaderspath);
		if (exists(shaderspath)) {
			config.shaders_preset = allocFile(shaderspath);
			Config_readOptionsString(config.shaders_preset);
		}
		else config.shaders_preset = NULL;
		

		
}

static void Config_syncShaders(char* key, int value) {
	int i = -1;
	if (exactMatch(key,config.shaders.options[SH_SHADERS_PRESET].key)) {
		readShadersPreset(value);
		i = SH_SHADERS_PRESET;
	}
	if (exactMatch(key,config.shaders.options[SH_NROFSHADERS].key)) {
		GFX_setShaders(value);
		shadersreload = 1;
		i = SH_NROFSHADERS;
	}

	if (exactMatch(key, config.shaders.options[SH_SHADER1].key)) {
		char** shaderList = config.shaders.options[SH_SHADER1].values;
		if (shaderList) {
			LOG_info("minarch: updating shader 1 - %i\n",value);
			int count = 0;
			while (shaderList && shaderList[count]) count++;
			if (value >= 0 && value < count) {
				GFX_updateShader(0, shaderList[value], NULL, NULL,NULL,NULL);
				i = SH_SHADER1;
			} 
		}
	}
	if (exactMatch(key,config.shaders.options[SH_SHADER1_FILTER].key)) {
		GFX_updateShader(0,NULL,NULL,&value,NULL,NULL);
		i = SH_SHADER1_FILTER;
	}
	if (exactMatch(key,config.shaders.options[SH_SRCTYPE1].key)) {
		GFX_updateShader(0,NULL,NULL,NULL,NULL,&value);
		i = SH_SRCTYPE1;
	}
	if (exactMatch(key,config.shaders.options[SH_SCALETYPE1].key)) {
		GFX_updateShader(0,NULL,NULL,NULL,&value,NULL);
		i = SH_SCALETYPE1;
	}
	if (exactMatch(key,config.shaders.options[SH_UPSCALE1].key)) {
		GFX_updateShader(0,NULL,&value,NULL,NULL,NULL);
		i = SH_UPSCALE1;
	}
	if (exactMatch(key, config.shaders.options[SH_SHADER2].key)) {
		char** shaderList = config.shaders.options[SH_SHADER2].values;
		if (shaderList) {
			LOG_info("minarch: updating shader 2 - %i\n",value);
			int count = 0;
			while (shaderList && shaderList[count]) count++;
			if (value >= 0 && value < count) {
				GFX_updateShader(1, shaderList[value], NULL, NULL,NULL,NULL);
				i = SH_SHADER2;
			}
		}
	}
	if (exactMatch(key,config.shaders.options[SH_SHADER2_FILTER].key)) {
		GFX_updateShader(1,NULL,NULL,&value,NULL,NULL);
		i = SH_SHADER2_FILTER;
	}
	if (exactMatch(key,config.shaders.options[SH_SRCTYPE2].key)) {
		GFX_updateShader(1,NULL,NULL,NULL,NULL,&value);
		i = SH_SRCTYPE2;
	}
	if (exactMatch(key,config.shaders.options[SH_SCALETYPE2].key)) {
		GFX_updateShader(1,NULL,NULL,NULL,&value,NULL);
		i = SH_SCALETYPE2;
	}
	if (exactMatch(key,config.shaders.options[SH_UPSCALE2].key)) {
		GFX_updateShader(1,NULL,&value,NULL,NULL,NULL);
		i = SH_UPSCALE2;
	}
	if (exactMatch(key, config.shaders.options[SH_SHADER3].key)) {
		char** shaderList = config.shaders.options[SH_SHADER3].values;
		if (shaderList) {
			LOG_info("minarch: updating shader 3 - %i\n",value);
			int count = 0;
			while (shaderList && shaderList[count]) count++;
			if (value >= 0 && value < count) {
				GFX_updateShader(2, shaderList[value], NULL, NULL,NULL,NULL);
				i = SH_SHADER3;
			}
		}
	}
	if (exactMatch(key,config.shaders.options[SH_SHADER3_FILTER].key)) {
		GFX_updateShader(2,NULL,NULL,&value,NULL,NULL);
		i = SH_SHADER3_FILTER;
	}
	if (exactMatch(key,config.shaders.options[SH_SRCTYPE3].key)) {
		GFX_updateShader(2,NULL,NULL,NULL,NULL,&value);
		i = SH_SRCTYPE3;
	}
	if (exactMatch(key,config.shaders.options[SH_SCALETYPE3].key)) {
		GFX_updateShader(2,NULL,NULL,NULL,&value,NULL);
		i = SH_SCALETYPE3;
	}
	if (exactMatch(key,config.shaders.options[SH_UPSCALE3].key)) {
		GFX_updateShader(2,NULL,&value,NULL,NULL,NULL);
		i = SH_UPSCALE3;
	}
	
	if (i==-1) return;
	Option* option = &config.shaders.options[i];
	option->value = value;
	shadersreload = 1;
}

////////
void loadShaderSettings() {
	int menucount = 0;
	config.shaderpragmas.options = calloc(config.shaders.options[SH_NROFSHADERS].value*32+1, sizeof(Option));
	for (int i=0; i < config.shaders.options[SH_NROFSHADERS].value; i++) {
		ShaderParam *params = PLAT_getShaderPragmas(i);
		if(params == NULL) continue;
		for (int j = 0; j < 32; j++) {
			if(params[j].def || params[j].min || params[j].max) {
				config.shaderpragmas.options[menucount].key = params[j].name;
				config.shaderpragmas.options[menucount].name = params[j].name;
				config.shaderpragmas.options[menucount].desc = params[j].name;
				config.shaderpragmas.options[menucount].default_value = params[j].def;
				
				int steps = (int)((params[j].max - params[j].min) / params[j].step) + 1;
				config.shaderpragmas.options[menucount].values = malloc(sizeof(char *) * (steps + 1));
				config.shaderpragmas.options[menucount].labels = malloc(sizeof(char *) * (steps + 1));
				for (int s = 0; s < steps; s++) {
					float val = params[j].min + s * params[j].step;
					char *str = malloc(16);
					snprintf(str, 16, "%.2f", val);
					config.shaderpragmas.options[menucount].values[s] = str;
					config.shaderpragmas.options[menucount].labels[s] = str;
					if(params[j].value == val)
						config.shaderpragmas.options[menucount].value = s;
				}
				config.shaderpragmas.options[menucount].count = steps;
				config.shaderpragmas.options[menucount].values[steps] = NULL;
				config.shaderpragmas.options[menucount].labels[steps] = NULL;
				menucount++;
			}
		}
	}
	config.shaderpragmas.count = menucount;
}
void initShaders() {
	for (int i=0; config.shaders.options[i].key; i++) {
		if(i!=SH_SHADERS_PRESET) {
			Option* option = &config.shaders.options[i];;
			Config_syncShaders(option->key, option->value);
		}
	}
	// first initialize the shaders menu and then reinitilize settings to finally apply the extra shader settings to the shaders themselves
	loadShaderSettings();
	Config_readOptions();
	// set shader settings after re-reading conigs
	for (int y=0; y < config.shaders.options[SH_NROFSHADERS].value; y++) {
		ShaderParam *params = PLAT_getShaderPragmas(y);
		if (params == NULL) {
			break;
		}
		for (int i=0; i < config.shaderpragmas.count; i++) {
			for (int j = 0; j < 32; j++) {
				if (exactMatch(params[j].name, config.shaderpragmas.options[i].key)) {
					params[j].value = strtof(config.shaderpragmas.options[i].values[config.shaderpragmas.options[i].value], NULL);
				}
			}
		}
	}
	shadersreload = 0;
}

///////////////////////////////
static struct Special {
	int palette_updated;
} special;
static void Special_updatedDMGPalette(int frames) {
	// LOG_info("Special_updatedDMGPalette(%i)\n", frames);
	special.palette_updated = frames; // must wait a few frames
}
static void Special_refreshDMGPalette(void) {
	special.palette_updated -= 1;
	if (special.palette_updated>0) return;
	
	int rgb = getInt("/tmp/dmg_grid_color");
	GFX_setEffectColor(rgb);
}
static void Special_init(void) {
	if (special.palette_updated>1) special.palette_updated = 1;
	// else if (exactMatch((char*)core.tag, "GBC"))  {
	// 	putInt("/tmp/dmg_grid_color",0xF79E);
	// 	special.palette_updated = 1;
	// }
}
static void Special_render(void) {
	if (special.palette_updated) Special_refreshDMGPalette();
}
static void Special_quit(void) {
	system("rm -f /tmp/dmg_grid_color");
}
///////////////////////////////

static  int Option_getValueIndex(Option* item, const char* value) {
	if (!value || !item || !item->values) return 0;
	int i = 0;
	while (item->values[i]) {
		if (!strcmp(item->values[i], value)) return i;
		i++;
	}
	return 0;
}
static void Option_setValue(Option* item, const char* value) {
	// TODO: store previous value?
	item->value = Option_getValueIndex(item, value);
}

// TODO: does this also need to be applied to OptionList_vars()?
static const char* option_key_name[] = {
	"pcsx_rearmed_analog_combo", "DualShock Toggle Combo",
	NULL
};
static const char* getOptionNameFromKey(const char* key, const char* name) {
	char* _key = NULL;
	for (int i=0; (_key = (char*)option_key_name[i]); i+=2) {
		if (exactMatch((char*)key,_key)) return option_key_name[i+1];
	}
	return name;
}

// the following 3 functions always touch config.core, the rest can operate on arbitrary OptionLists
static void OptionList_init(const struct retro_core_option_definition *defs) {
	LOG_info("OptionList_init\n");
	int count;
	for (count=0; defs[count].key; count++);
	
	// LOG_info("count: %i\n", count);
	
	// TODO: add frontend options to this? so the can use the same override method? eg. minarch_*

	config.core.count = count;
	config.core.categories = NULL; // There is no categories in v1 definition
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
			strcpy(item->name, getOptionNameFromKey(def->key,def->desc));
			
			if (def->info) {
				len = strlen(def->info) + 1;
				item->desc = calloc(len, sizeof(char));
				strncpy(item->desc, def->info, len);

				item->full = calloc(len, sizeof(char));
				strncpy(item->full, item->desc, len);
				// item->desc[len-1] = '\0';
				
				// these magic numbers are more about chars per line than pixel width 
				// so it's not going to be relative to the screen size, only the scale
				// what does that even mean?
				GFX_wrapText(font.tiny, item->desc, SCALE1(240), 2); // TODO magic number!
				GFX_wrapText(font.medium, item->full, SCALE1(240), 7); // TODO: magic number!
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
			
			// LOG_info("\tINIT %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		}
	}
	// fflush(stdout);
}

static void OptionList_v2_init(const struct retro_core_options_v2 *opt_defs) {
	LOG_info("OptionList_v2_init\n");
	struct retro_core_option_v2_category   *cats = opt_defs->categories;
	struct retro_core_option_v2_definition *defs = opt_defs->definitions;

	int cat_count = 0;
	while (cats[cat_count].key) cat_count++;

	int count = 0;
	while (defs[count].key) count++;
	
	// LOG_info("%i categories, %i options\n", cat_count, count);
	
	// TODO: add frontend options to this? so the can use the same override method? eg. minarch_*
	
	if (cat_count) {
		config.core.categories = calloc(cat_count + 1, sizeof(OptionCategory));

		for (int i=0; i<cat_count; i++) {
			const struct retro_core_option_v2_category *cat = &cats[i];
			OptionCategory* item = &config.core.categories[i];

			item->key  = strdup(cat->key);
			item->desc = strdup(cat->desc);
			item->info = cat->info ? strdup(cat->info) : NULL;
			printf("CATEGORY %s\n", item->key);
		}
	}
	else {
		config.core.categories = NULL;
	}

	config.core.count = count;
	if (count) {
		config.core.options = calloc(count+1, sizeof(Option));
		
		for (int i=0; i<config.core.count; i++) {
			const struct retro_core_option_v2_definition *def = &defs[i];
			Option* item = &config.core.options[i];
		
			item->key = strdup(def->key);
			item->name = strdup(getOptionNameFromKey(def->key, def->desc_categorized ? def->desc_categorized : def->desc));
			item->category = def->category_key ? strdup(def->category_key) : NULL;

			if (def->info) {
				item->desc = strdup(def->info);
				item->full = strdup(item->desc);
				
				// these magic numbers are more about chars per line than pixel width 
				// so it's not going to be relative to the screen size, only the scale
				// what does that even mean?
				GFX_wrapText(font.tiny, item->desc, SCALE1(240), 2); // TODO magic number!
				GFX_wrapText(font.medium, item->full, SCALE1(240), 7); // TODO: magic number!
			}
		
			for (count=0; def->values[count].value; count++);
		
			item->count = count;
			item->values = calloc(count+1, sizeof(char*));
			item->labels = calloc(count+1, sizeof(char*));
	
			for (int j=0; j<count; j++) {
				const char* value = def->values[j].value;
				const char* label = def->values[j].label;
		
				item->values[j] = strdup(value);
		
				if (label) {
					item->labels[j] = strdup(label);
				}
				else {
					item->labels[j] = item->values[j];
				}
				// printf("\t%s\n", item->labels[j]);
			}
			
			item->value = Option_getValueIndex(item, def->default_value);
			item->default_value = item->value;
			
			// LOG_info("\tINIT %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
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
	// if (item) LOG_info("\tGET %s (%s) = %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
	if (item) {
		int count = 0;
		while ( item->values && item->values[count]) count++;
		if (item->value >= 0 && item->value < count) {
			return item->values[item->value];
		}
	}
	// else LOG_warn("unknown option %s \n", key);
	return NULL;
}
static void OptionList_setOptionRawValue(OptionList* list, const char* key, int value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		item->value = value;
		list->changed = 1;
		// LOG_info("\tRAW SET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);

		if (exactMatch((char*)core.tag, "GB") && containsString(item->key, "palette")) Special_updatedDMGPalette(3); // from options
	}
	else LOG_info("unknown option %s \n", key);
}
static void OptionList_setOptionValue(OptionList* list, const char* key, const char* value) {
	Option* item = OptionList_getOption(list, key);
	if (item) {
		Option_setValue(item, value);
		list->changed = 1;
		// LOG_info("\tSET %s (%s) TO %s (%s)\n", item->name, item->key, item->labels[item->value], item->values[item->value]);
		// if (list->on_set) list->on_set(list, key);
		
		if (exactMatch((char*)core.tag, "GB") && containsString(item->key, "palette")) Special_updatedDMGPalette(2); // from core
	}
	else LOG_info("unknown option %s \n", key);
}
static void OptionList_setOptionVisibility(OptionList* list, const char* key, int visible) {
	Option* item = OptionList_getOption(list, key);
	if (item) item->hidden = !visible;
	else printf("unknown option %s \n", key); fflush(stdout);
}

///////////////////////////////

static void Menu_beforeSleep();
static void Menu_afterSleep();

static void Menu_saveState(void);
static void Menu_loadState(void);

static int setFastForward(int enable) {
	fast_forward = enable;
	return enable;
}

static uint32_t buttons = 0; // RETRO_DEVICE_ID_JOYPAD_* buttons
static int ignore_menu = 0;
static void input_poll_callback(void) {
	PAD_poll();

	int show_setting = 0;
	PWR_update(NULL, &show_setting, Menu_beforeSleep, Menu_afterSleep);

	// I _think_ this can stay as is...
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
	}
	if (PAD_isPressed(BTN_MENU) && (PAD_isPressed(BTN_PLUS) || PAD_isPressed(BTN_MINUS))) {
		ignore_menu = 1;
	}
	if (PAD_isPressed(BTN_MENU) && PAD_isPressed(BTN_SELECT)) {
		ignore_menu = 1;
		newScreenshot = 1;
		quit = 1;
		Menu_saveState();
		putFile(GAME_SWITCHER_PERSIST_PATH, game.path + strlen(SDCARD_PATH));
		GFX_clear(screen);
		
	}
	
	if (PAD_justPressed(BTN_POWER)) {
		
	}
	else if (PAD_justReleased(BTN_POWER)) {
		
	}
	
	static int toggled_ff_on = 0; // this logic only works because TOGGLE_FF is before HOLD_FF in the menu...
	for (int i=0; i<SHORTCUT_COUNT; i++) {
		ButtonMapping* mapping = &config.shortcuts[i];
		int btn = 1 << mapping->local;
		if (btn==BTN_NONE) continue; // not bound
		if (!mapping->mod || PAD_isPressed(BTN_MENU)) {
			if (i==SHORTCUT_TOGGLE_FF) {
				if (PAD_justPressed(btn)) {
					toggled_ff_on = setFastForward(!fast_forward);
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
					fast_forward = setFastForward(PAD_isPressed(btn));
					if (mapping->mod) ignore_menu = 1; // very unlikely but just in case
				}
			}
			else if (PAD_justPressed(btn)) {
				switch (i) {
					case SHORTCUT_SAVE_STATE: 
						newScreenshot = 1;
						Menu_saveState(); 
						break;
					case SHORTCUT_LOAD_STATE: Menu_loadState(); break;
					case SHORTCUT_RESET_GAME: core.reset(); break;
					case SHORTCUT_SAVE_QUIT:
						newScreenshot = 1;
						quit = 1;
						Menu_saveState();
						break;
					case SHORTCUT_GAMESWITCHER:
						newScreenshot = 1;
						quit = 1;
						Menu_saveState();
						putFile(GAME_SWITCHER_PERSIST_PATH, game.path + strlen(SDCARD_PATH));
						break;
					case SHORTCUT_CYCLE_SCALE:
						screen_scaling += 1;
						int count = config.frontend.options[FE_OPT_SCALING].count;
						if (screen_scaling>=count) screen_scaling -= count;
						Config_syncFrontend(config.frontend.options[FE_OPT_SCALING].key, screen_scaling);
						break;
					case SHORTCUT_CYCLE_EFFECT:
						screen_effect += 1;
						if (screen_effect>=EFFECT_COUNT) screen_effect -= EFFECT_COUNT;
						Config_syncFrontend(config.frontend.options[FE_OPT_EFFECT].key, screen_effect);
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
	
	// TODO: figure out how to ignore button when MENU+button is handled first
	// TODO: array size of LOCAL_ whatever that macro is
	// TODO: then split it into two loops
	// TODO: first check for MENU+button
	// TODO: when found mark button the array
	// TODO: then check for button
	// TODO: only modify if absent from array
	// TODO: the shortcuts loop above should also contribute to the array
	
	buttons = 0;
	for (int i=0; config.controls[i].name; i++) {
		ButtonMapping* mapping = &config.controls[i];
		int btn = 1 << mapping->local;
		if (btn==BTN_NONE) continue; // present buttons can still be unbound
		if (gamepad_type==0) {
			switch(btn) {
				case BTN_DPAD_UP: 		btn = BTN_UP; break;
				case BTN_DPAD_DOWN: 	btn = BTN_DOWN; break;
				case BTN_DPAD_LEFT: 	btn = BTN_LEFT; break;
				case BTN_DPAD_RIGHT: 	btn = BTN_RIGHT; break;
			}
		}
		if (PAD_isPressed(btn) && (!mapping->mod || PAD_isPressed(BTN_MENU))) {
			buttons |= 1 << mapping->retro;
			if (mapping->mod) ignore_menu = 1;
		}
		//  && !PWR_ignoreSettingInput(btn, show_setting)
	}
	
	// if (buttons) LOG_info("buttons: %i\n", buttons);
}
static int16_t input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port==0 && device==RETRO_DEVICE_JOYPAD && index==0) {
		if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return buttons;
		return (buttons >> id) & 1;
	}
	else if (port==0 && device==RETRO_DEVICE_ANALOG) {
		if (index==RETRO_DEVICE_INDEX_ANALOG_LEFT) {
			if (id==RETRO_DEVICE_ID_ANALOG_X) return pad.laxis.x;
			else if (id==RETRO_DEVICE_ID_ANALOG_Y) return pad.laxis.y;
		}
		else if (index==RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
			if (id==RETRO_DEVICE_ID_ANALOG_X) return pad.raxis.x;
			else if (id==RETRO_DEVICE_ID_ANALOG_Y) return pad.raxis.y;
		}
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
				//printf("UNAVAILABLE: %s\n", var->description); fflush(stdout);
				continue;
			}
			else {
				//printf("PRESENT    : %s\n", var->description); fflush(stdout);
			}
			present[var->id] = 1;
			core_button_names[var->id] = var->description;
		}
	}
	
	puts("---------------------------------");

	for (int i=0;default_button_mapping[i].name; i++) {
		ButtonMapping* mapping = &default_button_mapping[i];
		//LOG_info("DEFAULT %s (%s): <%s>\n", core_button_names[mapping->retro], mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]));
		if (core_button_names[mapping->retro]) mapping->name = (char*)core_button_names[mapping->retro];
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
		//LOG_info("%s: <%s> (%i:%i)\n", mapping->name, (mapping->local==BTN_ID_NONE ? "NONE" : device_button_names[mapping->local]), mapping->local, mapping->retro);
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
	// case RETRO_ENVIRONMENT_SET_ROTATION: { /* 1 */
	// 	LOG_info("RETRO_ENVIRONMENT_SET_ROTATION %i\n", *(int *)data); // core requests frontend to handle rotation
	// 	break;
	// }
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
		const enum retro_pixel_format *format = (const enum retro_pixel_format *)data;
		LOG_info("Requested pixel format by core: %d\n", *format); // Log the requested format (raw integer value)
	
		// Check if the requested format is supported
		if (*format == RETRO_PIXEL_FORMAT_XRGB8888) {
			fmt = RETRO_PIXEL_FORMAT_XRGB8888;
			LOG_info("Format supported: RETRO_PIXEL_FORMAT_XRGB8888\n");
			return true;  // Indicate success
		} else if (*format == RETRO_PIXEL_FORMAT_RGB565) {
			fmt = RETRO_PIXEL_FORMAT_RGB565;
			LOG_info("Format supported: RETRO_PIXEL_FORMAT_RGB565\n");
			return true;  // Indicate success
		} 
		// Log unsupported formats
		LOG_info("Format not supported, defaulting to RGB565\n");
		fmt = RETRO_PIXEL_FORMAT_RGB565;
		return false;  // Indicate failure
	}
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: { /* 11 */
		// puts("RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
		Input_init((const struct retro_input_descriptor *)data);
		return false;
		break;
	} 
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
		// puts("RETRO_ENVIRONMENT_GET_VARIABLE ");
		struct retro_variable *var = (struct retro_variable *)data;
		if (var && var->key) {
			var->value = OptionList_getOptionValue(&config.core, var->key);
			// printf("\t%s = \"%s\"\n", var->key, var->value);
		}
		// fflush(stdout);
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
	case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: { /* 22 */
		// LOG_info("%i: RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK\n", cmd);
		break;
	}
	case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: { /* 23 */
	        struct retro_rumble_interface *iface = (struct retro_rumble_interface*)data;

	        // LOG_info("Setup rumble interface.\n");
	        iface->set_rumble_state = set_rumble_state;
		break;
	}
	case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
		unsigned *out = (unsigned *)data;
		if (out)
			*out = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
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
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: { /* 35 */
		// LOG_info("RETRO_ENVIRONMENT_SET_CONTROLLER_INFO\n");
		const struct retro_controller_info *infos = (const struct retro_controller_info *)data;
		if (infos) {
			// TODO: store to gamepad_values/gamepad_labels for gamepad_device
			const struct retro_controller_info *info = &infos[0];
			for (int i=0; i<info->num_types; i++) {
				const struct retro_controller_description *type = &info->types[i];
				if (exactMatch((char*)type->desc,"dualshock")) { // currently only enabled for PlayStation
					has_custom_controllers = 1;
					break;
				}
				// printf("\t%i: %s\n", type->id, type->desc);
			}
		}
		fflush(stdout);
		return false; // TODO: tmp
		break;
	}
	// RETRO_ENVIRONMENT_SET_MEMORY_MAPS (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	case RETRO_ENVIRONMENT_GET_LANGUAGE: { /* 39 */
		// puts("RETRO_ENVIRONMENT_GET_LANGUAGE");
		if (data) *(int *) data = RETRO_LANGUAGE_ENGLISH;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: { /* (40 | RETRO_ENVIRONMENT_EXPERIMENTAL) */
		// puts("RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER");
		break;
	}
	
	case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
		// fixes fbneo save state graphics corruption
		// puts("RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE");
		int *out_p = (int *)data;
		if (out_p) {
			int out = 0;
			out |= RETRO_AV_ENABLE_VIDEO;
			out |= RETRO_AV_ENABLE_AUDIO;
			*out_p = out;
		}
		break;
	}
	
	// RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS (42 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	// RETRO_ENVIRONMENT_GET_INPUT_BITMASKS (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: { /* 51 | RETRO_ENVIRONMENT_EXPERIMENTAL */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: { /* 52 */
		// puts("RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION");
		if (data) *(unsigned *)data = 2;
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: { /* 53 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS");
		if (data) {
			OptionList_reset();
			OptionList_init((const struct retro_core_option_definition *)data);
			Config_readOptions();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: { /* 54 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL");
		const struct retro_core_options_intl *options = (const struct retro_core_options_intl *)data;
		if (options && options->us) {
			OptionList_reset();
			OptionList_init(options->us);
			Config_readOptions();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: { /* 55 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY");
	 	if (data) {
			const struct retro_core_option_display *display = (const struct retro_core_option_display *)data;
			LOG_info("Core asked for option key %s to be %s\n", display->key, display->visible ? "visible" : "invisible");
			OptionList_setOptionVisibility(&config.core, display->key, display->visible);
		}
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
	// RETRO_ENVIRONMENT_GET_GAME_INFO_EXT 66
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: { /* 67 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2");
		if (data) {
			OptionList_reset();
			OptionList_v2_init((const struct retro_core_options_v2 *)data); 
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: { /* 68 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL");
		if (data) {
			const struct retro_core_options_v2_intl *intl = (const struct retro_core_options_v2_intl *)data;
			OptionList_reset();
			OptionList_v2_init(intl->us);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK: {  /* 69 */
		// puts("RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK");
		if (data) {
			struct retro_core_options_update_display_callback *update_display_cb = (struct retro_core_options_update_display_callback *) data;
			core.update_visibility_callback = update_display_cb->callback;
		}
		else {
			core.update_visibility_callback = NULL;
		}
		break;
	}
	// used by fceumm
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
	case RETRO_ENVIRONMENT_SET_HW_RENDER:
	{
		struct retro_hw_render_callback *cb = (struct retro_hw_render_callback*)data;
		
		// Log the requested context
		LOG_info("Core requested GL context type: %d, version %d.%d\n", 
			cb->context_type, cb->version_major, cb->version_minor);

		// Fallback if version is 0.0 or other unexpected values
		if (cb->context_type == 4 && cb->version_major == 0 && cb->version_minor == 0) {
			LOG_info("Core requested invalid GL context type or version, defaulting to GLES 2.0\n");
			cb->context_type = RETRO_HW_CONTEXT_OPENGLES3;
			cb->version_major = 3;
			cb->version_minor = 0;
		}

		return true;
	}
	default:
		// LOG_debug("Unsupported environment cmd: %u\n", cmd);
		return false;
	}
	return true;
}

///////////////////////////////

static void hdmimon(void) {
	// handle HDMI change
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi==-1) had_hdmi = has_hdmi;
	if (has_hdmi!=had_hdmi) {
		had_hdmi = has_hdmi;

		LOG_info("restarting after HDMI change...\n");
		Menu_beforeSleep();
		sleep(4);
		show_menu = 0;
		quit = 1;
	}
}

///////////////////////////////

// TODO: this is a dumb API
SDL_Surface* digits;
#define DIGIT_WIDTH 9
#define DIGIT_HEIGHT 8
#define DIGIT_TRACKING -2
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
	digits = SDL_CreateRGBSurface(SDL_SWSURFACE,SCALE2(DIGIT_WIDTH*DIGIT_COUNT,DIGIT_HEIGHT),FIXED_DEPTH, 0,0,0,0);
	SDL_FillRect(digits, NULL, RGB_BLACK);
	
	SDL_Surface* digit;
	char* chars[] = { "0","1","2","3","4","5","6","7","8","9","/",".","%","x","(",")", NULL };
	char* c;
	int i = 0;
	while ((c = chars[i])) {
		digit = TTF_RenderUTF8_Blended(font.tiny, c, COLOR_WHITE);
		SDL_BlitSurface(digit, NULL, digits, &(SDL_Rect){ (i * SCALE1(DIGIT_WIDTH)) + (SCALE1(DIGIT_WIDTH) - digit->w)/2, (SCALE1(DIGIT_HEIGHT) - digit->h)/2});
		SDL_FreeSurface(digit);
		i += 1;
	}
}
static int MSG_blitChar(int n, int x, int y) {
	if (n!=DIGIT_SPACE) SDL_BlitSurface(digits, &(SDL_Rect){n*SCALE1(DIGIT_WIDTH),0,SCALE2(DIGIT_WIDTH,DIGIT_HEIGHT)}, screen, &(SDL_Rect){x,y});
	return x + SCALE1(DIGIT_WIDTH + DIGIT_TRACKING);
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

static const char* bitmap_font[] = {
	['0'] = 
		" 111 "
		"1   1"
		"1   1"
		"1  11"
		"1 1 1"
		"11  1"
		"1   1"
		"1   1"
		" 111 ",
	['1'] =
		"   1 "
		" 111 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 ",
	['2'] =
		" 111 "
		"1   1"
		"    1"
		"   1 "
		"  1  "
		" 1   "
		"1    "
		"1    "
		"11111",
	['3'] =
		" 111 "
		"1   1"
		"    1"
		"    1"
		" 111 "
		"    1"
		"    1"
		"1   1"
		" 111 ",
	['4'] =
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		"11111"
		"    1"
		"    1",
	['5'] =
		"11111"
		"1    "
		"1    "
		"1111 "
		"    1"
		"    1"
		"    1"
		"1   1"
		" 111 ",
	['6'] =
		" 111 "
		"1    "
		"1    "
		"1111 "
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		" 111 ",
	['7'] =
		"11111"
		"    1"
		"    1"
		"   1 "
		"  1  "
		"  1  "
		"  1  "
		"  1  "
		"  1  ",
	['8'] =
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		" 111 ",
	['9'] =
		" 111 "
		"1   1"
		"1   1"
		"1   1"
		"1   1"
		" 1111"
		"    1"
		"    1"
		" 111 ",
	['.'] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		" 11  "
		" 11  ",
	[','] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"  1  "
		"  1  "
		" 1   ",
	[' '] = 
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     "
		"     ",
	['('] = 
		"   1 "
		"  1  "
		" 1   "
		" 1   "
		" 1   "
		" 1   "
		" 1   "
		"  1  "
		"   1 ",
	[')'] = 
		" 1   "
		"  1  "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"   1 "
		"  1  "
		" 1   ",
	['/'] = 
		"   1 "
		"   1 "
		"   1 "
		"  1  "
		"  1  "
		"  1  "
		" 1   "
		" 1   "
		" 1   ",
	['x'] = 
		"     "
		"     "
		"1   1"
		"1   1"
		" 1 1 "
		"  1  "
		" 1 1 "
		"1   1"
		"1   1",
	['%'] = 
		" 1   "
		"1 1  "
		"1 1 1"
		" 1 1 "
		"  1  "
		" 1 1 "
		"1 1 1"
		"  1 1"
		"   1 ",
	['-'] =
		"     "
		"     "
		"     "
		"     "
		" 111 "
		"     "
		"     "
		"     "
		"     ",
	['c'] = 
        "     "
        "     "
        " 111 "
        "1   1"
        "1    "
        "1    "
        "1    "
        "1   1"
        " 111 ",
	['m'] = 
        "     "
        "     "
        "11 11"
        "1 1 1"
        "1 1 1"
        "1   1"
        "1   1"
        "1   1"
        "1   1",
	['z'] =
		"     "
        "     "
        "     "
        "11111"
        "   1 "
        "  1  "
        " 1   "
        "1    "
        "11111",
	['h'] =
		"     "
        "1    "
        "1    "
        "1    "
        "1111 "
        "1   1"
        "1   1"
        "1   1"
        "1   1",

	};


	void drawRect(int x, int y, int w, int h, uint32_t c, uint32_t *data, int stride) {
		for (int _x = x; _x < x + w; _x++) {
			data[_x + y * stride] = c;
			data[_x + (y + h - 1) * stride] = c;
		}
		for (int _y = y; _y < y + h; _y++) {
			data[x + _y * stride] = c;
			data[x + w - 1 + _y * stride] = c;
		}
	}
	
	
	void fillRect(int x, int y, int w, int h, uint32_t c, uint32_t *data, int stride) {
		for (int _y = y; _y < y + h; _y++) {
			for (int _x = x; _x < x + w; _x++) {
				data[_x + _y * stride] = c;
			}
		}
	}
	
	static void blitBitmapText(char* text, int ox, int oy, uint32_t* data, int stride, int width, int height) {
		#define CHAR_WIDTH 5
		#define CHAR_HEIGHT 9
		#define LETTERSPACING 1
	
		int len = strlen(text);
		int w = ((CHAR_WIDTH + LETTERSPACING) * len) - 1;
		int h = CHAR_HEIGHT;
	
		if (ox < 0) ox = width - w + ox;
		if (oy < 0) oy = height - h + oy;
	
		// Clamp to screen bounds (optional but recommended)
		if (ox + w > width) w = width - ox;
		if (oy + h > height) h = height - oy;
	
		// Draw background rectangle (black RGBA8888)
		fillRect(ox, oy, w, h, 0x000000FF, data, stride);
	
		data += oy * stride + ox;
	
		for (int y = 0; y < CHAR_HEIGHT; y++) {
			uint32_t* row = data + y * stride;
			for (int i = 0; i < len; i++) {
				const char* c = bitmap_font[(unsigned char)text[i]];
				for (int x = 0; x < CHAR_WIDTH; x++) {
					if (c[y * CHAR_WIDTH + x] == '1') {
						*row = 0xFFFFFFFF;  // white RGBA8888
					}
					row++;
				}
				row += LETTERSPACING;
			}
		}
	}
	
	
	



void drawGauge(int x, int y, float percent, int width, int height, uint32_t *data, int stride) {
	// Clamp percent to 0.0 - 1.0
	if (percent < 0.0f) percent = 0.0f;
	if (percent > 1.0f) percent = 1.0f;

	uint8_t red   = (uint8_t)(percent * 255.0f);
	uint8_t green = (uint8_t)((1.0f - percent) * 255.0f);
	uint8_t blue  = 0;
	uint8_t alpha = 255;

	uint32_t fillColor = (red << 24) | (green << 16) | (blue << 8) | alpha;
	uint32_t borderColor = 0xFFFFFFFF;  // White RGBA
	uint32_t bgColor = 0x000000FF;      // Black RGBA

	// Background
	fillRect(x, y, width, height, bgColor, data, stride);

	// Filled portion
	int filledWidth = (int)(percent * width);
	fillRect(x, y, filledWidth, height, fillColor, data, stride);

	// Outline
	drawRect(x, y, width, height, borderColor, data, stride);
}



///////////////////////////////

static int cpu_ticks = 0;
static int fps_ticks = 0;
static int use_ticks = 0;
static double fps_double = 0;
static double cpu_double = 0;
static double use_double = 0;
static uint32_t sec_start = 0;

#ifdef USES_SWSCALER
	static int fit = 1;
#else
	static int fit = 0;
#endif	

static void selectScaler(int src_w, int src_h, int src_p) {
	int src_x,src_y,dst_x,dst_y,dst_w,dst_h,dst_p,scale;
	double aspect;
	
	int aspect_w = src_w;
	int aspect_h = CEIL_DIV(aspect_w, core.aspect_ratio);
	
	// TODO: make sure this doesn't break fit==1 devices
	if (aspect_h<src_h) {
		aspect_h = src_h;
		aspect_w = aspect_h * core.aspect_ratio;
		aspect_w += aspect_w % 2;
	}

	char scaler_name[16];
	
	src_x = 0;
	src_y = 0;
	dst_x = 0;
	dst_y = 0;

	// unmodified by crop
	renderer.true_w = src_w;
	renderer.true_h = src_h;
	
	// TODO: this is saving non-rgb30 devices from themselves...or rather, me
	int scaling = screen_scaling;
	if (scaling==SCALE_CROPPED && DEVICE_WIDTH==HDMI_WIDTH) {
		scaling = SCALE_NATIVE;
	}
	
	if (scaling==SCALE_NATIVE || scaling==SCALE_CROPPED) {
		// this is the same whether fit or oversized
		scale = MIN(DEVICE_WIDTH/src_w, DEVICE_HEIGHT/src_h);
		if (!scale) {
			sprintf(scaler_name, "forced crop");
			dst_w = DEVICE_WIDTH;
			dst_h = DEVICE_HEIGHT;
			dst_p = DEVICE_PITCH;
			
			int ox = (DEVICE_WIDTH  - src_w) / 2; // may be negative
			int oy = (DEVICE_HEIGHT - src_h) / 2; // may be negative
			
			if (ox<0) src_x = -ox;
			else dst_x = ox;
			
			if (oy<0) src_y = -oy;
			else dst_y = oy;
		}
		// TODO: this is all kinds of messed up
		// TODO: is this blowing up because the smart has to rotate before scaling?
		// TODO: or is it just that I'm trying to cram 4 logical rects into 2 rect arguments
		// TODO: eg. src.size + src.clip + dst.size + dst.clip
		else if (scaling==SCALE_CROPPED) {
			int scale_x = CEIL_DIV(DEVICE_WIDTH, src_w);
			int scale_y = CEIL_DIV(DEVICE_HEIGHT, src_h);
			scale = MIN(scale_x, scale_y);

			sprintf(scaler_name, "cropped");
			dst_w = DEVICE_WIDTH;
			dst_h = DEVICE_HEIGHT;
			dst_p = DEVICE_PITCH;

			int scaled_w = src_w * scale;
			int scaled_h = src_h * scale;

			int ox = (DEVICE_WIDTH  - scaled_w) / 2; // may be negative
			int oy = (DEVICE_HEIGHT - scaled_h) / 2; // may be negative

			if (ox<0) {
				src_x = -ox / scale;
				src_w -= src_x * 2;
			}
			else {
				dst_x = ox;
				// dst_w -= ox * 2;
			}

			if (oy<0) {
				src_y = -oy / scale;
				src_h -= src_y * 2;
			}
			else {
				dst_y = oy;
				// dst_h -= oy * 2;
			}
		}
		else {
			sprintf(scaler_name, "integer");
			int scaled_w = src_w * scale;
			int scaled_h = src_h * scale;
			dst_w = DEVICE_WIDTH;
			dst_h = DEVICE_HEIGHT;
			dst_p = DEVICE_PITCH;
			dst_x = (DEVICE_WIDTH  - scaled_w) / 2; // should always be positive
			dst_y = (DEVICE_HEIGHT - scaled_h) / 2; // should always be positive
		}
	}
	else if (fit) {
		// these both will use a generic nn scaler
		if (scaling==SCALE_FULLSCREEN) {
			sprintf(scaler_name, "full fit");
			dst_w = DEVICE_WIDTH;
			dst_h = DEVICE_HEIGHT;
			dst_p = DEVICE_PITCH;
			scale = -1; // nearest neighbor
		}
		else {
			double scale_f = MIN(((double)DEVICE_WIDTH)/aspect_w, ((double)DEVICE_HEIGHT)/aspect_h);
			LOG_info("scale_f:%f\n", scale_f);
			
			sprintf(scaler_name, "aspect fit");
			dst_w = aspect_w * scale_f;
			dst_h = aspect_h * scale_f;
			dst_p = DEVICE_PITCH;
			dst_x = (DEVICE_WIDTH  - dst_w) / 2;
			dst_y = (DEVICE_HEIGHT - dst_h) / 2;
			scale = (scale_f==1.0 && dst_w==src_w && dst_h==src_h) ? 1 : -1;
		}
	} 
	else {
		int scale_x = CEIL_DIV(DEVICE_WIDTH, src_w);
		int scale_y = CEIL_DIV(DEVICE_HEIGHT,src_h);
		
		// odd resolutions (eg. PS1 Rayman: 320x239) is throwing this off, need to snap to eights
		int r = (DEVICE_HEIGHT-src_h)%8;
		if (r && r<8) scale_y -= 1;
		
		scale = MAX(scale_x, scale_y);
		// if (scale>4) scale = 4;
		// if (scale>2) scale = 4; // TODO: restore, requires sanity checking
		
		int scaled_w = src_w * scale;
		int scaled_h = src_h * scale;
		
		if (scaling==SCALE_FULLSCREEN) {
			sprintf(scaler_name, "full%i", scale);
			// type = 'full (oversized)';
			dst_w = scaled_w;
			dst_h = scaled_h;
			dst_p = dst_w * FIXED_BPP;
		}
		else if (scaling==SCALE_ASPECT_SCREEN) {
	
			int scale_x = DEVICE_WIDTH / src_w;
			int scale_y = DEVICE_HEIGHT / src_h;
			
			// Use the smaller scale to ensure it fits on screen
			scale = MIN(scale_x, scale_y);
			aspect = (double)src_w / src_h;
			
			// Optionally, clamp to a max scale (e.g., 4x) if needed
			// if (scale > 4) scale = 4;
			
			int scaled_w = src_w * scale;
			int scaled_h = src_h * scale;
			
			// Center the image on screen
			dst_w = scaled_w;
			dst_h = scaled_h;
			dst_x = (DEVICE_WIDTH - dst_w) / 2;
			dst_y = (DEVICE_HEIGHT - dst_h) / 2;
			
			dst_p = dst_w * FIXED_BPP;
			
			sprintf(scaler_name, "raw%i", scale);
			LOG_info("ignore core aspect %ix%i\n\n",dst_w,dst_h);
			
		}
		else {
			double src_aspect_ratio = ((double)src_w) / src_h;
			// double core_aspect_ratio
			double fixed_aspect_ratio = ((double)DEVICE_WIDTH) / DEVICE_HEIGHT;
			int core_aspect = core.aspect_ratio * 1000;
			int fixed_aspect = fixed_aspect_ratio * 1000;
			
			// still having trouble with FC's 1.306 (13/10? wtf) on 4:3 devices
			// specifically I think it has trouble when src, core, and fixed 
			// ratios don't match
			
			// it handles src and core matching but fixed not, eg. GB and GBA 
			// or core and fixed matching but not src, eg. odd PS resolutions
			
			// we need to transform the src size to core aspect
			// then to fixed aspect
						
			if (core_aspect>fixed_aspect) {
				sprintf(scaler_name, "aspect%iL", scale);
				// letterbox
				// dst_w = scaled_w;
				// dst_h = scaled_w / fixed_aspect_ratio;
				// dst_h += dst_h%2;
				int aspect_h = DEVICE_WIDTH / core.aspect_ratio;
				double aspect_hr = ((double)aspect_h) / DEVICE_HEIGHT;
				dst_w = scaled_w;
				dst_h = scaled_h / aspect_hr;

				dst_y = (dst_h - scaled_h) / 2;
			}
			else if (core_aspect<fixed_aspect) {
				sprintf(scaler_name, "aspect%iP", scale);
				// pillarbox
				// dst_w = scaled_h * fixed_aspect_ratio;
				// dst_w += dst_w%2;
				// dst_h = scaled_h;
				aspect_w = DEVICE_HEIGHT * core.aspect_ratio;
				double aspect_wr = ((double)aspect_w) / DEVICE_WIDTH;
				dst_w = scaled_w / aspect_wr;
				dst_h = scaled_h;
				
				dst_w = (dst_w/8)*8;
				dst_x = (dst_w - scaled_w) / 2;
			}
			else {
				sprintf(scaler_name, "aspect%iM", scale);
				// perfect match
				dst_w = scaled_w;
				dst_h = scaled_h;
			}
			dst_p = dst_w * FIXED_BPP;
		}
	}
	
	// TODO: need to sanity check scale and demands on the buffer
	
	// LOG_info("aspect: %ix%i (%f)\n", aspect_w,aspect_h,core.aspect_ratio);
	
	renderer.src_x = src_x;
	renderer.src_y = src_y;
	renderer.src_w = src_w;
	renderer.src_h = src_h;
	renderer.src_p = src_p;
	renderer.dst_x = dst_x;
	renderer.dst_y = dst_y;
	renderer.dst_w = dst_w;
	renderer.dst_h = dst_h;
	renderer.dst_p = dst_p;
	renderer.scale = scale;
	renderer.aspect = (scaling==SCALE_ASPECT_SCREEN) ? aspect: (scaling==SCALE_NATIVE||scaling==SCALE_CROPPED)?0:(scaling==SCALE_FULLSCREEN?-1:core.aspect_ratio);
	renderer.blit = GFX_getScaler(&renderer);
		
	// LOG_info("coreAR:%0.3f fixedAR:%0.3f srcAR: %0.3f\nname:%s\nfit:%i scale:%i\nsrc_x:%i src_y:%i src_w:%i src_h:%i src_p:%i\ndst_x:%i dst_y:%i dst_w:%i dst_h:%i dst_p:%i\naspect_w:%i aspect_h:%i\n",
	// 	core.aspect_ratio, ((double)DEVICE_WIDTH) / DEVICE_HEIGHT, ((double)src_w) / src_h,
	// 	scaler_name,
	// 	fit,scale,
	// 	src_x,src_y,src_w,src_h,src_p,
	// 	dst_x,dst_y,dst_w,dst_h,dst_p,
	// 	aspect_w,aspect_h
	// );

	if (fit) {
		dst_w = DEVICE_WIDTH;
		dst_h = DEVICE_HEIGHT;
	}
	// dont need this anymore with OpenGL
	// if (screen->w!=dst_w || screen->h!=dst_w || screen->pitch!=dst_p) {
		// screen = GFX_resize(dst_w,dst_h,dst_p);
	// }
}
static int firstframe = 1;
static void screen_flip(SDL_Surface* screen) {
	
	if (use_core_fps) {
		GFX_flip_fixed_rate(screen, core.fps);
	}
	else {
		GFX_GL_Swap();
		// GFX_flip(screen);
	}
}


// couple of animation functions for pixel data keeping them all cause wanna use them later
void applyFadeIn(uint32_t **data, size_t pitch, unsigned width, unsigned height, int *frame_counter, int max_frames) {
    size_t pixels_per_row = pitch / sizeof(uint32_t);
    static uint32_t temp_buffer[1920 * 1080];

    if (*frame_counter >= max_frames) {
        return;
    }

    float progress = (float)(*frame_counter) / (float)max_frames;
    float eased = progress * progress * (3 - 2 * progress);

    float fade_alpha = eased;

    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            size_t idx = y * pixels_per_row + x;

            uint32_t color = (*data)[idx];

            uint8_t a = (color >> 24) & 0xFF;
            uint8_t b = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t r = (color >> 0) & 0xFF;

            r = (uint8_t)(r * fade_alpha);
            g = (uint8_t)(g * fade_alpha);
            b = (uint8_t)(b * fade_alpha);
            a = (uint8_t)(a * fade_alpha);

            temp_buffer[idx] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    (*frame_counter)++;
    *data = temp_buffer;
}

void applyZoomFadeIn(uint32_t **data, size_t pitch, unsigned width, unsigned height, int *frame_counter, int max_frames) {
    size_t pixels_per_row = pitch / sizeof(uint32_t);
    static uint32_t temp_buffer[1920 * 1080];

    if (*frame_counter >= max_frames)
        return;

    float progress = (float)(*frame_counter) / (float)max_frames;
    float eased = progress * progress * (3.0f - 2.0f * progress);

    float start_zoom = 6.0f;
    float end_zoom = 1.0f;
    float zoom = start_zoom - eased * (start_zoom - end_zoom);

    float fade_alpha = eased;

    int center_x = width / 2;
    int center_y = height / 2;

    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            float src_x = center_x + (x - center_x) / zoom;
            float src_y = center_y + (y - center_y) / zoom;

            int ix = (int)src_x;
            int iy = (int)src_y;

            size_t dst_idx = y * pixels_per_row + x;
            uint32_t color = 0xFF000000; 

            if (ix >= 0 && ix < (int)width && iy >= 0 && iy < (int)height) {
                size_t src_idx = iy * pixels_per_row + ix;
                color = (*data)[src_idx];
            }

            uint8_t a = (color >> 24) & 0xFF;
            uint8_t b = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t r = (color >> 0) & 0xFF;

            r = (uint8_t)(r * fade_alpha);
            g = (uint8_t)(g * fade_alpha);
            b = (uint8_t)(b * fade_alpha);
            a = (uint8_t)(a * fade_alpha);

            temp_buffer[dst_idx] = (a << 24) | (b << 16) | (g << 8) | r;
        }
    }

    (*frame_counter)++;
    *data = temp_buffer;
}


// Looney Tunes opening effect :D
void applyCircleReveal(uint32_t **data, size_t pitch, unsigned width, unsigned height, int *frame_counter, int max_frames) {
    static uint32_t temp_buffer[1920 * 1080];

    if (*frame_counter >= max_frames)
        return;

    uint32_t *src = *data;
    size_t pixels_per_row = pitch / sizeof(uint32_t);

    float progress = (float)(*frame_counter) / (float)max_frames;
    float eased = progress * progress * (3.0f - 2.0f * progress);

    float max_radius = sqrtf((float)(width * width + height * height)) * 0.5f;
    float radius = eased * max_radius;

    int cx = (int)(width / 2);
    int cy = (int)(height / 2);

    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            size_t idx = y * pixels_per_row + x;

            float dx = (float)x - (float)cx;
            float dy = (float)y - (float)cy;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist <= radius) {
                temp_buffer[idx] = src[idx];
            } else {
                uint32_t color = src[idx];
                uint8_t a = (color >> 24) & 0xFF; 
                temp_buffer[idx] = (a << 24);  
            }
        }
    }

    (*frame_counter)++;
    *data = temp_buffer;
}

static void video_refresh_callback_main(const void *data, unsigned width, unsigned height, size_t pitch) {
	// return;
	
	Special_render();
	
	// static int tmp_frameskip = 0;
	// if ((tmp_frameskip++)%2) return;
	
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
	if (!data) {
		return;
	}

	fps_ticks += 1;
	
	if (downsample) pitch /= 2; // everything expects 16 but we're downsampling from 32
	
	// if source has changed size (or forced by dst_p==0)
	// eg. true src + cropped src + fixed dst + cropped dst
	if (renderer.dst_p==0 || width!=renderer.true_w || height!=renderer.true_h) {
		selectScaler(width, height, pitch);
		// GFX_clearAll();
		GFX_resetShaders();
	}
	
	// debug
	if (show_debug && !isnan(currentratio) && !isnan(currentfps) && !isnan(currentreqfps)  && !isnan(currentbufferms) &&
	currentbuffersize >= 0  && currentbufferfree >= 0 && SDL_GetTicks() > 5000) {
		int x = 2 + renderer.src_x;
		int y = 2 + renderer.src_y;
		char debug_text[250];
		int scale = renderer.scale;
		if (scale==-1) scale = 1; // nearest neighbor flag

		sprintf(debug_text, "%ix%i %ix %i/%i", renderer.src_w,renderer.src_h, scale,currentsampleratein,currentsamplerateout);
		blitBitmapText(debug_text,x,y,(uint32_t*)data,pitch / 4, width,height);
		
		sprintf(debug_text, "%.03f/%i/%.0f/%i", currentratio,
				currentbuffersize,currentbufferms, currentbufferfree);
		blitBitmapText(debug_text, x, y + 14, (uint32_t*)data, pitch / 4, width,
					height);

		sprintf(debug_text, "%i,%i %ix%i", renderer.dst_x,renderer.dst_y, renderer.src_w*scale,renderer.src_h*scale);
		blitBitmapText(debug_text,-x,y,(uint32_t*)data,pitch / 4, width,height);
	
		sprintf(debug_text, "%ix%i", renderer.dst_w,renderer.dst_h);
		blitBitmapText(debug_text,-x,-y,(uint32_t*)data,pitch / 4, width,height);

		//want this to overwrite bottom right in case screen is too small this info more important tbh
		PLAT_getCPUTemp();
		sprintf(debug_text, "%.01f/%.01f/%.0f%%/%ihz/%ic", currentfps, currentreqfps,currentcpuse,currentcpuspeed,currentcputemp);
		blitBitmapText(debug_text,x,-y,(uint32_t*)data,pitch / 4, width,height);

		sprintf(debug_text, "%i/%ix%i/%ix%i/%ix%i", currentshaderpass, currentshadersrcw,currentshadersrch,currentshadertexw,currentshadertexh,currentshaderdstw,currentshaderdsth);
		blitBitmapText(debug_text,x,-y - 14,(uint32_t*)data,pitch / 4, width,height);
	
		double buffer_fill = (double) (currentbuffersize - currentbufferfree) / (double) currentbuffersize;
		drawGauge(x, y + 30, buffer_fill, width / 2, 8, (uint32_t*)data, pitch / 4);
	}
	
	static int frame_counter = 0;
	const int max_frames = 8; 
	if(frame_counter<9) {
		applyFadeIn((uint32_t **) &data, pitch, width, height, &frame_counter, max_frames);
	}

	// LOG_info("video_refresh_callback: %ix%i@%i %ix%i@%i\n",width,height,pitch,screen->w,screen->h,screen->pitch);


	renderer.src = (void*)data;
	renderer.dst = screen->pixels;

	SDL_PauseAudio(0);
	GFX_blitRenderer(&renderer);

	screen_flip(screen);
	last_flip_time = SDL_GetTicks();
}


const void* lastframe = NULL;

static Uint32* rgbaData = NULL;
static size_t rgbaDataSize = 0;

static void video_refresh_callback(const void* data, unsigned width, unsigned height, size_t pitch) {

	// I need to check quit here because sometimes quit is true but callback is still called by the core after and it still runs one more frame and it looks ugly :D
	if(!quit) {
		if (!rgbaData || rgbaDataSize != width * height) {
			if (rgbaData) free(rgbaData);
			rgbaDataSize = width * height;
			rgbaData = (Uint32*)malloc(rgbaDataSize * sizeof(Uint32));
			if (!rgbaData) {
				printf("Failed to allocate memory for RGBA8888 data.\n");
				return;
			}
		}

		if(!fast_forward && data) {
			if(ambient_mode!=0) {
				GFX_setAmbientColor(data, width, height,pitch,ambient_mode);
				LEDS_updateLeds();
			}
		}

		if (!data) {
			if (lastframe) {
				data = lastframe;
			} else {
				return; // No data to display
			}
		} else if (fmt == RETRO_PIXEL_FORMAT_XRGB8888) {
			// convert XRGB8888 to RGBA8888
			const uint32_t* src = (const uint32_t*)data;
			for (unsigned i = 0; i < width * height; ++i) {
				uint32_t pixel = src[i];
				uint8_t r = (pixel >> 16) & 0xFF;
				uint8_t g = (pixel >> 8) & 0xFF;
				uint8_t b = (pixel >> 0) & 0xFF;
				uint8_t a = 0xFF;
				rgbaData[i] = (a << 24) | (b << 16) | (g << 8) | r;
			}
			data = rgbaData;

		} else {
			// convert RGB565 to RGBA8888
			const uint16_t* srcData = (const uint16_t*)data;
			unsigned srcPitchInPixels = pitch / sizeof(uint16_t); 

			for (unsigned y = 0; y < height; ++y) {
				for (unsigned x = 0; x < width; ++x) {
					uint16_t pixel = srcData[y * srcPitchInPixels + x];

					uint8_t r = ((pixel >> 11) & 0x1F) << 3; 
					uint8_t g = ((pixel >> 5) & 0x3F) << 2;   
					uint8_t b = (pixel & 0x1F) << 3;          
					uint8_t a = 0xFF;

					rgbaData[y * width + x] = (a << 24) | (b << 16) | (g << 8) | r;
				}
			}
			data = rgbaData;

		}

		pitch = width * sizeof(Uint32);
		lastframe = data;
		
		video_refresh_callback_main(data,width,height,pitch);
	}
}
///////////////////////////////

static void audio_sample_callback(int16_t left, int16_t right) {
	if (!fast_forward || ff_audio) {
		if (use_core_fps) {
			SND_batchSamples_fixed_rate(&(const SND_Frame){left,right}, 1);
		}
		else {
			SND_batchSamples(&(const SND_Frame){left,right}, 1);
		}
	}
}
static size_t audio_sample_batch_callback(const int16_t *data, size_t frames) { 
	if (!fast_forward || ff_audio) {
		if (use_core_fps) {
			return SND_batchSamples_fixed_rate((const SND_Frame*)data, frames);
		}
		else {
			return SND_batchSamples((const SND_Frame*)data, frames);
		}
	}
	else return frames;
	// return frames;
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
	core.cheat_reset = dlsym(core.handle, "retro_cheat_reset");
	core.cheat_set = dlsym(core.handle, "retro_cheat_set");
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
	

	LOG_info("Block Extract: %d\n", info.block_extract);

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
	sprintf((char*)core.cheats_dir, SDCARD_PATH "/Cheats/%s", core.tag);
	sprintf((char*)core.overlays_dir, SDCARD_PATH "/Overlays/%s", core.tag);
	
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

void Core_applyCheats(struct Cheats *cheats)
{
	if (!cheats)
		return;

	if (!core.cheat_reset || !core.cheat_set)
		return;

	core.cheat_reset();
	for (int i = 0; i < cheats->count; i++) {
		if (cheats->cheats[i].enabled) {
			core.cheat_set(i, cheats->cheats[i].enabled, cheats->cheats[i].code);
		}
	}
}

int Core_updateAVInfo(void) {
	struct retro_system_av_info av_info = {};
	core.get_system_av_info(&av_info);

	double a = av_info.geometry.aspect_ratio;
	if (a<=0) a = (double)av_info.geometry.base_width / av_info.geometry.base_height;

	int changed = (core.fps != av_info.timing.fps || core.sample_rate != av_info.timing.sample_rate || core.aspect_ratio != a);

	core.fps = av_info.timing.fps;
	core.sample_rate = av_info.timing.sample_rate;
	core.aspect_ratio = a;

	if (changed) LOG_info("aspect_ratio: %f (%ix%i) fps: %f\n", a, av_info.geometry.base_width,av_info.geometry.base_height, core.fps);

	return changed;
}

void Core_load(void) {
	LOG_info("Core_load\n");
	struct retro_game_info game_info;
	game_info.path = game.tmp_path[0]?game.tmp_path:game.path;
	game_info.data = game.data;
	game_info.size = game.size;
	LOG_info("game path: %s (%i)\n", game_info.path, game.size);
	core.load_game(&game_info);

	char cheats_path[MAX_PATH] = {0};
	Cheat_getPath(cheats_path);
	if (cheats_path[0] != '\0') {
		LOG_info("cheat file path: %s\n", cheats_path);
		Cheats_load(cheats_path);
		Core_applyCheats(&cheatcodes);
	}

	SRAM_read();
	RTC_read();
	// NOTE: must be called after core.load_game!
	core.set_controller_port_device(0, RETRO_DEVICE_JOYPAD); // set a default, may update after loading configs
	Core_updateAVInfo();
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
		Cheats_free();
		RTC_write();
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
	STATUS_QUIT = 30,
	STATUS_RESET= 31,
};

// TODO: I don't love how overloaded this has become
static struct {
	SDL_Surface* bitmap;
	SDL_Surface* overlay;
	char* items[MENU_ITEM_COUNT];
	char* disc_paths[9]; // up to 9 paths, Arc the Lad Collection is 7 discs
	char minui_dir[256];
	char slot_path[256];
	char base_path[256];
	char bmp_path[256];
	char txt_path[256];
	int disc;
	int total_discs;
	int slot;
	int save_exists;
	int preview_exists;
} menu = {
	.bitmap = NULL,
	.disc = -1,
	.total_discs = 0,
	.save_exists = 0,
	.preview_exists = 0,
	
	.items = {
		[ITEM_CONT] = "Continue",
		[ITEM_SAVE] = "Save",
		[ITEM_LOAD] = "Load",
		[ITEM_OPTS] = "Options",
		[ITEM_QUIT] = "Quit",
	}
};

void Menu_init(void) {
	menu.overlay = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE,DEVICE_WIDTH,DEVICE_HEIGHT,32,SDL_PIXELFORMAT_RGBA8888);
	SDL_SetSurfaceBlendMode(menu.overlay, SDL_BLENDMODE_BLEND);
	Uint32 color = SDL_MapRGBA(menu.overlay->format, 0, 0, 0, 0);
	SDL_FillRect(screen, NULL, color);
	
	char emu_name[256];
	getEmuName(game.path, emu_name);
	sprintf(menu.minui_dir, SHARED_USERDATA_PATH "/.minui/%s", emu_name);
	mkdir(menu.minui_dir, 0755);

	sprintf(menu.slot_path, "%s/%s.txt", menu.minui_dir, game.name);
	
	if (simple_mode) menu.items[ITEM_OPTS] = "Reset";
	
	if (game.m3u_path[0]) {
		char* tmp;
		strcpy(menu.base_path, game.m3u_path);
		tmp = strrchr(menu.base_path, '/') + 1;
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
				strcpy(disc_path, menu.base_path);
				tmp = disc_path + strlen(disc_path);
				strcpy(tmp, line);
				
				// found a valid disc path
				if (exists(disc_path)) {
					menu.disc_paths[menu.total_discs] = strdup(disc_path);
					// matched our current disc
					if (exactMatch(disc_path, game.path)) {
						menu.disc = menu.total_discs;
					}
					menu.total_discs += 1;
				}
			}
			fclose(file);
		}
	}
}
void Menu_quit(void) {
	SDL_FreeSurface(menu.overlay);
}
void Menu_beforeSleep() {
	LOG_info("beforeSleep\n");
	SRAM_write();
	RTC_write();
	State_autosave();
	putFile(AUTO_RESUME_PATH, game.path + strlen(SDCARD_PATH));
	PWR_setCPUSpeed(CPU_SPEED_MENU);
}
void Menu_afterSleep() {
	LOG_info("beforeSleep\n");
	unlink(AUTO_RESUME_PATH);
	setOverclock(overclock);
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
	char* category; // currently displayed category
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
		
		PWR_update(&dirty, NULL, Menu_beforeSleep, Menu_afterSleep);
		
	
		GFX_clear(screen);
		GFX_blitMessage(font.medium, message, screen, &(SDL_Rect){0,SCALE1(PADDING),screen->w,screen->h-SCALE1(PILL_SIZE+PADDING)});
		GFX_blitButtonGroup(pairs, 0, screen, 1);
		GFX_flip(screen);
		dirty = 0;
		
		
		hdmimon();
	}
	GFX_setMode(MODE_MENU);
	return MENU_CALLBACK_NOP; // TODO: this should probably be an arg
}

static int Menu_options(MenuList* list);

static int MenuList_freeItems(MenuList* list, int i) {
	// TODO: what calls this? do menu's register for needing it? then call it on quit for each?
	if (list->items) free(list->items);
	return MENU_CALLBACK_NOP;
}

static int OptionFrontend_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	Config_syncFrontend(item->key, item->value);
	return MENU_CALLBACK_NOP;
}
static MenuList OptionFrontend_menu = {
	.type = MENU_VAR,
	.on_change = OptionFrontend_optionChanged,
	.items = NULL,
};
static int OptionFrontend_openMenu(MenuList* list, int i) {
	if (OptionFrontend_menu.items==NULL) {
		// TODO: where do I free this? I guess I don't :sweat_smile:
		if (!config.frontend.enabled_count) {
			int enabled_count = 0;
			for (int i=0; i<config.frontend.count; i++) {
				if (!config.frontend.options[i].lock) enabled_count += 1;
			}
			config.frontend.enabled_count = enabled_count;
			config.frontend.enabled_options = calloc(enabled_count+1, sizeof(Option*));
			int j = 0;
			for (int i=0; i<config.frontend.count; i++) {
				Option* item = &config.frontend.options[i];
				if (item->lock) continue;
				config.frontend.enabled_options[j] = item;
				j += 1;
			}
		}
		OptionFrontend_menu.items = calloc(config.frontend.enabled_count+1, sizeof(MenuItem));
		for (int j=0; j<config.frontend.enabled_count; j++) {
			Option* option = config.frontend.enabled_options[j];
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
		for (int j=0; j<config.frontend.enabled_count; j++) {
			Option* option = config.frontend.enabled_options[j];
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

static int OptionEmulator_openMenu(MenuList* list, int i);
static int OptionEmulator_optionDetail(MenuList* list, int i);

static MenuList OptionEmulator_menu = {
	.type = MENU_FIXED,
	.on_confirm = OptionEmulator_optionDetail, // TODO: this needs pagination to be truly useful
	.on_change = OptionEmulator_optionChanged,
	.items = NULL,
};

static int OptionEmulator_optionDetail(MenuList* list, int i) {
	MenuItem* item = &list->items[i];

	if (item->values == NULL) {
		// This is a category item
		// Display the corresponding submenu
		list->category = item->key;
		LOG_info("%s: displaying category %s\n", __FUNCTION__, item->key);

		int prev_enabled_count = config.core.enabled_count;
		Option **prev_enabled = config.core.enabled_options;
		MenuItem *prev_items = OptionEmulator_menu.items;

		OptionEmulator_openMenu(list, 0);
		list->category = NULL;

		config.core.enabled_count = prev_enabled_count ;
		config.core.enabled_options = prev_enabled ;
		OptionEmulator_menu.items = prev_items;

		LOG_info("%s: back to root menu\n", __FUNCTION__);
	}
	else {
		Option* option = OptionList_getOption(&config.core, item->key);
		if (option->full) return Menu_message(option->full, (char*[]){ "B","BACK", NULL });
		else return MENU_CALLBACK_NOP;
	}
}

static int OptionEmulator_openMenu(MenuList* list, int index) {
	LOG_info("%s: limit to category %s\n", __FUNCTION__, list->category ? list->category : "<all>");

	if (list->category == NULL) {
		if (core.update_visibility_callback) {
			LOG_info("%s: calling update visibility callback\n", __FUNCTION__);
			core.update_visibility_callback();
		}
	}

	int enabled_count = 0;
	config.core.enabled_options = calloc(config.core.count + 1, sizeof(Option*));
	for (int i=0; i<config.core.count; i++) {
		Option *item = &config.core.options[i];

		// Exclude locked and hidden items
		if (item->lock || item->hidden) {
			continue;
		}
		// Restrict to the current category
		if (list->category == NULL && item->category) {
			continue;
		}
		if (list->category && (item->category == NULL || strcmp(item->category, list->category))) {
			continue;
		}

		config.core.enabled_options[enabled_count++] = item;
	}
	config.core.enabled_count = enabled_count;
	config.core.enabled_options = realloc(config.core.enabled_options, sizeof(Option *) * (enabled_count + 1));

	// If we are at the top level, add the categories
	int cat_count = 0;

	if (list->category == NULL && config.core.categories) {
		while (config.core.categories[cat_count].key) {
			cat_count++;
		}
	}

	OptionEmulator_menu.items = calloc(cat_count + config.core.enabled_count + 1, sizeof(MenuItem));

	for (int i=0; i<cat_count; i++) {
		OptionCategory *cat = &config.core.categories[i];
		MenuItem *item = &OptionEmulator_menu.items[i];
		item->key = cat->key;
		item->name = cat->desc;
		item->desc = cat->info;
	}

	for (int i=0; i<config.core.enabled_count; i++) {
		Option *option = config.core.enabled_options[i];
		MenuItem *item = &OptionEmulator_menu.items[cat_count + i];
		item->key = option->key;
		item->name = option->name;
		item->desc = option->desc;
		item->value = option->value;
		item->values = option->labels;
	}
	
	if (cat_count || config.core.enabled_count) {
		Menu_options(&OptionEmulator_menu);
		free(OptionEmulator_menu.items);
		free(config.core.enabled_options);
		OptionEmulator_menu.items = NULL;
		config.core.enabled_count = 0;
		config.core.enabled_options = NULL;
	}
	else {
		if (list->category) {
			Menu_message("This category has no options.", (char*[]){ "B","BACK", NULL });
		}
		else {
			Menu_message("This core has no options.", (char*[]){ "B","BACK", NULL });
		}
	}
	
	return MENU_CALLBACK_NOP;
}

int OptionControls_bind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=button_labels) {
		// LOG_info("changed gamepad_type\n");
		return MENU_CALLBACK_NOP;
	}
	
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
		GFX_delay();
		hdmimon();
	}
	return MENU_CALLBACK_NEXT_ITEM;
}
static int OptionControls_unbind(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=button_labels) return MENU_CALLBACK_NOP;
	
	ButtonMapping* button = &config.controls[item->id];
	button->local = -1;
	button->mod = 0;
	return MENU_CALLBACK_NOP;
}
static int OptionControls_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	if (item->values!=gamepad_labels) return MENU_CALLBACK_NOP;

	if (has_custom_controllers) {
		gamepad_type = item->value;
		int device = strtol(gamepad_values[item->value], NULL, 0);
		core.set_controller_port_device(0, device);
	}
	return MENU_CALLBACK_NOP;
}
static MenuList OptionControls_menu = {
	.type = MENU_INPUT,
	.desc = "Press A to set and X to clear."
		"\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
	,
	.on_confirm = OptionControls_bind,
	.on_change = OptionControls_unbind,
	.items = NULL
};

static int OptionControls_openMenu(MenuList* list, int i) {
	LOG_info("OptionControls_openMenu\n");

	if (OptionControls_menu.items==NULL) {
		
		// TODO: where do I free this?
		OptionControls_menu.items = calloc(RETRO_BUTTON_COUNT+1+has_custom_controllers, sizeof(MenuItem));
		int k = 0;
		
		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->name = "Controller";
			item->desc = "Select the type of controller.";
			item->value = gamepad_type;
			item->values = gamepad_labels;
			item->on_change = OptionControls_optionChanged;
		}
		
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
			
			//LOG_info("\t%s (%i:%i)\n", button->name, button->local, button->retro);
			
			MenuItem* item = &OptionControls_menu.items[k++];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
			item->values = button_labels;
		}
	}
	else {
		// update values
		int k = 0;
		
		if (has_custom_controllers) {
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = gamepad_type;
		}
		
		for (int j=0; config.controls[j].name; j++) {
			ButtonMapping* button = &config.controls[j];
			if (button->ignore) continue;
			
			MenuItem* item = &OptionControls_menu.items[k++];
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
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
		GFX_delay();
		hdmimon();
	}
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
	.desc = "Press A to set and X to clear." 
		"\nSupports single button and MENU+button." // TODO: not supported on nano because POWER doubles as MENU
	,
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
		// TODO: where do I free this? I guess I don't :sweat_smile:
		OptionShortcuts_menu.items = calloc(SHORTCUT_COUNT+1, sizeof(MenuItem));
		for (int j=0; config.shortcuts[j].name; j++) {
			ButtonMapping* button = &config.shortcuts[j];
			MenuItem* item = &OptionShortcuts_menu.items[j];
			item->id = j;
			item->name = button->name;
			item->desc = NULL;
			item->value = button->local + 1;
			if (button->mod) item->value += LOCAL_BUTTON_COUNT;
			item->values = button_labels;
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

static int OptionQuicksave_onConfirm(MenuList* list, int i) {
	Menu_beforeSleep();
	PWR_powerOff();
}

static int OptionCheats_optionChanged(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	struct Cheat *cheat = &cheatcodes.cheats[i];
	cheat->enabled = item->value;
	Core_applyCheats(&cheatcodes);
	return MENU_CALLBACK_NOP;
}

static int OptionCheats_optionDetail(MenuList* list, int i) {
	MenuItem* item = &list->items[i];
	struct Cheat *cheat = &cheatcodes.cheats[i];
	if (cheat->info) 
		return Menu_message((char*)cheat->info, (char*[]){ "B","BACK", NULL });
	else return MENU_CALLBACK_NOP;
}

static MenuList OptionCheats_menu = {
	.type = MENU_FIXED,
	.on_confirm = OptionCheats_optionDetail, // TODO: this needs pagination to be truly useful
	.on_change = OptionCheats_optionChanged,
	.items = NULL,
};
static int OptionCheats_openMenu(MenuList* list, int i) {
	if (OptionCheats_menu.items == NULL) {
		// populate
		OptionCheats_menu.items = calloc(cheatcodes.count + 1, sizeof(MenuItem));
		for (int i = 0; i<cheatcodes.count; i++) {
			struct Cheat *cheat = &cheatcodes.cheats[i];
			MenuItem *item = &OptionCheats_menu.items[i];

			// this stuff gets actually copied around.. what year is it?
			int len = strlen(cheat->name) + 1;
			item->name = calloc(len, sizeof(char));
			strcpy(item->name, cheat->name);

			if(cheat->info) {
				len = strlen(cheat->info) + 1;
				item->desc = calloc(len, sizeof(char));
				strncpy(item->desc, cheat->info, len);
				
				// these magic numbers are more about chars per line than pixel width 
				// so it's not going to be relative to the screen size, only the scale
				// what does that even mean?
				GFX_wrapText(font.tiny, item->desc, SCALE1(240), 2); // TODO magic number!
			}

			item->value = cheat->enabled;
			item->values = onoff_labels;
		}
	}
	else {
		// update
		for (int j = 0; j < cheatcodes.count; j++) {
			struct Cheat *cheat = &cheatcodes.cheats[i];
			MenuItem *item = &OptionCheats_menu.items[i];
			// I guess that makes sense, nobody is changing these but us - what about state restore?
			if(!cheat->enabled)
				continue;
			item->value = cheat->enabled;
		}
	}

	if (OptionCheats_menu.items[0].name) {
		Menu_options(&OptionCheats_menu);
	}
	else {
		Menu_message("No cheat file loaded.", (char*[]){ "B","BACK", NULL });
	}
	
	return MENU_CALLBACK_NOP;
}





static int OptionPragmas_optionChanged(MenuList* list, int i) {
		MenuItem* item = &list->items[i];
		for (int i=0; i < config.shaders.options[SH_NROFSHADERS].value; i++) {
			ShaderParam *params = PLAT_getShaderPragmas(i);
			for (int j = 0; j < 32; j++) {
				if (exactMatch(params[j].name, item->key)) {
					params[j].value = strtof(item->values[item->value], NULL);
				}
			}
		}
		for (int i = 0; i < config.shaderpragmas.count; i++) {
			MenuItem* item = &list->items[i];
			config.shaderpragmas.options[i].value = item->value ;

		}
		return MENU_CALLBACK_NOP;
}

static MenuList PragmasOptions_menu = {
	.type = MENU_FIXED,
	.on_confirm = NULL,
	.on_change = OptionPragmas_optionChanged,
	.items = NULL
};
static int OptionPragmas_openMenu(MenuList* list, int i) {
	LOG_info("OptionPragmas oppenen\n");
	
	PragmasOptions_menu.items = calloc(config.shaderpragmas.count + 1, sizeof(MenuItem));

	for (int i = 0; i < config.shaderpragmas.count; i++) {
		
		MenuItem* item = &PragmasOptions_menu.items[i];
		Option* configitem = &config.shaderpragmas.options[i];
		item->id = i;
		item->name = configitem->name;
		item->desc = configitem->desc;
		item->value = configitem->value;
		item->key = configitem->key;
		item->values = configitem->values;
	
	}
	
	
	if (PragmasOptions_menu.items[0].name) {
		Menu_options(&PragmasOptions_menu);
	} else {
		Menu_message("No extra settings found", (char*[]){"B", "BACK", NULL});
	}

	return MENU_CALLBACK_NOP;
}
static int OptionShaders_optionChanged(MenuList* list, int i) {
		MenuItem* item = &list->items[i];
		Config_syncShaders(item->key, item->value);

		for (int i = 0; i < config.shaders.count; i++) {
			MenuItem* item = &list->items[i];
			item->value = config.shaders.options[i].value;

		}
		initShaders();
		return MENU_CALLBACK_NOP;
}

static MenuList ShaderOptions_menu = {
	.type = MENU_FIXED,
	.on_confirm = NULL,
	.on_change = OptionShaders_optionChanged,
	.items = NULL
};

static int OptionShaders_openMenu(MenuList* list, int i) {
	LOG_info("OptionShaders_openMenu\n");

	
		int filecount;
		char** filelist = list_files_in_folder(SHADERS_FOLDER "/glsl", &filecount,NULL);


	// Check if folder read failed or no files found
	if (!filelist || filecount == 0) {
		Menu_message("No shaders available\n/Shaders folder or shader files not found", (char*[]){"B", "BACK", NULL});
		return MENU_CALLBACK_NOP;
	}

	// NULL-terminate filelist just in case
	filelist = realloc(filelist, sizeof(char*) * (filecount + 1));
	filelist[filecount] = NULL;

	ShaderOptions_menu.items = calloc(config.shaders.count + 1, sizeof(MenuItem));
	for (int i = 0; i < config.shaders.count; i++) {
		MenuItem* item = &ShaderOptions_menu.items[i];
		Option* configitem = &config.shaders.options[i];
		item->id = i;
		item->name = configitem->name;
		item->desc = configitem->desc;
		item->value = configitem->value;
		item->key = configitem->key;
		if(i == SH_EXTRASETTINGS) item->on_confirm = OptionPragmas_openMenu;

		if (strcmp(config.shaders.options[i].key, "minarch_shader1") == 0 ||
			strcmp(config.shaders.options[i].key, "minarch_shader2") == 0 ||
			strcmp(config.shaders.options[i].key, "minarch_shader3") == 0) {
			item->values = filelist;
			config.shaders.options[i].values = filelist;
		} else {
			item->values = config.shaders.options[i].values;
		}
	}
	

	if (ShaderOptions_menu.items[0].name) {
		Menu_options(&ShaderOptions_menu);
	} else {
		Menu_message("No shaders available\n/Shaders folder or shader files not found", (char*[]){"B", "BACK", NULL});
	}

	return MENU_CALLBACK_NOP;
}



static MenuList options_menu = {
	.type = MENU_LIST,
	.items = (MenuItem[]) {
		{"Frontend", "NextUI (" BUILD_DATE " " BUILD_HASH ")",.on_confirm=OptionFrontend_openMenu},
		{"Emulator",.on_confirm=OptionEmulator_openMenu},
		{"Shaders",.on_confirm=OptionShaders_openMenu},
		// TODO: this should be hidden with no cheats available
		{"Cheats",.on_confirm=OptionCheats_openMenu},
		{"Controls",.on_confirm=OptionControls_openMenu},
		{"Shortcuts",.on_confirm=OptionShortcuts_openMenu}, 
		{"Save Changes",.on_confirm=OptionSaveChanges_openMenu},
		{NULL},
		{NULL},
	}
};

static void OptionSaveChanges_updateDesc(void) {
	options_menu.items[4].desc = getSaveDesc();
}

#define OPTION_PADDING 8

static int Menu_options(MenuList* list) {
	MenuItem* items = list->items;
	int type = list->type;

	int dirty = 1;
	int show_options = 1;
	int show_settings = 0;
	int await_input = 0;
	
	// dependent on option list offset top and bottom, eg. the gray triangles
	int max_visible_options = (screen->h - ((SCALE1(PADDING + PILL_SIZE) * 2) + SCALE1(BUTTON_SIZE))) / SCALE1(BUTTON_SIZE); // 7 for 480, 10 for 720
	
	int count;
	for (count=0; items[count].name; count++);
	int selected = 0;
	int start = 0;
	int end = MIN(count,max_visible_options);
	int visible_rows = end;
	
	OptionSaveChanges_updateDesc();
	
	int defer_menu = false;
	while (show_options) {
		if (await_input) {
			defer_menu = true;
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
				start = MAX(0,count - max_visible_options);
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
		else {
			MenuItem* item = &items[selected];
			if (item->values && item->values!=button_labels) { // not an input binding
				if (PAD_justRepeated(BTN_LEFT)) {
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
					// first check if its not out of bounds already
					int i = 0;
					while (item->values[i]) i++; 
					if (item->value >= i) item->value = 0;
				
					if (item->values[item->value+1]) item->value += 1;
					else item->value = 0;
				
					if (item->on_change) item->on_change(list, selected);
					else if (list->on_change) list->on_change(list, selected);
				
					dirty = 1;
				}
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
			// eg. set a flag to call on_confirm at the beginning of the next frame?
			else if (list->on_confirm) {
				if (item->values==button_labels) await_input = 1; // button binding
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
		
		if (!defer_menu) PWR_update(&dirty, &show_settings, Menu_beforeSleep, Menu_afterSleep);
		
		if (defer_menu && PAD_justReleased(BTN_MENU)) defer_menu = false;
		
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
					
					GFX_blitPillDark(ASSET_BUTTON, screen, &(SDL_Rect){
						ox,
						oy+SCALE1(j*BUTTON_SIZE),
						w,
						SCALE1(BUTTON_SIZE)
					});
					text_color = uintToColour(THEME_COLOR5_255);
					
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
					GFX_blitPillLight(ASSET_BUTTON, screen, &(SDL_Rect){
						ox,
						oy+SCALE1(j*BUTTON_SIZE),
						mw,
						SCALE1(BUTTON_SIZE)
					});
				}
				
				if (item->values == NULL) {
					// This is a navigation item, used to displayed a specific category
					text = TTF_RenderUTF8_Blended(font.small, ">", COLOR_WHITE); // always white
					SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
						ox + mw - text->w - SCALE1(OPTION_PADDING),
						oy+SCALE1((j*BUTTON_SIZE)+3)
					});
					SDL_FreeSurface(text);
				}
				else {
					if (item->value>=0) {
						int count = 0;
						while ( item->values && item->values[count]) count++;
						if (item->value >= 0 && item->value < count) {
							const char *str = item->values[item->value];
							text = TTF_RenderUTF8_Blended(font.tiny, str ? str : "none", str ? COLOR_WHITE : COLOR_GRAY); // always white
							if (text) {
								SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
									ox + mw - text->w - SCALE1(OPTION_PADDING),
									oy+SCALE1((j*BUTTON_SIZE)+3)
								});
								SDL_FreeSurface(text);
							}
						}
					}
				}
				
				// TODO: blit a black pill on unselected rows (to cover longer item->values?) or truncate longer item->values?
				if (j==selected_row) {
					// white pill
					int w = 0;
					TTF_SizeUTF8(font.small, item->name, &w, NULL);
					w += SCALE1(OPTION_PADDING*2);
					GFX_blitPillDark(ASSET_BUTTON, screen, &(SDL_Rect){
						ox,
						oy+SCALE1(j*BUTTON_SIZE),
						w,
						SCALE1(BUTTON_SIZE)
					});
					text_color = uintToColour(THEME_COLOR5_255);
					
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
						if(item->values) {
							for (int j=0; item->values[j]; j++) {
								TTF_SizeUTF8(font.tiny, item->values[j], &rw, NULL);
								if (lw+rw>w) w = lw+rw;
								if (rw>mrw) mrw = rw;
							}
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
					GFX_blitPillLight(ASSET_BUTTON, screen, &(SDL_Rect){
						ox,
						oy+SCALE1(j*BUTTON_SIZE),
						mw,
						SCALE1(BUTTON_SIZE)
					});
					
					// white pill
					int w = 0;
					TTF_SizeUTF8(font.small, item->name, &w, NULL);
					w += SCALE1(OPTION_PADDING*2);
					GFX_blitPillDark(ASSET_BUTTON, screen, &(SDL_Rect){
						ox,
						oy+SCALE1(j*BUTTON_SIZE),
						w,
						SCALE1(BUTTON_SIZE)
					});
					text_color = uintToColour(THEME_COLOR5_255);
					
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
					int count = 0;
					while ( item->values && item->values[count]) count++;
					if (item->value >= 0 && item->value < count) {
						text = TTF_RenderUTF8_Blended(font.tiny, item->values[item->value], COLOR_WHITE); // always white
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							ox + mw - text->w - SCALE1(OPTION_PADDING),
							oy+SCALE1((j*BUTTON_SIZE)+3)
						});
						SDL_FreeSurface(text);
					}
				}
			}
		}
		
		if (count>max_visible_options) {
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
		
		hdmimon();
	}
	
	// GFX_clearAll();
	// GFX_flip(screen);
	
	return 0;
}

static void Menu_scale(SDL_Surface* src, SDL_Surface* dst) {
	// LOG_info("Menu_scale src: %ix%i dst: %ix%i\n", src->w,src->h,dst->w,dst->h);
	
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
	
	int scaling = screen_scaling;
	if (scaling==SCALE_CROPPED && DEVICE_WIDTH==HDMI_WIDTH) {
		scaling = SCALE_NATIVE;
	}
	if (scaling==SCALE_NATIVE) {
		// LOG_info("native\n");
		
		rx = renderer.dst_x;
		ry = renderer.dst_y;
		rw = renderer.src_w;
		rh = renderer.src_h;
		if (renderer.scale) {
			// LOG_info("scale: %i\n", renderer.scale);
			rw *= renderer.scale;
			rh *= renderer.scale;
		}
		else {
			// LOG_info("forced crop\n"); // eg. fc on nano, vb on smart
			rw -= renderer.src_x * 2;
			rh -= renderer.src_y * 2;
			sw = rw;
			sh = rh;
		}
		
		if (dw==DEVICE_WIDTH/2) {
			// LOG_info("halve\n");
			rx /= 2;
			ry /= 2;
			rw /= 2;
			rh /= 2;
		}
	}
	else if (scaling==SCALE_CROPPED) {
		// LOG_info("cropped\n");
		sw -= renderer.src_x * 2;
		sh -= renderer.src_y * 2;

		rx = renderer.dst_x;
		ry = renderer.dst_y;
		rw = sw * renderer.scale;
		rh = sh * renderer.scale;
		
		if (dw==DEVICE_WIDTH/2) {
			// LOG_info("halve\n");
			rx /= 2;
			ry /= 2;
			rw /= 2;
			rh /= 2;
		}
	}
	
	if (scaling==SCALE_ASPECT || rw>dw || rh>dh) {
		// LOG_info("aspect\n");
		double fixed_aspect_ratio = ((double)DEVICE_WIDTH) / DEVICE_HEIGHT;
		int core_aspect = core.aspect_ratio * 1000;
		int fixed_aspect = fixed_aspect_ratio * 1000;
		
		if (core_aspect>fixed_aspect) {
			// LOG_info("letterbox\n");
			rw = dw;
			rh = rw / core.aspect_ratio;
			rh += rh%2;
		}
		else if (core_aspect<fixed_aspect) {
			// LOG_info("pillarbox\n");
			rh = dh;
			rw = rh * core.aspect_ratio;
			rw += rw%2;
			rw = (rw/8)*8; // probably necessary here since we're not scaling by an integer
		}
		else {
			// LOG_info("perfect match\n");
			rw = dw;
			rh = dh;
		}
		
		rx = (dw - rw) / 2;
		ry = (dh - rh) / 2;
	}
	
	// LOG_info("Menu_scale (r): %i,%i %ix%i\n",rx,ry,rw,rh);
	// LOG_info("offset: %i,%i\n", renderer.src_x, renderer.src_y);

	// dumb nearest neighbor scaling
	int mx = (sw << 16) / rw;
	int my = (sh << 16) / rh;
	int ox = (renderer.src_x << 16);
	int sx = ox;
	int sy = (renderer.src_y << 16);
	int lr = -1;
	int sr = 0;
	int dr = ry * dp;
	int cp = dp * FIXED_BPP;
	
	// LOG_info("Menu_scale (s): %i,%i %ix%i\n",sx,sy,sw,sh);
	// LOG_info("mx:%i my:%i sx>>16:%i sy>>16:%i\n",mx,my,((sx+mx) >> 16),((sy+my) >> 16));

	for (int dy=0; dy<rh; dy++) {
		sx = ox;
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

static void Menu_initState(void) {
	if (exists(menu.slot_path)) menu.slot = getInt(menu.slot_path);
	if (menu.slot==8) menu.slot = 0;
	
	menu.save_exists = 0;
	menu.preview_exists = 0;
}
static void Menu_updateState(void) {
	// LOG_info("Menu_updateState\n");

	int last_slot = state_slot;
	state_slot = menu.slot;

	char save_path[256];
	State_getPath(save_path);

	state_slot = last_slot;

	sprintf(menu.bmp_path, "%s/%s.%d.bmp", menu.minui_dir, game.name, menu.slot);
	sprintf(menu.txt_path, "%s/%s.%d.txt", menu.minui_dir, game.name, menu.slot);
	
	menu.save_exists = exists(save_path);
	menu.preview_exists = menu.save_exists && exists(menu.bmp_path);

	// LOG_info("save_path: %s (%i)\n", save_path, menu.save_exists);
	// LOG_info("bmp_path: %s txt_path: %s (%i)\n", menu.bmp_path, menu.txt_path, menu.preview_exists);
}

typedef struct {
    char* pixels;
    char* path;
	int w;
	int h;
} SaveImageArgs;

int save_screenshot_thread(void* data) {

    SaveImageArgs* args = (SaveImageArgs*)data;
	SDL_Surface* rawSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		args->pixels, args->w, args->h, 32, args->w * 4, SDL_PIXELFORMAT_ABGR8888
	);
	SDL_Surface* converted = SDL_ConvertSurfaceFormat(rawSurface, SDL_PIXELFORMAT_RGBA8888, 0);
	SDL_FreeSurface(rawSurface);

    SDL_RWops* rw = SDL_RWFromFile(args->path, "wb");
    if (!rw) {
        SDL_Log("Failed to open file for writing: %s", SDL_GetError());
    } else {
        if (IMG_SavePNG_RW(converted, rw, 1) != 0) {
            SDL_Log("Failed to save PNG: %s", SDL_GetError());
        }
    }
	LOG_info("saved screenshot\n");
	SDL_FreeSurface(converted);
    free(args->path);
    free(args->pixels);
    free(args);
    return 0;
}
SDL_Thread* screenshotsavethread;
static void Menu_saveState(void) {
	// LOG_info("Menu_saveState\n");
	if(quit) {
		SDL_PauseAudio(1);
	}
	Menu_updateState();
	
	if (menu.total_discs) {
		char* disc_path = menu.disc_paths[menu.disc];
		putFile(menu.txt_path, disc_path + strlen(menu.base_path));
	}
	
	// if already in menu use menu.bitmap instead for saving screenshots otherwise create new one on the fly
	if (newScreenshot) {
		int cw, ch;
		unsigned char* pixels = GFX_GL_screenCapture(&cw, &ch);
		SaveImageArgs* args = malloc(sizeof(SaveImageArgs));
		args->pixels = pixels;
		args->w = cw;
		args->h = ch;
		args->path = SDL_strdup(menu.bmp_path); 
		SDL_WaitThread(screenshotsavethread, NULL);
		screenshotsavethread = SDL_CreateThread(save_screenshot_thread, "SaveScreenshotThread", args);
		newScreenshot = 0;
	} else {
		SDL_RWops* rw = SDL_RWFromFile(menu.bmp_path, "wb");
		IMG_SavePNG_RW(menu.bitmap, rw,1);
		LOG_info("saved screenshot\n");
	}
	
	state_slot = menu.slot;
	putInt(menu.slot_path, menu.slot);
	State_write();
}
static void Menu_loadState(void) {
	Menu_updateState();
	
	if (menu.save_exists) {
		if (menu.total_discs) {
			char slot_disc_name[256];
			getFile(menu.txt_path, slot_disc_name, 256);
		
			char slot_disc_path[256];
			if (slot_disc_name[0]=='/') strcpy(slot_disc_path, slot_disc_name);
			else sprintf(slot_disc_path, "%s%s", menu.base_path, slot_disc_name);
		
			char* disc_path = menu.disc_paths[menu.disc];
			if (!exactMatch(slot_disc_path, disc_path)) {
				Game_changeDisc(slot_disc_path);
			}
		}
	
		state_slot = menu.slot;
		putInt(menu.slot_path, menu.slot);
		State_read();
	}
}

static char* getAlias(char* path, char* alias) {
	// LOG_info("alias path: %s\n", path);
	char* tmp;
	char map_path[256];
	strcpy(map_path, path);
	tmp = strrchr(map_path, '/');
	if (tmp) {
		tmp += 1;
		strcpy(tmp, "map.txt");
		// LOG_info("map_path: %s\n", map_path);
	}
	char* file_name = strrchr(path,'/');
	if (file_name) file_name += 1;
	// LOG_info("file_name: %s\n", file_name);
	
	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			char line[256];
			while (fgets(line,256,file)!=NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line)==0) continue; // skip empty lines
			
				tmp = strchr(line,'\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp+1;
					if (exactMatch(file_name,key)) {
						strcpy(alias, value);
						break;
					}
				}
			}
			fclose(file);
		}
	}
}

static void Menu_loop(void) {

	int cw, ch;
	unsigned char* pixels = GFX_GL_screenCapture(&cw, &ch);
	
	renderer.dst = pixels;
	SDL_Surface* rawSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pixels, cw, ch, 32, cw * 4, SDL_PIXELFORMAT_ABGR8888
	);
	SDL_Surface* converted = SDL_ConvertSurfaceFormat(rawSurface, SDL_PIXELFORMAT_RGBA8888, 0);
	SDL_FreeSurface(rawSurface);
	free(pixels); 

	menu.bitmap = converted;
	SDL_Surface* backing = SDL_CreateRGBSurfaceWithFormat(0,DEVICE_WIDTH,DEVICE_HEIGHT,32,SDL_PIXELFORMAT_RGBA8888); 
	

	SDL_Rect dst = {
		0,
		0,
		screen->w,
		screen->h
	};
	SDL_BlitScaled(menu.bitmap, NULL, backing, &dst);
	
	int restore_w = screen->w;
	int restore_h = screen->h;
	int restore_p = screen->pitch;
	if (restore_w!=DEVICE_WIDTH || restore_h!=DEVICE_HEIGHT) {
		screen = GFX_resize(DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
	}

	char act[PATH_MAX];
	sprintf(act, "gametimectl.elf stop '%s' &", replaceString2(game.path, "'", "'\\''"));
	system(act);

	SRAM_write();
	RTC_write();
	PWR_warn(0);
	if (!HAS_POWER_BUTTON) PWR_enableSleep();
	PWR_setCPUSpeed(CPU_SPEED_MENU); // set Hz directly

	GFX_setEffect(EFFECT_NONE);
	
	int rumble_strength = VIB_getStrength();
	VIB_setStrength(0);
	
	PWR_enableAutosleep();
	PAD_reset();
	
	
	// path and string things
	char* tmp;
	char rom_name[256]; // without extension or cruft
	getDisplayName(game.name, rom_name);
	getAlias(game.path, rom_name);
	
	int rom_disc = -1;
	char disc_name[16];
	if (menu.total_discs) {
		rom_disc = menu.disc;
		sprintf(disc_name, "Disc %i", menu.disc+1);
	}
		
	int selected = 0; // resets every launch
	Menu_initState();
	
	int status = STATUS_CONT; // TODO: no longer used?
	int show_setting = 0;
	int dirty = 1;
	int ignore_menu = 0;
	int menu_start = 0;
	SDL_Surface* preview = SDL_CreateRGBSurface(SDL_SWSURFACE,DEVICE_WIDTH/2,DEVICE_HEIGHT/2,32,RGBA_MASK_8888); // TODO: retain until changed?

	LEDS_initLeds();
	LEDS_updateLeds();

	//set vid.blit to null for menu drawing no need for blitrender drawing
	GFX_clearShaders();
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
			if (menu.total_discs>1 && selected==ITEM_CONT) {
				menu.disc -= 1;
				if (menu.disc<0) menu.disc += menu.total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", menu.disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot -= 1;
				if (menu.slot<0) menu.slot += MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		else if (PAD_justPressed(BTN_RIGHT)) {
			if (menu.total_discs>1 && selected==ITEM_CONT) {
				menu.disc += 1;
				if (menu.disc==menu.total_discs) menu.disc -= menu.total_discs;
				dirty = 1;
				sprintf(disc_name, "Disc %i", menu.disc+1);
			}
			else if (selected==ITEM_SAVE || selected==ITEM_LOAD) {
				menu.slot += 1;
				if (menu.slot>=MENU_SLOT_COUNT) menu.slot -= MENU_SLOT_COUNT;
				dirty = 1;
			}
		}
		
		if (dirty && (selected==ITEM_SAVE || selected==ITEM_LOAD)) {
			Menu_updateState();
		}
		
		if (PAD_justPressed(BTN_B) || (BTN_WAKE!=BTN_MENU && PAD_tappedMenu(now))) {
			status = STATUS_CONT;
			show_menu = 0;
		}
		else if (PAD_justPressed(BTN_A)) {
			switch(selected) {
				case ITEM_CONT:
				if (menu.total_discs && rom_disc!=menu.disc) {
						status = STATUS_DISC;
						char* disc_path = menu.disc_paths[menu.disc];
						Game_changeDisc(disc_path);
					}
					else {
						status = STATUS_CONT;
					}
					show_menu = 0;
				break;
				
				case ITEM_SAVE: {
					Menu_saveState();
					status = STATUS_SAVE;
					show_menu = 0;
				}
				break;
				case ITEM_LOAD: {
					Menu_loadState();
					status = STATUS_LOAD;
					show_menu = 0;
				}
				break;
				case ITEM_OPTS: {
					if (simple_mode) {
						core.reset();
						status = STATUS_RESET;
						show_menu = 0;
					}
					else {
						int old_scaling = screen_scaling;
						Menu_options(&options_menu);
						if (screen_scaling!=old_scaling) {
							selectScaler(renderer.true_w,renderer.true_h,renderer.src_p);
						
							restore_w = screen->w;
							restore_h = screen->h;
							restore_p = screen->pitch;
							screen = GFX_resize(DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
							SDL_Rect dst = {0, 0, DEVICE_WIDTH, DEVICE_HEIGHT};
							SDL_BlitScaled(menu.bitmap,NULL,backing,&dst);
						}
						dirty = 1;
					}
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

		PWR_update(&dirty, &show_setting, Menu_beforeSleep, Menu_afterSleep);
		if(dirty) {
			GFX_clear(screen);

			GFX_drawOnLayer(menu.bitmap,0,0,DEVICE_WIDTH,DEVICE_HEIGHT,0.4f,1,0);
			

			int ox, oy;
			int ow = GFX_blitHardwareGroup(screen, show_setting);
			int max_width = screen->w - SCALE1(PADDING * 2) - ow;
			
			char display_name[256];
			int text_width = GFX_truncateText(font.large, rom_name, display_name, max_width, SCALE1(BUTTON_PADDING*2));
			max_width = MIN(max_width, text_width);

			SDL_Surface* text;
			text = TTF_RenderUTF8_Blended(font.large, display_name, uintToColour(THEME_COLOR6_255));
			GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
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
			
			if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting);
			else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"MENU","SLEEP", NULL }, 0, screen, 0);
			GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OKAY", NULL }, 1, screen, 1);
			
			// list
			oy = (((DEVICE_HEIGHT / FIXED_SCALE) - PADDING * 2) - (MENU_ITEM_COUNT * PILL_SIZE)) / 2;
			for (int i=0; i<MENU_ITEM_COUNT; i++) {
				char* item = menu.items[i];
				SDL_Color text_color = COLOR_WHITE;
				
				if (i==selected) {
					text_color = uintToColour(THEME_COLOR5_255);

					// disc change
					if (menu.total_discs>1 && i==ITEM_CONT) {				
						GFX_blitPillDark(ASSET_WHITE_PILL, screen, &(SDL_Rect){
							SCALE1(PADDING),
							SCALE1(oy + PADDING),
							screen->w - SCALE1(PADDING * 2),
							SCALE1(PILL_SIZE)
						});
						text = TTF_RenderUTF8_Blended(font.large, disc_name, text_color);
						SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){
							screen->w - SCALE1(PADDING + BUTTON_PADDING) - text->w,
							SCALE1(oy + PADDING + 4)
						});
						SDL_FreeSurface(text);
					}
					
					TTF_SizeUTF8(font.large, item, &ow, NULL);
					ow += SCALE1(BUTTON_PADDING*2);
					
					// pill
					GFX_blitPillDark(ASSET_WHITE_PILL, screen, &(SDL_Rect){
						SCALE1(PADDING),
						SCALE1(oy + PADDING + (i * PILL_SIZE)),
						ow,
						SCALE1(PILL_SIZE)
					});
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
				#define PAGINATION_HEIGHT 6
				// unscaled
				int hw = DEVICE_WIDTH / 2;
				int hh = DEVICE_HEIGHT / 2;
				int pw = hw + SCALE1(WINDOW_RADIUS*2);
				int ph = hh + SCALE1(WINDOW_RADIUS*2 + PAGINATION_HEIGHT + WINDOW_RADIUS);
				ox = DEVICE_WIDTH - pw - SCALE1(PADDING);
				oy = (DEVICE_HEIGHT - ph) / 2;
				
				// window
				GFX_blitRect(ASSET_STATE_BG, screen, &(SDL_Rect){ox,oy,pw,ph});
				ox += SCALE1(WINDOW_RADIUS);
				oy += SCALE1(WINDOW_RADIUS);
				
				if (menu.preview_exists) { // has save, has preview
					// lotta memory churn here
					SDL_Surface* bmp = IMG_Load(menu.bmp_path);
					SDL_Surface* raw_preview = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA8888,0);
					if (raw_preview) {
						SDL_FreeSurface(bmp); 
						bmp = raw_preview; 
					}
					// LOG_info("raw_preview %ix%i\n", raw_preview->w,raw_preview->h);
					SDL_Rect preview_rect = {ox,oy,hw,hh};
					SDL_FillRect(screen, &preview_rect, SDL_MapRGBA(screen->format,0,0,0,255));
					SDL_BlitScaled(bmp,NULL,preview,NULL);
					SDL_BlitSurface(preview, NULL, screen, &(SDL_Rect){ox,oy});
					SDL_FreeSurface(bmp);
				}
				else {
					SDL_Rect preview_rect = {ox,oy,hw,hh};
					SDL_FillRect(screen, &preview_rect, SDL_MapRGBA(screen->format,0,0,0,255));
					if (menu.save_exists) GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					else GFX_blitMessage(font.large, "Empty Slot", screen, &preview_rect);
				}
				
				// pagination
				ox += (pw-SCALE1(15*MENU_SLOT_COUNT))/2;
				oy += hh+SCALE1(WINDOW_RADIUS);
				for (int i=0; i<MENU_SLOT_COUNT; i++) {
					if (i==menu.slot)GFX_blitAsset(ASSET_PAGE, NULL, screen, &(SDL_Rect){ox+SCALE1(i*15),oy});
					else GFX_blitAsset(ASSET_DOT, NULL, screen, &(SDL_Rect){ox+SCALE1(i*15)+4,oy+SCALE1(2)});
				}
			}
			GFX_flip(screen);
			dirty=0;
		} else {
			// please dont flip cause it will cause current_fps dip and audio is weird first seconds
			GFX_delay();
		}
		hdmimon();
	}
	
	SDL_FreeSurface(preview);
	if(menu.bitmap) SDL_FreeSurface(menu.bitmap);
	PAD_reset();

	GFX_clearAll();
	PWR_warn(1);
	
	int count = 0;
	char** overlayList = config.frontend.options[FE_OPT_OVERLAY].values;
	while ( overlayList && overlayList[count]) count++;
	if (overlay >= 0 && overlay < count) {
		GFX_setOverlay(overlayList[overlay],core.tag);
	}

	GFX_setOffsetX(screenx);
	GFX_setOffsetY(screeny);
	if (!quit) {
		if (restore_w!=DEVICE_WIDTH || restore_h!=DEVICE_HEIGHT) {
			screen = GFX_resize(restore_w,restore_h,restore_p);
		}
		GFX_setEffect(screen_effect);

		GFX_clear(screen);

		setOverclock(overclock); // restore overclock value
		if (rumble_strength) VIB_setStrength(rumble_strength);
		
		if (!HAS_POWER_BUTTON) PWR_disableSleep();

		char act[PATH_MAX];
		sprintf(act, "gametimectl.elf start '%s' &", replaceString2(game.path, "'", "'\\''"));
		system(act);
	}
	else if (exists(NOUI_PATH)) PWR_powerOff(); // TODO: won't work with threaded core, only check this once per launch
	

	SDL_FreeSurface(backing);
	PWR_disableAutosleep();
}

// TODO: move to PWR_*?
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

static void resetFPSCounter() {
	sec_start = SDL_GetTicks();
	fps_ticks = 0.0;
	fps_double = 0.0;
}

static void chooseSyncRef(void) {
	switch (sync_ref) {
		case SYNC_SRC_AUTO:   use_core_fps = (core.get_region() == RETRO_REGION_PAL); break;
		case SYNC_SRC_SCREEN: use_core_fps = 0; break;
		case SYNC_SRC_CORE:   use_core_fps = 1; break;
	}
	LOG_info("%s: sync_ref is set to %s, game region is %s, use core fps = %s\n",
		  __FUNCTION__,
		  sync_ref_labels[sync_ref],
		  core.get_region() == RETRO_REGION_NTSC ? "NTSC" : "PAL",
		  use_core_fps ? "yes" : "no");
}

static void trackFPS(void) {
	cpu_ticks += 1;
	static int last_use_ticks = 0;
	uint32_t now = SDL_GetTicks();
	if (now - sec_start>=1000) {
		double last_time = (double)(now - sec_start) / 1000;
		fps_double = fps_ticks / last_time;
		cpu_double = cpu_ticks / last_time;
		// use_ticks = getUsage();
		// if (use_ticks && last_use_ticks) {
		// 	use_double = (use_ticks - last_use_ticks) / last_time;
		// }
		// last_use_ticks = use_ticks;
		sec_start = now;
		cpu_ticks = 0;
		fps_ticks = 0;
		
		// LOG_info("fps: %f cpu: %f\n", fps_double, cpu_double);
	}
}

static void limitFF(void) {
	static uint64_t ff_frame_time = 0;
	static uint64_t last_time = 0;
	static int last_max_speed = -1;
	if (last_max_speed!=max_ff_speed) {
		last_max_speed = max_ff_speed;
		ff_frame_time = 1000000 / (core.fps * (max_ff_speed + 1));
	}
	
	uint64_t now = getMicroseconds();
	if (fast_forward && max_ff_speed) {
		if (last_time == 0) last_time = now;
		int elapsed = now - last_time;
		if (elapsed>0 && elapsed<0x80000) {
			if (elapsed<ff_frame_time) {
				int delay = (ff_frame_time - elapsed) / 1000;
				if (delay>0 && delay<17) { // don't allow a delay any greater than a frame
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
	pthread_t cpucheckthread;
    pthread_create(&cpucheckthread, NULL, PLAT_cpu_monitor, NULL);

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
	
	LOG_info("rom_path: %s\n", rom_path);
	

	
	screen = GFX_init(MODE_MENU);

	// initialize default shaders
	GFX_initShaders();

	PAD_init();
	DEVICE_WIDTH = screen->w;
	DEVICE_HEIGHT = screen->h;
	DEVICE_PITCH = screen->pitch;
	// LOG_info("DEVICE_SIZE: %ix%i (%i)\n", DEVICE_WIDTH,DEVICE_HEIGHT,DEVICE_PITCH);
	
	VIB_init();
	PWR_init();
	if (!HAS_POWER_BUTTON) PWR_disableSleep();
	MSG_init();
	IMG_Init(IMG_INIT_PNG);
	Core_open(core_path, tag_name);

	fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	environment_callback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

	Game_open(rom_path); // nes tries to load gamegenie setting before this returns ffs
	if (!game.is_open) goto finish;
	
	simple_mode = exists(SIMPLE_MODE_PATH);
	
	// restore options
	Config_load(); // before init?
	Config_init();
	Config_readOptions(); // cores with boot logo option (eg. gb) need to load options early
	setOverclock(overclock);

	Core_init();

	// TODO: find a better place to do this
	// mixing static and loaded data is messy
	// why not move to Core_init()?
	// ah, because it's defined before options_menu...
	options_menu.items[1].desc = (char*)core.version;
	Core_load();
	Input_init(NULL);
	Config_readOptions(); // but others load and report options later (eg. nes)
	Config_readControls(); // restore controls (after the core has reported its defaults)
	
	SND_init(core.sample_rate, core.fps);
	InitSettings(); // after we initialize audio
	Menu_init();
	State_resume();
	Menu_initState(); // make ready for state shortcuts

	PWR_warn(1);
	PWR_disableAutosleep();

	// force a vsync immediately before loop
	// for better frame pacing?
	GFX_clearAll();
	GFX_clearLayers(0);
	GFX_clear(screen);

	// need to draw real black background first otherwise u get weird pixels sometimes

	GFX_flip(screen);

	Special_init(); // after config

	sec_start = SDL_GetTicks();
	resetFPSCounter();
	chooseSyncRef();
	
	int has_pending_opt_change = 0;
	LOG_info("Starting shaders %ims\n\n",SDL_GetTicks());


	// then initialize custom  shaders from settings
	initShaders();

	// release config when all is loaded
	Config_free();
	LOG_info("total startup time %ims\n\n",SDL_GetTicks());
	while (!quit) {
		GFX_startFrame();
	
		core.run();
		limitFF();
		trackFPS();
		

		if (has_pending_opt_change) {
			has_pending_opt_change = 0;
			if (Core_updateAVInfo()) {
				LOG_info("AV info changed, reset sound system");
				SND_resetAudio(core.sample_rate, core.fps);
			}
			resetFPSCounter();
			chooseSyncRef();
		}

		
		if (show_menu) {

			Menu_loop();
			has_pending_opt_change = config.core.changed;
			resetFPSCounter();
			chooseSyncRef();
			// reload shaders if needed (dont want to always do it as it blacks the screen)
			if(shadersreload) 
				initShaders();
			// this is not needed
			// SND_resetAudio(core.sample_rate, core.fps);
		}
	
		hdmimon();
	}
	int cw, ch;
	unsigned char* pixels = GFX_GL_screenCapture(&cw, &ch);
	
	renderer.dst = pixels;
	SDL_Surface* rawSurface = SDL_CreateRGBSurfaceWithFormatFrom(
		pixels, cw, ch, 32, cw * 4, SDL_PIXELFORMAT_ABGR8888
	);
	SDL_Surface* converted = SDL_ConvertSurfaceFormat(rawSurface, SDL_PIXELFORMAT_RGBA8888, 0);
	screen = converted;
	SDL_FreeSurface(rawSurface);
	free(pixels); 
	GFX_animateSurfaceOpacity(converted, 0, 0, cw, ch,
							  255, 0, CFG_getMenuTransitions() ? 200 : 20, 1);
	SDL_FreeSurface(converted); 
	
	if(rgbaData) free(rgbaData);

	Menu_quit();
	QuitSettings();
	
finish:

	Game_close();
	Core_unload();
	Core_quit();
	Core_close();
	Config_quit();
	Special_quit();
	MSG_quit();
	PWR_quit();
	VIB_quit();
	// already happens on Core_unload
	SND_quit();
	PAD_quit();
	GFX_quit();
	SDL_WaitThread(screenshotsavethread, NULL);
	return EXIT_SUCCESS;
}
