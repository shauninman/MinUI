/**
 * minui.c - MinUI launcher application
 *
 * The main launcher UI for MinUI, providing a simple file browser interface
 * for navigating ROMs, recently played games, collections, and tools.
 *
 * Architecture:
 * - File browser with directory stack navigation
 * - Recently played games tracking (up to 24 entries)
 * - ROM collections support via .txt files
 * - Multi-disc game support via .m3u playlists
 * - Display name aliasing via map.txt files
 * - Auto-resume support for returning to last played game
 * - Alphabetical indexing with L1/R1 shoulder button navigation
 *
 * Key Features:
 * - Platform-agnostic ROM paths (stored relative to SDCARD_PATH)
 * - Collating ROM folders (e.g., "GB (Game Boy)" and "GB (Game Boy Color)" appear as "GB")
 * - Thumbnail support from .res/ subdirectories
 * - Simple mode (hides Tools, disables sleep)
 * - HDMI hotplug detection and restart
 *
 * Data Structures:
 * - Array: Dynamic array for entries, directories, recents
 * - Hash: Simple key-value map for name aliasing
 * - Directory: Represents a folder with entries and rendering state
 * - Entry: Represents a file/folder (ROM, PAK, or directory)
 * - Recent: Recently played game with path and optional alias
 */

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "utils.h"

///////////////////////////////
// Dynamic array implementation
///////////////////////////////

/**
 * Generic dynamic array with automatic growth.
 *
 * Stores pointers to any type. Initial capacity is 8,
 * doubles when full. Used for directories, entries, and recents.
 */
typedef struct Array {
	int count;
	int capacity;
	void** items;
} Array;

/**
 * Creates a new empty array.
 *
 * @return Pointer to allocated Array
 *
 * @warning Caller must free with Array_free() or type-specific free function
 */
static Array* Array_new(void) {
	Array* self = malloc(sizeof(Array));
	self->count = 0;
	self->capacity = 8;
	self->items = malloc(sizeof(void*) * self->capacity);
	return self;
}

/**
 * Appends an item to the end of the array.
 *
 * Automatically doubles capacity when full.
 *
 * @param self Array to modify
 * @param item Pointer to add (not copied, caller retains ownership)
 */
static void Array_push(Array* self, void* item) {
	if (self->count >= self->capacity) {
		self->capacity *= 2;
		self->items = realloc(self->items, sizeof(void*) * self->capacity);
	}
	self->items[self->count++] = item;
}

/**
 * Inserts an item at the beginning of the array.
 *
 * Shifts all existing items to the right. Used to add most
 * recent game to top of recents list.
 *
 * @param self Array to modify
 * @param item Pointer to insert
 */
static void Array_unshift(Array* self, void* item) {
	if (self->count == 0)
		return Array_push(self, item);
	Array_push(self, NULL); // ensures we have enough capacity
	for (int i = self->count - 2; i >= 0; i--) {
		self->items[i + 1] = self->items[i];
	}
	self->items[0] = item;
}

/**
 * Removes and returns the last item from the array.
 *
 * @param self Array to modify
 * @return Pointer to removed item, or NULL if array is empty
 *
 * @note Caller assumes ownership of returned pointer
 */
static void* Array_pop(Array* self) {
	if (self->count == 0)
		return NULL;
	return self->items[--self->count];
}

/**
 * Reverses the order of all items in the array.
 *
 * @param self Array to modify
 */
static void Array_reverse(Array* self) {
	int end = self->count - 1;
	int mid = self->count / 2;
	for (int i = 0; i < mid; i++) {
		void* item = self->items[i];
		self->items[i] = self->items[end - i];
		self->items[end - i] = item;
	}
}

/**
 * Frees the array structure.
 *
 * @param self Array to free
 *
 * @warning Does NOT free the items themselves - use type-specific free functions
 */
static void Array_free(Array* self) {
	free(self->items);
	free(self);
}

/**
 * Finds the index of a string in a string array.
 *
 * @param self Array of string pointers
 * @param str String to find
 * @return Index of first matching string, or -1 if not found
 */
static int StringArray_indexOf(Array* self, char* str) {
	for (int i = 0; i < self->count; i++) {
		if (exactMatch(self->items[i], str))
			return i;
	}
	return -1;
}

/**
 * Frees a string array and all strings it contains.
 *
 * @param self Array to free
 */
static void StringArray_free(Array* self) {
	for (int i = 0; i < self->count; i++) {
		free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////
// Simple hash map (key-value store)
///////////////////////////////

/**
 * Simple key-value map using parallel arrays.
 *
 * Used for loading map.txt files that alias ROM display names.
 * Not a true hash - just linear search through keys array.
 */
typedef struct Hash {
	Array* keys;
	Array* values;
} Hash; // not really a hash

/**
 * Creates a new empty hash map.
 *
 * @return Pointer to allocated Hash
 *
 * @warning Caller must free with Hash_free()
 */
static Hash* Hash_new(void) {
	Hash* self = malloc(sizeof(Hash));
	self->keys = Array_new();
	self->values = Array_new();
	return self;
}

/**
 * Frees a hash map and all its keys and values.
 *
 * @param self Hash to free
 */
static void Hash_free(Hash* self) {
	StringArray_free(self->keys);
	StringArray_free(self->values);
	free(self);
}

/**
 * Stores a key-value pair in the hash map.
 *
 * Both key and value are duplicated with strdup().
 * Does not check for duplicate keys - allows multiple entries.
 *
 * @param self Hash to modify
 * @param key Key string
 * @param value Value string
 */
static void Hash_set(Hash* self, char* key, char* value) {
	Array_push(self->keys, strdup(key));
	Array_push(self->values, strdup(value));
}

/**
 * Retrieves a value by key from the hash map.
 *
 * @param self Hash to search
 * @param key Key to look up
 * @return Pointer to value string, or NULL if key not found
 *
 * @note Returned pointer is owned by the Hash - do not free
 */
static char* Hash_get(Hash* self, char* key) {
	int i = StringArray_indexOf(self->keys, key);
	if (i == -1)
		return NULL;
	return self->values->items[i];
}

///////////////////////////////
// File browser entries
///////////////////////////////

/**
 * Type of entry in the file browser.
 */
enum EntryType {
	ENTRY_DIR, // Directory (open to browse contents)
	ENTRY_PAK, // .pak folder (executable tool/app)
	ENTRY_ROM, // ROM file (launch with emulator)
};

/**
 * Represents a file or folder in the browser.
 *
 * Entries can be ROMs, directories, or .pak applications.
 * Display names are processed to remove region codes and extensions.
 */
typedef struct Entry {
	char* path; // Full path to file/folder
	char* name; // Cleaned display name (may be aliased via map.txt)
	char* unique; // Disambiguating text when multiple entries have same name
	int type; // ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
	int alpha; // Index into parent Directory's alphas array for L1/R1 navigation
} Entry;

/**
 * Creates a new entry from a path.
 *
 * Automatically processes the display name to remove extensions,
 * region codes, and other metadata.
 *
 * @param path Full path to the file/folder
 * @param type ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
 * @return Pointer to allocated Entry
 *
 * @warning Caller must free with Entry_free()
 */
static Entry* Entry_new(char* path, int type) {
	char display_name[256];
	getDisplayName(path, display_name);
	Entry* self = malloc(sizeof(Entry));
	self->path = strdup(path);
	self->name = strdup(display_name);
	self->unique = NULL;
	self->type = type;
	self->alpha = 0;
	return self;
}

/**
 * Frees an entry and all its strings.
 *
 * @param self Entry to free
 */
static void Entry_free(Entry* self) {
	free(self->path);
	free(self->name);
	if (self->unique)
		free(self->unique);
	free(self);
}

/**
 * Finds an entry by path in an entry array.
 *
 * @param self Array of Entry pointers
 * @param path Path to search for
 * @return Index of matching entry, or -1 if not found
 */
static int EntryArray_indexOf(Array* self, char* path) {
	for (int i = 0; i < self->count; i++) {
		Entry* entry = self->items[i];
		if (exactMatch(entry->path, path))
			return i;
	}
	return -1;
}

/**
 * Comparison function for qsort - sorts entries alphabetically by name.
 *
 * @param a First entry pointer (Entry**)
 * @param b Second entry pointer (Entry**)
 * @return Negative if a < b, 0 if equal, positive if a > b
 */
static int EntryArray_sortEntry(const void* a, const void* b) {
	Entry* item1 = *(Entry**)a;
	Entry* item2 = *(Entry**)b;
	return strcasecmp(item1->name, item2->name);
}

/**
 * Sorts an entry array alphabetically by display name.
 *
 * @param self Array to sort (modified in place)
 */
static void EntryArray_sort(Array* self) {
	qsort(self->items, self->count, sizeof(void*), EntryArray_sortEntry);
}

/**
 * Frees an entry array and all entries it contains.
 *
 * @param self Array to free
 */
static void EntryArray_free(Array* self) {
	for (int i = 0; i < self->count; i++) {
		Entry_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////
// Fixed-size integer array
///////////////////////////////

/**
 * Fixed-size array of integers for alphabetical indexing.
 *
 * Stores up to 27 indices (one for # and one for each letter A-Z).
 * Each value is the index of the first entry starting with that letter.
 */
#define INT_ARRAY_MAX 27
typedef struct IntArray {
	int count;
	int items[INT_ARRAY_MAX];
} IntArray;
/**
 * Creates a new empty integer array.
 *
 * @return Pointer to allocated IntArray
 *
 * @warning Caller must free with IntArray_free()
 */
static IntArray* IntArray_new(void) {
	IntArray* self = malloc(sizeof(IntArray));
	self->count = 0;
	memset(self->items, 0, sizeof(int) * INT_ARRAY_MAX);
	return self;
}

/**
 * Appends an integer to the array.
 *
 * @param self Array to modify
 * @param i Value to append
 *
 * @warning Does not check capacity - caller must ensure count < INT_ARRAY_MAX
 */
static void IntArray_push(IntArray* self, int i) {
	self->items[self->count++] = i;
}

/**
 * Frees an integer array.
 *
 * @param self Array to free
 */
static void IntArray_free(IntArray* self) {
	free(self);
}

///////////////////////////////
// Directory structure and indexing
///////////////////////////////

/**
 * Represents a directory in the file browser.
 *
 * Maintains list of entries, alphabetical index, and rendering state
 * (selected item, visible window start/end).
 */
typedef struct Directory {
	char* path; // Full path to directory
	char* name; // Display name
	Array* entries; // Array of Entry pointers
	IntArray* alphas; // Alphabetical index for L1/R1 navigation
	// Rendering state
	int selected; // Currently selected entry index
	int start; // First visible entry index
	int end; // One past last visible entry index
} Directory;

/**
 * Gets the alphabetical index for a string.
 *
 * Used to group entries by first letter for L1/R1 shoulder button navigation.
 *
 * @param str String to index
 * @return 0 for non-alphabetic, 1-26 for A-Z (case-insensitive)
 */
static int getIndexChar(char* str) {
	char i = 0;
	char c = tolower(str[0]);
	if (c >= 'a' && c <= 'z')
		i = (c - 'a') + 1;
	return i;
}

/**
 * Generates a unique name for an entry when duplicates exist.
 *
 * Appends the emulator name in parentheses to disambiguate entries
 * with identical display names but from different systems.
 *
 * Example: "Tetris" becomes "Tetris (GB)" or "Tetris (NES)"
 *
 * @param entry Entry to generate unique name for
 * @param out_name Output buffer for unique name (min 256 bytes)
 */
static void getUniqueName(Entry* entry, char* out_name) {
	char* filename = strrchr(entry->path, '/') + 1;
	char emu_tag[256];
	getEmuName(entry->path, emu_tag);

	char* tmp;
	strcpy(out_name, entry->name);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, " (");
	tmp = out_name + strlen(out_name);
	strcpy(tmp, emu_tag);
	tmp = out_name + strlen(out_name);
	strcpy(tmp, ")");
}

/**
 * Indexes a directory's entries and applies name aliasing.
 *
 * This function performs several important tasks:
 * 1. Loads map.txt (if present) to alias display names
 * 2. Filters out entries marked as hidden via map.txt
 * 3. Re-sorts entries if any names were aliased
 * 4. Detects duplicate display names and generates unique names
 * 5. Builds alphabetical index for L1/R1 navigation
 *
 * Map.txt format: Each line is "filename<TAB>display name"
 * - If display name starts with '.', the entry is hidden
 * - Collections use a shared map.txt in COLLECTIONS_PATH
 *
 * Duplicate handling:
 * - If two entries have the same display name but different filenames,
 *   shows the filename to disambiguate
 * - If filenames are also identical (cross-platform ROMs), appends
 *   the emulator name in parentheses
 *
 * @param self Directory to index (modified in place)
 */
static void Directory_index(Directory* self) {
	int is_collection = prefixMatch(COLLECTIONS_PATH, self->path);
	int skip_index = exactMatch(FAUX_RECENT_PATH, self->path) || is_collection; // not alphabetized

	// Load map.txt for name aliasing if present
	Hash* map = NULL;
	char map_path[256];
	sprintf(map_path, "%s/map.txt", is_collection ? COLLECTIONS_PATH : self->path);
	if (exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			map = Hash_new();
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue; // skip empty lines

				// Parse "filename\tdisplay name" format
				char* tmp = strchr(line, '\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp + 1;
					Hash_set(map, key, value);
				}
			}
			fclose(file);

			// Apply aliases from map
			int resort = 0;
			int filter = 0;
			for (int i = 0; i < self->entries->count; i++) {
				Entry* entry = self->entries->items[i];
				char* filename = strrchr(entry->path, '/') + 1;
				char* alias = Hash_get(map, filename);
				if (alias) {
					free(entry->name);
					entry->name = strdup(alias);
					resort = 1;
					// Check if any alias starts with '.' (hidden)
					if (!filter && hide(entry->name))
						filter = 1;
				}
			}

			// Remove hidden entries (those with aliases starting with '.')
			if (filter) {
				Array* entries = Array_new();
				for (int i = 0; i < self->entries->count; i++) {
					Entry* entry = self->entries->items[i];
					if (hide(entry->name)) {
						Entry_free(entry);
					} else {
						Array_push(entries, entry);
					}
				}
				// Not EntryArray_free - entries were moved, not freed
				Array_free(self->entries);
				self->entries = entries;
			}
			if (resort)
				EntryArray_sort(self->entries);
		}
	}

	// Detect duplicates and build alphabetical index
	Entry* prior = NULL;
	int alpha = -1;
	int index = 0;
	for (int i = 0; i < self->entries->count; i++) {
		Entry* entry = self->entries->items[i];
		if (map) {
			char* filename = strrchr(entry->path, '/') + 1;
			char* alias = Hash_get(map, filename);
			if (alias) {
				free(entry->name);
				entry->name = strdup(alias);
			}
		}

		// Detect duplicate display names
		if (prior != NULL && exactMatch(prior->name, entry->name)) {
			if (prior->unique)
				free(prior->unique);
			if (entry->unique)
				free(entry->unique);

			char* prior_filename = strrchr(prior->path, '/') + 1;
			char* entry_filename = strrchr(entry->path, '/') + 1;

			// If filenames differ, use them to disambiguate
			if (exactMatch(prior_filename, entry_filename)) {
				// Same filename (cross-platform ROM) - use emulator name
				char prior_unique[256];
				char entry_unique[256];
				getUniqueName(prior, prior_unique);
				getUniqueName(entry, entry_unique);

				prior->unique = strdup(prior_unique);
				entry->unique = strdup(entry_unique);
			} else {
				// Different filenames - show them
				prior->unique = strdup(prior_filename);
				entry->unique = strdup(entry_filename);
			}
		}

		// Build alphabetical index for L1/R1 navigation
		if (!skip_index) {
			int a = getIndexChar(entry->name);
			if (a != alpha) {
				index = self->alphas->count;
				IntArray_push(self->alphas, i);
				alpha = a;
			}
			entry->alpha = index;
		}

		prior = entry;
	}

	if (map)
		Hash_free(map);
}

// Forward declarations for directory entry getters
static Array* getRoot(void);
static Array* getRecents(void);
static Array* getCollection(char* path);
static Array* getDiscs(char* path);
static Array* getEntries(char* path);

/**
 * Creates a new directory from a path.
 *
 * Automatically determines which type of directory this is and
 * populates its entries accordingly:
 * - Root (SDCARD_PATH): Shows systems, recents, collections, tools
 * - Recently played (FAUX_RECENT_PATH): Shows recent games
 * - Collection (.txt file): Loads games from text file
 * - Multi-disc (.m3u file): Shows disc list
 * - Regular directory: Shows files and subdirectories
 *
 * @param path Full path to directory
 * @param selected Initial selected index
 * @return Pointer to allocated Directory
 *
 * @warning Caller must free with Directory_free()
 */
static Directory* Directory_new(char* path, int selected) {
	char display_name[256];
	getDisplayName(path, display_name);

	Directory* self = malloc(sizeof(Directory));
	self->path = strdup(path);
	self->name = strdup(display_name);
	if (exactMatch(path, SDCARD_PATH)) {
		self->entries = getRoot();
	} else if (exactMatch(path, FAUX_RECENT_PATH)) {
		self->entries = getRecents();
	} else if (!exactMatch(path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, path) &&
	           suffixMatch(".txt", path)) {
		self->entries = getCollection(path);
	} else if (suffixMatch(".m3u", path)) {
		self->entries = getDiscs(path);
	} else {
		self->entries = getEntries(path);
	}
	self->alphas = IntArray_new();
	self->selected = selected;
	Directory_index(self);
	return self;
}

/**
 * Frees a directory and all its contents.
 *
 * @param self Directory to free
 */
static void Directory_free(Directory* self) {
	free(self->path);
	free(self->name);
	EntryArray_free(self->entries);
	IntArray_free(self->alphas);
	free(self);
}

/**
 * Pops and frees the top directory from a directory array.
 *
 * @param self Array of Directory pointers
 */
static void DirectoryArray_pop(Array* self) {
	Directory_free(Array_pop(self));
}

/**
 * Frees a directory array and all directories it contains.
 *
 * @param self Array to free
 */
static void DirectoryArray_free(Array* self) {
	for (int i = 0; i < self->count; i++) {
		Directory_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////
// Recently played games
///////////////////////////////

/**
 * Represents a recently played game.
 *
 * Paths are stored relative to SDCARD_PATH for platform portability.
 * This allows the same SD card to work across different devices.
 */
typedef struct Recent {
	char* path; // Path relative to SDCARD_PATH (without prefix)
	char* alias; // Optional custom display name
	int available; // 1 if emulator exists, 0 if not
} Recent;

// Global used to pass alias when opening ROM from recents/collections
// This is a workaround to avoid changing function signatures
static char* recent_alias = NULL;

static int hasEmu(char* emu_name);

/**
 * Creates a new recent entry.
 *
 * @param path ROM path relative to SDCARD_PATH (without prefix)
 * @param alias Optional custom display name, or NULL
 * @return Pointer to allocated Recent
 *
 * @warning Caller must free with Recent_free()
 */
static Recent* Recent_new(char* path, char* alias) {
	Recent* self = malloc(sizeof(Recent));

	// Need full path to determine emulator
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	self->path = strdup(path);
	self->alias = alias ? strdup(alias) : NULL;
	self->available = hasEmu(emu_name);
	return self;
}

/**
 * Frees a recent entry.
 *
 * @param self Recent to free
 */
static void Recent_free(Recent* self) {
	free(self->path);
	if (self->alias)
		free(self->alias);
	free(self);
}

/**
 * Finds a recent by path in a recent array.
 *
 * @param self Array of Recent pointers
 * @param str Path to search for (relative to SDCARD_PATH)
 * @return Index of matching recent, or -1 if not found
 */
static int RecentArray_indexOf(Array* self, char* str) {
	for (int i = 0; i < self->count; i++) {
		Recent* item = self->items[i];
		if (exactMatch(item->path, str))
			return i;
	}
	return -1;
}

/**
 * Frees a recent array and all recents it contains.
 *
 * @param self Array to free
 */
static void RecentArray_free(Array* self) {
	for (int i = 0; i < self->count; i++) {
		Recent_free(self->items[i]);
	}
	Array_free(self);
}

///////////////////////////////
// Global state
///////////////////////////////

static Directory* top; // Current directory being viewed
static Array* stack; // Stack of open directories for navigation
static Array* recents; // Recently played games list

static int quit = 0; // Set to 1 to exit main loop
static int can_resume = 0; // 1 if selected ROM has a save state
static int should_resume = 0; // Set to 1 when X button pressed to resume
static int simple_mode = 0; // 1 if simple mode enabled (hides Tools, disables sleep)
static char slot_path[256]; // Path to save state slot file for can_resume check

// State restoration variables for preserving selection when navigating
static int restore_depth = -1;
static int restore_relative = -1;
static int restore_selected = 0;
static int restore_start = 0;
static int restore_end = 0;

///////////////////////////////
// Recents management
///////////////////////////////

#define MAX_RECENTS 24 // A multiple of all menu row counts (4, 6, 8, 12)

/**
 * Saves the recently played list to disk.
 *
 * Format: One entry per line, "path\talias\n" or just "path\n"
 * Paths are relative to SDCARD_PATH for platform portability.
 */
static void saveRecents(void) {
	FILE* file = fopen(RECENT_PATH, "w");
	if (file) {
		for (int i = 0; i < recents->count; i++) {
			Recent* recent = recents->items[i];
			fputs(recent->path, file);
			if (recent->alias) {
				fputs("\t", file);
				fputs(recent->alias, file);
			}
			putc('\n', file);
		}
		fclose(file);
	}
}

/**
 * Adds a ROM to the recently played list.
 *
 * If the ROM is already in the list, it's moved to the top.
 * If the list is full, the oldest entry is removed.
 *
 * @param path Full ROM path (will be made relative to SDCARD_PATH)
 * @param alias Optional custom display name, or NULL
 */
static void addRecent(char* path, char* alias) {
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	int id = RecentArray_indexOf(recents, path);
	if (id == -1) { // add new entry
		while (recents->count >= MAX_RECENTS) {
			Recent_free(Array_pop(recents));
		}
		Array_unshift(recents, Recent_new(path, alias));
	} else if (id > 0) { // bump existing entry to top
		for (int i = id; i > 0; i--) {
			void* tmp = recents->items[i - 1];
			recents->items[i - 1] = recents->items[i];
			recents->items[i] = tmp;
		}
	}
	// If id == 0, already at top, no action needed
	saveRecents();
}

///////////////////////////////
// ROM/emulator detection
///////////////////////////////

/**
 * Checks if an emulator is installed.
 *
 * Searches in two locations:
 * 1. Shared: /mnt/SDCARD/Roms/Emus/<emu>.pak/launch.sh
 * 2. Platform-specific: /mnt/SDCARD/Emus/<platform>/<emu>.pak/launch.sh
 *
 * @param emu_name Emulator name (e.g., "GB", "NES")
 * @return 1 if emulator exists, 0 otherwise
 */
static int hasEmu(char* emu_name) {
	char pak_path[256];
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
	if (exists(pak_path))
		return 1;

	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	return exists(pak_path);
}

/**
 * Checks if a directory contains a .cue file for multi-disc games.
 *
 * The .cue file must be named after the directory itself.
 * Example: /Roms/PS1/Final Fantasy VII/Final Fantasy VII.cue
 *
 * @param dir_path Full path to directory
 * @param cue_path Output buffer for .cue file path (modified in place)
 * @return 1 if .cue file exists, 0 otherwise
 *
 * @note cue_path is always written, even if file doesn't exist
 */
static int hasCue(char* dir_path, char* cue_path) {
	char* tmp = strrchr(dir_path, '/') + 1; // folder name
	sprintf(cue_path, "%s/%s.cue", dir_path, tmp);
	return exists(cue_path);
}

/**
 * Checks if a ROM has an associated .m3u playlist for multi-disc games.
 *
 * The .m3u file must be in the parent directory and named after that directory.
 * Example: For /Roms/PS1/Game/disc1.bin, looks for /Roms/PS1/Game.m3u
 *
 * @param rom_path Full path to ROM file
 * @param m3u_path Output buffer for .m3u file path (modified in place)
 * @return 1 if .m3u file exists, 0 otherwise
 *
 * @note m3u_path is always written, even if file doesn't exist
 */
static int hasM3u(char* rom_path, char* m3u_path) {
	char* tmp;

	strcpy(m3u_path, rom_path);
	tmp = strrchr(m3u_path, '/') + 1;
	tmp[0] = '\0';

	// path to parent directory
	char base_path[256];
	strcpy(base_path, m3u_path);

	tmp = strrchr(m3u_path, '/');
	tmp[0] = '\0';

	// get parent directory name
	char dir_name[256];
	tmp = strrchr(m3u_path, '/');
	strcpy(dir_name, tmp);

	// dir_name is also our m3u file name
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, dir_name);

	// add extension
	tmp = m3u_path + strlen(m3u_path);
	strcpy(tmp, ".m3u");

	return exists(m3u_path);
}

/**
 * Loads recently played games from disk.
 *
 * This function performs several important tasks:
 * 1. Handles disc change requests (from in-game disc swapping)
 * 2. Loads recent games from RECENT_PATH file
 * 3. Filters out games whose emulators no longer exist
 * 4. Deduplicates multi-disc games (shows only most recent disc)
 * 5. Populates the global recents array
 *
 * Multi-disc handling:
 * - If a game has an .m3u file, only the most recently played disc
 *   from that game is shown in recents
 * - This prevents the recents list from being flooded with discs
 *   from the same game
 *
 * @return 1 if any playable recents exist, 0 otherwise
 */
static int hasRecents(void) {
	LOG_info("hasRecents %s\n", RECENT_PATH);
	int has = 0;

	// Track parent directories to avoid duplicate multi-disc entries
	Array* parent_paths = Array_new();
	if (exists(CHANGE_DISC_PATH)) {
		char sd_path[256];
		getFile(CHANGE_DISC_PATH, sd_path, 256);
		if (exists(sd_path)) {
			char* disc_path = sd_path + strlen(SDCARD_PATH); // makes path platform agnostic
			Recent* recent = Recent_new(disc_path, NULL);
			if (recent->available)
				has += 1;
			Array_push(recents, recent);

			char parent_path[256];
			strcpy(parent_path, disc_path);
			char* tmp = strrchr(parent_path, '/') + 1;
			tmp[0] = '\0';
			Array_push(parent_paths, strdup(parent_path));
		}
		unlink(CHANGE_DISC_PATH);
	}

	FILE* file = fopen(RECENT_PATH, "r"); // newest at top
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			// LOG_info("line: %s\n", line);

			char* path = line;
			char* alias = NULL;
			char* tmp = strchr(line, '\t');
			if (tmp) {
				tmp[0] = '\0';
				alias = tmp + 1;
			}

			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, path);
			if (exists(sd_path)) {
				if (recents->count < MAX_RECENTS) {
					// this logic replaces an existing disc from a multi-disc game with the last used
					char m3u_path[256];
					if (hasM3u(sd_path, m3u_path)) { // TODO: this might tank launch speed
						char parent_path[256];
						strcpy(parent_path, path);
						char* tmp = strrchr(parent_path, '/') + 1;
						tmp[0] = '\0';

						int found = 0;
						for (int i = 0; i < parent_paths->count; i++) {
							char* path = parent_paths->items[i];
							if (prefixMatch(path, parent_path)) {
								found = 1;
								break;
							}
						}
						if (found)
							continue;

						Array_push(parent_paths, strdup(parent_path));
					}

					// LOG_info("path:%s alias:%s\n", path, alias);

					Recent* recent = Recent_new(path, alias);
					if (recent->available)
						has += 1;
					Array_push(recents, recent);
				}
			}
		}
		fclose(file);
	}

	saveRecents();

	StringArray_free(parent_paths);
	return has > 0;
}

/**
 * Checks if any ROM collections exist.
 *
 * @return 1 if collections directory contains any non-hidden files, 0 otherwise
 */
static int hasCollections(void) {
	int has = 0;
	if (!exists(COLLECTIONS_PATH))
		return has;

	DIR* dh = opendir(COLLECTIONS_PATH);
	struct dirent* dp;
	while ((dp = readdir(dh)) != NULL) {
		if (hide(dp->d_name))
			continue;
		has = 1;
		break;
	}
	closedir(dh);
	return has;
}

/**
 * Checks if a ROM system directory has any playable ROMs.
 *
 * A system is considered to have ROMs if:
 * 1. The emulator .pak exists
 * 2. The directory contains at least one non-hidden file
 *
 * @param dir_name Name of ROM directory (e.g., "GB (Game Boy)")
 * @return 1 if system has playable ROMs, 0 otherwise
 */
static int hasRoms(char* dir_name) {
	int has = 0;
	char emu_name[256];
	char rom_path[256];

	getEmuName(dir_name, emu_name);

	// check for emu pak
	if (!hasEmu(emu_name))
		return has;

	// check for at least one non-hidden file (assume it's a rom)
	sprintf(rom_path, "%s/%s/", ROMS_PATH, dir_name);
	DIR* dh = opendir(rom_path);
	if (dh != NULL) {
		struct dirent* dp;
		while ((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name))
				continue;
			has = 1;
			break;
		}
		closedir(dh);
	}
	return has;
}

///////////////////////////////
// Directory entry generation
///////////////////////////////

/**
 * Generates the root directory entry list.
 *
 * Root shows:
 * 1. Recently Played (if any recent games exist)
 * 2. ROM systems (folders in Roms/ with available emulators)
 *    - Deduplicates systems with the same display name (collating)
 *    - Applies aliases from Roms/map.txt
 * 3. Collections (if any exist)
 *    - Either as a "Collections" folder or promoted to root if no systems
 * 4. Tools (platform-specific, hidden in simple mode)
 *
 * @return Array of Entry pointers for root directory
 */
static Array* getRoot(void) {
	Array* root = Array_new();

	if (hasRecents())
		Array_push(root, Entry_new(FAUX_RECENT_PATH, ENTRY_DIR));

	Array* entries = Array_new();
	DIR* dh = opendir(ROMS_PATH);
	if (dh != NULL) {
		struct dirent* dp;
		char* tmp;
		char full_path[256];
		sprintf(full_path, "%s/", ROMS_PATH);
		tmp = full_path + strlen(full_path);
		Array* emus = Array_new();
		while ((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name))
				continue;
			if (hasRoms(dp->d_name)) {
				strcpy(tmp, dp->d_name);
				Array_push(emus, Entry_new(full_path, ENTRY_DIR));
			}
		}
		EntryArray_sort(emus);
		Entry* prev_entry = NULL;
		for (int i = 0; i < emus->count; i++) {
			Entry* entry = emus->items[i];
			if (prev_entry != NULL) {
				if (exactMatch(prev_entry->name, entry->name)) {
					Entry_free(entry);
					continue;
				}
			}
			Array_push(entries, entry);
			prev_entry = entry;
		}
		Array_free(emus); // just free the array part, entries now owns emus entries
		closedir(dh);
	}

	// copied/modded from Directory_index
	// we don't support hidden remaps here
	char map_path[256];
	sprintf(map_path, "%s/map.txt", ROMS_PATH);
	if (entries->count > 0 && exists(map_path)) {
		FILE* file = fopen(map_path, "r");
		if (file) {
			Hash* map = Hash_new();
			char line[256];
			while (fgets(line, 256, file) != NULL) {
				normalizeNewline(line);
				trimTrailingNewlines(line);
				if (strlen(line) == 0)
					continue; // skip empty lines

				char* tmp = strchr(line, '\t');
				if (tmp) {
					tmp[0] = '\0';
					char* key = line;
					char* value = tmp + 1;
					Hash_set(map, key, value);
				}
			}
			fclose(file);

			int resort = 0;
			for (int i = 0; i < entries->count; i++) {
				Entry* entry = entries->items[i];
				char* filename = strrchr(entry->path, '/') + 1;
				char* alias = Hash_get(map, filename);
				if (alias) {
					free(entry->name);
					entry->name = strdup(alias);
					resort = 1;
				}
			}
			if (resort)
				EntryArray_sort(entries);
			Hash_free(map);
		}
	}

	if (hasCollections()) {
		if (entries->count)
			Array_push(root, Entry_new(COLLECTIONS_PATH, ENTRY_DIR));
		else { // no visible systems, promote collections to root
			dh = opendir(COLLECTIONS_PATH);
			if (dh != NULL) {
				struct dirent* dp;
				char* tmp;
				char full_path[256];
				sprintf(full_path, "%s/", COLLECTIONS_PATH);
				tmp = full_path + strlen(full_path);
				Array* collections = Array_new();
				while ((dp = readdir(dh)) != NULL) {
					if (hide(dp->d_name))
						continue;
					strcpy(tmp, dp->d_name);
					Array_push(
					    collections,
					    Entry_new(full_path, ENTRY_DIR)); // yes, collections are fake directories
				}
				EntryArray_sort(collections);
				for (int i = 0; i < collections->count; i++) {
					Array_push(entries, collections->items[i]);
				}
				Array_free(
				    collections); // just free the array part, entries now owns collections entries
				closedir(dh);
			}
		}
	}

	// add systems to root
	for (int i = 0; i < entries->count; i++) {
		Array_push(root, entries->items[i]);
	}
	Array_free(entries); // root now owns entries' entries

	char* tools_path = SDCARD_PATH "/Tools/" PLATFORM;
	if (exists(tools_path) && !simple_mode)
		Array_push(root, Entry_new(tools_path, ENTRY_DIR));

	return root;
}

/**
 * Generates the Recently Played directory entry list.
 *
 * Filters out games whose emulators no longer exist.
 * Applies custom aliases if present.
 *
 * @return Array of Entry pointers for recently played games
 */
static Array* getRecents(void) {
	Array* entries = Array_new();
	for (int i = 0; i < recents->count; i++) {
		Recent* recent = recents->items[i];
		if (!recent->available)
			continue;

		char sd_path[256];
		sprintf(sd_path, "%s%s", SDCARD_PATH, recent->path);
		int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM;
		Entry* entry = Entry_new(sd_path, type);
		if (recent->alias) {
			free(entry->name);
			entry->name = strdup(recent->alias);
		}
		Array_push(entries, entry);
	}
	return entries;
}

/**
 * Generates entry list from a collection text file.
 *
 * Collection format: One ROM path per line (relative to SDCARD_PATH)
 * Example: /Roms/GB/Tetris.gb
 *
 * Only includes ROMs that currently exist on the SD card.
 *
 * @param path Full path to collection .txt file
 * @return Array of Entry pointers for collection items
 */
static Array* getCollection(char* path) {
	Array* entries = Array_new();
	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			char sd_path[256];
			sprintf(sd_path, "%s%s", SDCARD_PATH, line);
			if (exists(sd_path)) {
				int type = suffixMatch(".pak", sd_path) ? ENTRY_PAK : ENTRY_ROM;
				Array_push(entries, Entry_new(sd_path, type));
			}
		}
		fclose(file);
	}
	return entries;
}

/**
 * Generates disc list from an .m3u playlist file.
 *
 * M3U format: One disc file per line (relative to .m3u file location)
 * Example: disc1.bin
 *
 * Entries are named "Disc 1", "Disc 2", etc.
 *
 * @param path Full path to .m3u file
 * @return Array of Entry pointers for each disc
 */
static Array* getDiscs(char* path) {
	Array* entries = Array_new();

	char base_path[256];
	strcpy(base_path, path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';

	FILE* file = fopen(path, "r");
	if (file) {
		char line[256];
		int disc = 0;
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			char disc_path[256];
			sprintf(disc_path, "%s%s", base_path, line);

			if (exists(disc_path)) {
				disc += 1;
				Entry* entry = Entry_new(disc_path, ENTRY_ROM);
				free(entry->name);
				char name[16];
				sprintf(name, "Disc %i", disc);
				entry->name = strdup(name);
				Array_push(entries, entry);
			}
		}
		fclose(file);
	}
	return entries;
}

/**
 * Gets the first disc from an .m3u playlist.
 *
 * Used when auto-launching a multi-disc game.
 *
 * @param m3u_path Full path to .m3u file
 * @param disc_path Output buffer for first disc path
 * @return 1 if first disc found, 0 otherwise
 */
static int getFirstDisc(char* m3u_path, char* disc_path) {
	int found = 0;

	char base_path[256];
	strcpy(base_path, m3u_path);
	char* tmp = strrchr(base_path, '/') + 1;
	tmp[0] = '\0';

	FILE* file = fopen(m3u_path, "r");
	if (file) {
		char line[256];
		while (fgets(line, 256, file) != NULL) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (strlen(line) == 0)
				continue; // skip empty lines

			sprintf(disc_path, "%s%s", base_path, line);

			if (exists(disc_path))
				found = 1;
			break;
		}
		fclose(file);
	}
	return found;
}

static void addEntries(Array* entries, char* path) {
	DIR* dh = opendir(path);
	if (dh != NULL) {
		struct dirent* dp;
		char* tmp;
		char full_path[256];
		sprintf(full_path, "%s/", path);
		tmp = full_path + strlen(full_path);
		while ((dp = readdir(dh)) != NULL) {
			if (hide(dp->d_name))
				continue;
			strcpy(tmp, dp->d_name);
			int is_dir = dp->d_type == DT_DIR;
			int type;
			if (is_dir) {
				// TODO: this should make sure launch.sh exists
				if (suffixMatch(".pak", dp->d_name)) {
					type = ENTRY_PAK;
				} else {
					type = ENTRY_DIR;
				}
			} else {
				if (prefixMatch(COLLECTIONS_PATH, full_path)) {
					type = ENTRY_DIR; // :shrug:
				} else {
					type = ENTRY_ROM;
				}
			}
			Array_push(entries, Entry_new(full_path, type));
		}
		closedir(dh);
	}
}

static int isConsoleDir(char* path) {
	char* tmp;
	char parent_dir[256];
	strcpy(parent_dir, path);
	tmp = strrchr(parent_dir, '/');
	tmp[0] = '\0';

	return exactMatch(parent_dir, ROMS_PATH);
}

static Array* getEntries(char* path) {
	Array* entries = Array_new();

	if (isConsoleDir(path)) { // top-level console folder, might collate
		char collated_path[256];
		strcpy(collated_path, path);
		char* tmp = strrchr(collated_path, '(');
		// 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
		// but conditional so we can continue to support a bare tag name as a folder name
		if (tmp)
			tmp[1] = '\0';

		DIR* dh = opendir(ROMS_PATH);
		if (dh != NULL) {
			struct dirent* dp;
			char full_path[256];
			sprintf(full_path, "%s/", ROMS_PATH);
			tmp = full_path + strlen(full_path);
			// while loop so we can collate paths, see above
			while ((dp = readdir(dh)) != NULL) {
				if (hide(dp->d_name))
					continue;
				if (dp->d_type != DT_DIR)
					continue;
				strcpy(tmp, dp->d_name);

				if (!prefixMatch(collated_path, full_path))
					continue;
				addEntries(entries, full_path);
			}
			closedir(dh);
		}
	} else
		addEntries(entries, path); // just a subfolder

	EntryArray_sort(entries);
	return entries;
}

///////////////////////////////////////

///////////////////////////////
// Command execution
///////////////////////////////

/**
 * Queues a command to run after launcher exits.
 *
 * Writes the command to /tmp/next and sets quit flag.
 * The system's init script watches for this file and executes it.
 *
 * @param cmd Shell command to execute (must be properly quoted)
 */
static void queueNext(char* cmd) {
	LOG_info("cmd: %s\n", cmd);
	putFile("/tmp/next", cmd);
	quit = 1;
}

/**
 * Replaces all occurrences of a substring in a string.
 *
 * Modifies the string in place. Handles overlapping replacements
 * by recursing after each replacement.
 *
 * @param line String to modify (modified in place)
 * @param search Substring to find
 * @param replace Replacement string
 * @return Number of replacements made
 *
 * @note Based on https://stackoverflow.com/a/31775567/145965
 */
static int replaceString(char* line, const char* search, const char* replace) {
	char* sp; // start of pattern
	if ((sp = strstr(line, search)) == NULL) {
		return 0;
	}
	int count = 1;
	int sLen = strlen(search);
	int rLen = strlen(replace);
	if (sLen > rLen) {
		// move from right to left
		char* src = sp + sLen;
		char* dst = sp + rLen;
		while ((*dst = *src) != '\0') {
			dst++;
			src++;
		}
	} else if (sLen < rLen) {
		// move from left to right
		int tLen = strlen(sp) - sLen;
		char* stop = sp + rLen;
		char* src = sp + sLen + tLen;
		char* dst = sp + rLen + tLen;
		while (dst >= stop) {
			*dst = *src;
			dst--;
			src--;
		}
	}
	memcpy(sp, replace, rLen);
	count += replaceString(sp + rLen, search, replace);
	return count;
}

/**
 * Escapes single quotes in a string for shell command safety.
 *
 * Replaces ' with '\'' which safely handles quotes in bash strings.
 * Example: "it's" becomes "it'\''s"
 *
 * @param str String to escape (modified in place)
 * @return The modified string (same pointer as input)
 */
static char* escapeSingleQuotes(char* str) {
	replaceString(str, "'", "'\\''");
	return str;
}

///////////////////////////////
// Resume state checking
///////////////////////////////

/**
 * Checks if a ROM has a save state and prepares resume state.
 *
 * Sets global can_resume flag and slot_path if a state exists.
 * Handles multi-disc games by checking for .m3u files.
 *
 * Save state path format:
 * /.userdata/.minui/<emu>/<romname>.ext.txt
 *
 * @param rom_path Full ROM path
 * @param type ENTRY_DIR, ENTRY_PAK, or ENTRY_ROM
 */
static void readyResumePath(char* rom_path, int type) {
	char* tmp;
	can_resume = 0;
	char path[256];
	strcpy(path, rom_path);

	if (!prefixMatch(ROMS_PATH, path))
		return;

	char auto_path[256];
	if (type == ENTRY_DIR) {
		if (!hasCue(path, auto_path)) { // no cue?
			tmp = strrchr(auto_path, '.') + 1; // extension
			strcpy(tmp, "m3u"); // replace with m3u
			if (!exists(auto_path))
				return; // no m3u
		}
		strcpy(path, auto_path); // cue or m3u if one exists
	}

	if (!suffixMatch(".m3u", path)) {
		char m3u_path[256];
		if (hasM3u(path, m3u_path)) {
			// change path to m3u path
			strcpy(path, m3u_path);
		}
	}

	char emu_name[256];
	getEmuName(path, emu_name);

	char rom_file[256];
	tmp = strrchr(path, '/') + 1;
	strcpy(rom_file, tmp);

	sprintf(slot_path, "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name,
	        rom_file); // /.userdata/.minui/<EMU>/<romname>.ext.txt

	can_resume = exists(slot_path);
}
static void readyResume(Entry* entry) {
	readyResumePath(entry->path, entry->type);
}

static void saveLast(char* path);
static void loadLast(void);

static int autoResume(void) {
	// NOTE: bypasses recents

	if (!exists(AUTO_RESUME_PATH))
		return 0;

	char path[256];
	getFile(AUTO_RESUME_PATH, path, 256);
	unlink(AUTO_RESUME_PATH);
	sync();

	// make sure rom still exists
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);
	if (!exists(sd_path))
		return 0;

	// make sure emu still exists
	char emu_name[256];
	getEmuName(sd_path, emu_name);

	char emu_path[256];
	getEmuPath(emu_name, emu_path);

	if (!exists(emu_path))
		return 0;

	// putFile(LAST_PATH, FAUX_RECENT_PATH); // saveLast() will crash here because top is NULL

	char cmd[256];
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), escapeSingleQuotes(sd_path));
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	queueNext(cmd);
	return 1;
}

///////////////////////////////
// Entry opening (launching ROMs/apps)
///////////////////////////////

/**
 * Launches a .pak application.
 *
 * .pak folders are applications (tools, emulators) with a launch.sh script.
 * Saves to recents if in Roms path. Saves current path for state restoration.
 *
 * @param path Full path to .pak directory
 */
static void openPak(char* path) {
	// Save path before escaping (escapeSingleQuotes modifies string)
	if (prefixMatch(ROMS_PATH, path)) {
		addRecent(path, NULL);
	}
	saveLast(path);

	char cmd[256];
	sprintf(cmd, "'%s/launch.sh'", escapeSingleQuotes(path));
	queueNext(cmd);
}

/**
 * Launches a ROM with its emulator.
 *
 * This function handles:
 * - Multi-disc games (.m3u playlists)
 * - Resume states (saves/loads save state slot)
 * - Disc swapping for multi-disc games
 * - Adding to recently played list
 * - State restoration path tracking
 *
 * Multi-disc logic:
 * - If ROM has .m3u, add .m3u to recents (not individual disc)
 * - If launching .m3u directly, get first disc from playlist
 * - If resuming multi-disc game, load the disc that was saved
 *
 * @param path Full ROM path (may be .m3u or actual disc file)
 * @param last Path to save for state restoration (may differ from path)
 */
static void openRom(char* path, char* last) {
	LOG_info("openRom(%s,%s)\n", path, last);

	char sd_path[256];
	strcpy(sd_path, path);

	char m3u_path[256];
	int has_m3u = hasM3u(sd_path, m3u_path);

	char recent_path[256];
	strcpy(recent_path, has_m3u ? m3u_path : sd_path);

	if (has_m3u && suffixMatch(".m3u", sd_path)) {
		getFirstDisc(m3u_path, sd_path);
	}

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	if (should_resume) {
		char slot[16];
		getFile(slot_path, slot, 16);
		putFile(RESUME_SLOT_PATH, slot);
		should_resume = 0;

		if (has_m3u) {
			char rom_file[256];
			strcpy(rom_file, strrchr(m3u_path, '/') + 1);

			// get disc for state
			char disc_path_path[256];
			sprintf(disc_path_path, "%s/.minui/%s/%s.%s.txt", SHARED_USERDATA_PATH, emu_name,
			        rom_file, slot); // /.userdata/arm-480/.minui/<EMU>/<romname>.ext.0.txt

			if (exists(disc_path_path)) {
				// switch to disc path
				char disc_path[256];
				getFile(disc_path_path, disc_path, 256);
				if (disc_path[0] == '/')
					strcpy(sd_path, disc_path); // absolute
				else { // relative
					strcpy(sd_path, m3u_path);
					char* tmp = strrchr(sd_path, '/') + 1;
					strcpy(tmp, disc_path);
				}
			}
		}
	} else
		putInt(RESUME_SLOT_PATH, 8); // resume hidden default state

	char emu_path[256];
	getEmuPath(emu_name, emu_path);

	// NOTE: escapeSingleQuotes() modifies the passed string
	// so we need to save the path before we call that
	addRecent(recent_path, recent_alias); // yiiikes
	saveLast(last == NULL ? sd_path : last);

	char cmd[256];
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), escapeSingleQuotes(sd_path));
	queueNext(cmd);
}
/**
 * Opens a directory for browsing or auto-launches its contents.
 *
 * Auto-launch logic (when auto_launch=1):
 * - If directory contains a .cue file, launch it
 * - If directory contains a .m3u file, launch first disc
 * - Otherwise, open directory for browsing
 *
 * Used for:
 * - Multi-disc game folders (auto-launch .cue or .m3u)
 * - Regular folder navigation (auto_launch=0)
 * - State restoration (preserves selection/scroll position)
 *
 * @param path Full path to directory
 * @param auto_launch 1 to auto-launch contents, 0 to browse
 */
static void openDirectory(char* path, int auto_launch) {
	char auto_path[256];
	// Auto-launch .cue file if present
	if (hasCue(path, auto_path) && auto_launch) {
		openRom(auto_path, path);
		return;
	}

	// Auto-launch .m3u playlist if present
	char m3u_path[256];
	strcpy(m3u_path, auto_path);
	char* tmp = strrchr(m3u_path, '.') + 1; // extension
	strcpy(tmp, "m3u"); // replace with m3u
	if (exists(m3u_path) && auto_launch) {
		auto_path[0] = '\0';
		if (getFirstDisc(m3u_path, auto_path)) {
			openRom(auto_path, path);
			return;
		}
	}

	int selected = 0;
	int start = selected;
	int end = 0;
	if (top && top->entries->count > 0) {
		if (restore_depth == stack->count && top->selected == restore_relative) {
			selected = restore_selected;
			start = restore_start;
			end = restore_end;
		}
	}

	top = Directory_new(path, selected);
	top->start = start;
	top->end =
	    end ? end : ((top->entries->count < MAIN_ROW_COUNT) ? top->entries->count : MAIN_ROW_COUNT);

	Array_push(stack, top);
}
/**
 * Closes the current directory and returns to parent.
 *
 * Saves current scroll position and selection for potential restoration.
 * Updates global restore state and pops directory from stack.
 */
static void closeDirectory(void) {
	restore_selected = top->selected;
	restore_start = top->start;
	restore_end = top->end;
	DirectoryArray_pop(stack);
	restore_depth = stack->count;
	top = stack->items[stack->count - 1];
	restore_relative = top->selected;
}

/**
 * Opens an entry (ROM, directory, or application).
 *
 * Dispatches to appropriate handler based on entry type:
 * - ENTRY_ROM: Launch with emulator
 * - ENTRY_PAK: Launch application
 * - ENTRY_DIR: Open for browsing (with auto-launch)
 *
 * Special handling for collections: Uses collection path for
 * state restoration instead of actual ROM path.
 *
 * @param self Entry to open
 */
static void Entry_open(Entry* self) {
	recent_alias = self->name; // Passed to addRecent via global
	if (self->type == ENTRY_ROM) {
		char* last = NULL;
		// Collection ROMs use collection path for state restoration
		if (prefixMatch(COLLECTIONS_PATH, top->path)) {
			char* tmp;
			char filename[256];

			tmp = strrchr(self->path, '/');
			if (tmp)
				strcpy(filename, tmp + 1);

			char last_path[256];
			sprintf(last_path, "%s/%s", top->path, filename);
			last = last_path;
		}
		openRom(self->path, last);
	} else if (self->type == ENTRY_PAK) {
		openPak(self->path);
	} else if (self->type == ENTRY_DIR) {
		openDirectory(self->path, 1);
	}
}

///////////////////////////////
// State persistence (last played/position)
///////////////////////////////

/**
 * Saves the last accessed path for state restoration.
 *
 * Special case: Recently played path is implicit (always first item)
 * so we don't need to save the specific ROM, just that recents was open.
 *
 * @param path Path to save
 */
static void saveLast(char* path) {
	// special case for recently played
	if (exactMatch(top->path, FAUX_RECENT_PATH)) {
		// Most recent game is always at top, no need to save specific ROM
		path = FAUX_RECENT_PATH;
	}
	putFile(LAST_PATH, path);
}

/**
 * Loads and restores the last accessed path and selection.
 *
 * Rebuilds the directory stack from the saved path, restoring:
 * - Which directories were open
 * - Which item was selected
 * - Scroll position
 *
 * Handles special cases:
 * - Collated ROM folders (matches by prefix)
 * - Collection entries (matches by filename)
 * - Auto-launch directories (doesn't re-launch)
 *
 * Called after loading root directory during startup.
 */
static void loadLast(void) {
	if (!exists(LAST_PATH))
		return;

	char last_path[256];
	getFile(LAST_PATH, last_path, 256);

	char full_path[256];
	strcpy(full_path, last_path);

	char* tmp;
	char filename[256];
	tmp = strrchr(last_path, '/');
	if (tmp)
		strcpy(filename, tmp);

	Array* last = Array_new();
	while (!exactMatch(last_path, SDCARD_PATH)) {
		Array_push(last, strdup(last_path));

		char* slash = strrchr(last_path, '/');
		last_path[(slash - last_path)] = '\0';
	}

	while (last->count > 0) {
		char* path = Array_pop(last);
		if (!exactMatch(
		        path,
		        ROMS_PATH)) { // romsDir is effectively root as far as restoring state after a game
			char collated_path[256];
			collated_path[0] = '\0';
			if (suffixMatch(")", path) && isConsoleDir(path)) {
				strcpy(collated_path, path);
				tmp = strrchr(collated_path, '(');
				if (tmp)
					tmp[1] =
					    '\0'; // 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
			}

			for (int i = 0; i < top->entries->count; i++) {
				Entry* entry = top->entries->items[i];

				// NOTE: strlen() is required for collated_path, '\0' wasn't reading as NULL for some reason
				if (exactMatch(entry->path, path) ||
				    (strlen(collated_path) && prefixMatch(collated_path, entry->path)) ||
				    (prefixMatch(COLLECTIONS_PATH, full_path) &&
				     suffixMatch(filename, entry->path))) {
					top->selected = i;
					if (i >= top->end) {
						top->start = i;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end > top->entries->count) {
							top->end = top->entries->count;
							top->start = top->end - MAIN_ROW_COUNT;
						}
					}
					if (last->count == 0 && !exactMatch(entry->path, FAUX_RECENT_PATH) &&
					    !(!exactMatch(entry->path, COLLECTIONS_PATH) &&
					      prefixMatch(COLLECTIONS_PATH, entry->path)))
						break; // don't show contents of auto-launch dirs

					if (entry->type == ENTRY_DIR) {
						openDirectory(entry->path, 0);
						break;
					}
				}
			}
		}
		free(path); // we took ownership when we popped it
	}

	StringArray_free(last);
}

///////////////////////////////////////

///////////////////////////////
// Menu initialization and cleanup
///////////////////////////////

/**
 * Initializes the menu system.
 *
 * Sets up:
 * - Directory navigation stack
 * - Recently played games list
 * - Root directory
 * - Last accessed path restoration
 */
static void Menu_init(void) {
	stack = Array_new(); // array of open Directories
	recents = Array_new();

	openDirectory(SDCARD_PATH, 0);
	loadLast(); // restore state when available
}

/**
 * Cleans up menu system resources.
 *
 * Frees all allocated memory for recents and directory stack.
 */
static void Menu_quit(void) {
	RecentArray_free(recents);
	DirectoryArray_free(stack);
}

///////////////////////////////
// Main entry point
///////////////////////////////

/**
 * MinUI launcher main function.
 *
 * Initialization:
 * 1. Check for auto-resume (return from sleep with game running)
 * 2. Initialize graphics, input, power management
 * 3. Load menu state and recents
 *
 * Main Loop:
 * - Polls input (D-pad, buttons, shoulder buttons)
 * - Updates selection and scroll window
 * - Handles:
 *   - Navigation (up/down/left/right)
 *   - Alphabetical jump (L1/R1 shoulder buttons)
 *   - Open entry (A button)
 *   - Go back (B button)
 *   - Resume game (X button if save state exists)
 *   - Menu button (show version info or sleep)
 *   - Hardware settings (brightness/volume via PWR_update)
 * - Renders:
 *   - Entry list with selection highlight
 *   - Thumbnails from .res/ folders (if available)
 *   - Hardware status icons (battery, brightness, etc.)
 *   - Button hints at bottom
 * - Handles HDMI hotplug detection
 *
 * Exit:
 * - Saves state and cleans up resources
 * - If a ROM/app was launched, it's queued in /tmp/next
 */
int main(int argc, char* argv[]) {
	// Check for auto-resume first (fast path)
	if (autoResume())
		return 0;

	simple_mode = exists(SIMPLE_MODE_PATH);

	LOG_info("MinUI\n");
	InitSettings();

	SDL_Surface* screen = GFX_init(MODE_MAIN);
	// LOG_info("- graphics init: %lu\n", SDL_GetTicks() - main_begin);

	PAD_init();
	// LOG_info("- input init: %lu\n", SDL_GetTicks() - main_begin);

	PWR_init();
	if (!HAS_POWER_BUTTON && !simple_mode)
		PWR_disableSleep();
	// LOG_info("- power init: %lu\n", SDL_GetTicks() - main_begin);

	SDL_Surface* version = NULL;

	Menu_init();
	// LOG_info("- menu init: %lu\n", SDL_GetTicks() - main_begin);

	// Reduce CPU speed for menu browsing (saves power and heat)
	PWR_setCPUSpeed(CPU_SPEED_MENU);
	GFX_setVsync(VSYNC_STRICT);

	PAD_reset();
	int dirty = 1; // Set to 1 when screen needs redraw
	int show_version = 0; // 1 when showing version overlay
	int show_setting = 0; // 1=brightness, 2=volume overlay
	int was_online = PLAT_isOnline();

	// LOG_info("- loop start: %lu\n", SDL_GetTicks() - main_begin);
	while (!quit) {
		GFX_startFrame();
		unsigned long now = SDL_GetTicks();

		PAD_poll();

		int selected = top->selected;
		int total = top->entries->count;

		// Update power management (handles brightness/volume adjustments)
		PWR_update(&dirty, &show_setting, NULL, NULL);

		// Track online status changes (wifi icon)
		int is_online = PLAT_isOnline();
		if (was_online != is_online)
			dirty = 1;
		was_online = is_online;

		// Input handling - version overlay mode
		if (show_version) {
			if (PAD_justPressed(BTN_B) || PAD_tappedMenu(now)) {
				show_version = 0;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode)
					PWR_disableSleep();
			}
		} else {
			// Input handling - normal browsing mode
			if (PAD_tappedMenu(now)) {
				show_version = 1;
				dirty = 1;
				if (!HAS_POWER_BUTTON && !simple_mode)
					PWR_enableSleep();
			} else if (total > 0) {
				if (PAD_justRepeated(BTN_UP)) {
					if (selected == 0 && !PAD_justPressed(BTN_UP)) {
						// stop at top
					} else {
						selected -= 1;
						if (selected < 0) {
							selected = total - 1;
							int start = total - MAIN_ROW_COUNT;
							top->start = (start < 0) ? 0 : start;
							top->end = total;
						} else if (selected < top->start) {
							top->start -= 1;
							top->end -= 1;
						}
					}
				} else if (PAD_justRepeated(BTN_DOWN)) {
					if (selected == total - 1 && !PAD_justPressed(BTN_DOWN)) {
						// stop at bottom
					} else {
						selected += 1;
						if (selected >= total) {
							selected = 0;
							top->start = 0;
							top->end = (total < MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
						} else if (selected >= top->end) {
							top->start += 1;
							top->end += 1;
						}
					}
				}
				if (PAD_justRepeated(BTN_LEFT)) {
					selected -= MAIN_ROW_COUNT;
					if (selected < 0) {
						selected = 0;
						top->start = 0;
						top->end = (total < MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
					} else if (selected < top->start) {
						top->start -= MAIN_ROW_COUNT;
						if (top->start < 0)
							top->start = 0;
						top->end = top->start + MAIN_ROW_COUNT;
					}
				} else if (PAD_justRepeated(BTN_RIGHT)) {
					selected += MAIN_ROW_COUNT;
					if (selected >= total) {
						selected = total - 1;
						int start = total - MAIN_ROW_COUNT;
						top->start = (start < 0) ? 0 : start;
						top->end = total;
					} else if (selected >= top->end) {
						top->end += MAIN_ROW_COUNT;
						if (top->end > total)
							top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}

			// Alphabetical navigation with shoulder buttons
			if (PAD_justRepeated(BTN_L1) && !PAD_isPressed(BTN_R1) &&
			    !PWR_ignoreSettingInput(BTN_L1, show_setting)) {
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha - 1;
				if (i >= 0) {
					selected = top->alphas->items[i];
					if (total > MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end > total)
							top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			} else if (PAD_justRepeated(BTN_R1) && !PAD_isPressed(BTN_L1) &&
			           !PWR_ignoreSettingInput(BTN_R1, show_setting)) {
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha + 1;
				if (i < top->alphas->count) {
					selected = top->alphas->items[i];
					if (total > MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end > total)
							top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}

			// Update selection and mark dirty if changed
			if (selected != top->selected) {
				top->selected = selected;
				dirty = 1;
			}

			// Check if selected ROM has save state for resume
			if (dirty && total > 0)
				readyResume(top->entries->items[top->selected]);

			// Entry opening/navigation actions
			if (total > 0 && can_resume && PAD_justReleased(BTN_RESUME)) {
				should_resume = 1;
				Entry_open(top->entries->items[top->selected]);
				dirty = 1;
			} else if (total > 0 && PAD_justPressed(BTN_A)) {
				Entry_open(top->entries->items[top->selected]);
				total = top->entries->count;
				dirty = 1;

				if (total > 0)
					readyResume(top->entries->items[top->selected]);
			} else if (PAD_justPressed(BTN_B) && stack->count > 1) {
				closeDirectory();
				total = top->entries->count;
				dirty = 1;
				if (total > 0)
					readyResume(top->entries->items[top->selected]);
			}
		}

		// Rendering
		if (dirty) {
			GFX_clear(screen);

			int ox;
			int oy;

			// Thumbnail support:
			// For an entry named "NAME.EXT", check for /.res/NAME.EXT.png
			// Image must be <= FIXED_HEIGHT x FIXED_HEIGHT pixels
			int had_thumb = 0;
			if (!show_version && total > 0) {
				Entry* entry = top->entries->items[top->selected];
				char res_path[MAX_PATH];

				char res_root[MAX_PATH];
				strcpy(res_root, entry->path);

				char tmp_path[MAX_PATH];
				strcpy(tmp_path, entry->path);
				char* res_name = strrchr(tmp_path, '/') + 1;

				char* tmp = strrchr(res_root, '/');
				tmp[0] = '\0';

				sprintf(res_path, "%s/.res/%s.png", res_root, res_name);
				LOG_info("res_path: %s\n", res_path);
				if (exists(res_path)) {
					had_thumb = 1;
					SDL_Surface* thumb = IMG_Load(res_path);
					ox = MAX(FIXED_WIDTH - FIXED_HEIGHT, (FIXED_WIDTH - thumb->w));
					oy = (FIXED_HEIGHT - thumb->h) / 2;
					SDL_BlitSurface(thumb, NULL, screen, &(SDL_Rect){ox, oy});
					SDL_FreeSurface(thumb);
				}
			}

			int ow = GFX_blitHardwareGroup(screen, show_setting);

			if (show_version) {
				if (!version) {
					char release[256];
					getFile(ROOT_SYSTEM_PATH "/version.txt", release, 256);

					char *tmp, *commit;
					commit = strrchr(release, '\n');
					commit[0] = '\0';
					commit = strrchr(release, '\n') + 1;
					tmp = strchr(release, '\n');
					tmp[0] = '\0';

					// TODO: not sure if I want bare PLAT_* calls here
					char* extra_key = "Model";
					char* extra_val = PLAT_getModel();

					SDL_Surface* release_txt =
					    TTF_RenderUTF8_Blended(font.large, "Release", COLOR_DARK_TEXT);
					SDL_Surface* version_txt =
					    TTF_RenderUTF8_Blended(font.large, release, COLOR_WHITE);
					SDL_Surface* commit_txt =
					    TTF_RenderUTF8_Blended(font.large, "Commit", COLOR_DARK_TEXT);
					SDL_Surface* hash_txt = TTF_RenderUTF8_Blended(font.large, commit, COLOR_WHITE);

					SDL_Surface* key_txt =
					    TTF_RenderUTF8_Blended(font.large, extra_key, COLOR_DARK_TEXT);
					SDL_Surface* val_txt =
					    TTF_RenderUTF8_Blended(font.large, extra_val, COLOR_WHITE);

					int l_width = 0;
					int r_width = 0;

					if (release_txt->w > l_width)
						l_width = release_txt->w;
					if (commit_txt->w > l_width)
						l_width = commit_txt->w;
					if (key_txt->w > l_width)
						l_width = commit_txt->w;

					if (version_txt->w > r_width)
						r_width = version_txt->w;
					if (hash_txt->w > r_width)
						r_width = hash_txt->w;
					if (val_txt->w > r_width)
						r_width = val_txt->w;

#define VERSION_LINE_HEIGHT 24
					int x = l_width + SCALE1(8);
					int w = x + r_width;
					int h = SCALE1(VERSION_LINE_HEIGHT * 4);
					version = SDL_CreateRGBSurface(0, w, h, 16, 0, 0, 0, 0);

					SDL_BlitSurface(release_txt, NULL, version, &(SDL_Rect){0, 0});
					SDL_BlitSurface(version_txt, NULL, version, &(SDL_Rect){x, 0});
					SDL_BlitSurface(commit_txt, NULL, version,
					                &(SDL_Rect){0, SCALE1(VERSION_LINE_HEIGHT)});
					SDL_BlitSurface(hash_txt, NULL, version,
					                &(SDL_Rect){x, SCALE1(VERSION_LINE_HEIGHT)});

					SDL_BlitSurface(key_txt, NULL, version,
					                &(SDL_Rect){0, SCALE1(VERSION_LINE_HEIGHT * 3)});
					SDL_BlitSurface(val_txt, NULL, version,
					                &(SDL_Rect){x, SCALE1(VERSION_LINE_HEIGHT * 3)});

					SDL_FreeSurface(release_txt);
					SDL_FreeSurface(version_txt);
					SDL_FreeSurface(commit_txt);
					SDL_FreeSurface(hash_txt);
					SDL_FreeSurface(key_txt);
					SDL_FreeSurface(val_txt);
				}
				SDL_BlitSurface(
				    version, NULL, screen,
				    &(SDL_Rect){(screen->w - version->w) / 2, (screen->h - version->h) / 2});

				// buttons (duped and trimmed from below)
				if (show_setting && !GetHDMI())
					GFX_blitHardwareHints(screen, show_setting);
				else
					GFX_blitButtonGroup(
					    (char*[]){BTN_SLEEP == BTN_POWER ? "POWER" : "MENU", "SLEEP", NULL}, 0,
					    screen, 0);

				GFX_blitButtonGroup((char*[]){"B", "BACK", NULL}, 0, screen, 1);
			} else {
				// list
				if (total > 0) {
					int selected_row = top->selected - top->start;
					for (int i = top->start, j = 0; i < top->end; i++, j++) {
						Entry* entry = top->entries->items[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;
						int available_width =
						    (had_thumb && j != selected_row ? ox : screen->w) - SCALE1(PADDING * 2);
						if (i == top->start && !(had_thumb && j != selected_row))
							available_width -= ow; //

						SDL_Color text_color = COLOR_WHITE;

						trimSortingMeta(&entry_name);

						char display_name[256];
						int text_width = GFX_truncateText(
						    font.large, entry_unique ? entry_unique : entry_name, display_name,
						    available_width, SCALE1(BUTTON_PADDING * 2));
						int max_width = MIN(available_width, text_width);
						if (j == selected_row) {
							GFX_blitPill(ASSET_WHITE_PILL, screen,
							             &(SDL_Rect){SCALE1(PADDING),
							                         SCALE1(PADDING + (j * PILL_SIZE)), max_width,
							                         SCALE1(PILL_SIZE)});
							text_color = COLOR_BLACK;
						} else if (entry->unique) {
							trimSortingMeta(&entry_unique);
							char unique_name[256];
							GFX_truncateText(font.large, entry_unique, unique_name, available_width,
							                 SCALE1(BUTTON_PADDING * 2));

							SDL_Surface* text =
							    TTF_RenderUTF8_Blended(font.large, unique_name, COLOR_DARK_TEXT);
							SDL_BlitSurface(
							    text,
							    &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h},
							    screen,
							    &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING),
							                SCALE1(PADDING + (j * PILL_SIZE) + 4)});

							GFX_truncateText(font.large, entry_name, display_name, available_width,
							                 SCALE1(BUTTON_PADDING * 2));
						}
						SDL_Surface* text =
						    TTF_RenderUTF8_Blended(font.large, display_name, text_color);
						SDL_BlitSurface(
						    text,
						    &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h},
						    screen,
						    &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING),
						                SCALE1(PADDING + (j * PILL_SIZE) + 4)});
						SDL_FreeSurface(text);
					}
				} else {
					// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
					GFX_blitMessage(font.large, "Empty folder", screen,
					                &(SDL_Rect){0, 0, screen->w, screen->h}); //, NULL);
				}

				// buttons
				if (show_setting && !GetHDMI())
					GFX_blitHardwareHints(screen, show_setting);
				else if (can_resume)
					GFX_blitButtonGroup((char*[]){"X", "RESUME", NULL}, 0, screen, 0);
				else
					GFX_blitButtonGroup(
					    (char*[]){BTN_SLEEP == BTN_POWER ? "POWER" : "MENU",
					              BTN_SLEEP == BTN_POWER || simple_mode ? "SLEEP" : "INFO", NULL},
					    0, screen, 0);

				if (total == 0) {
					if (stack->count > 1) {
						GFX_blitButtonGroup((char*[]){"B", "BACK", NULL}, 0, screen, 1);
					}
				} else {
					if (stack->count > 1) {
						GFX_blitButtonGroup((char*[]){"B", "BACK", "A", "OPEN", NULL}, 1, screen,
						                    1);
					} else {
						GFX_blitButtonGroup((char*[]){"A", "OPEN", NULL}, 0, screen, 1);
					}
				}
			}

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();

		// if (!first_draw) {
		// 	first_draw = SDL_GetTicks();
		// 	LOG_info("- first draw: %lu\n", first_draw - main_begin);
		// }

		// HDMI hotplug detection
		// When HDMI is connected/disconnected, restart to reinit graphics
		// with correct resolution. Save state so we return to same position.
		static int had_hdmi = -1;
		int has_hdmi = GetHDMI();
		if (had_hdmi == -1)
			had_hdmi = has_hdmi;
		if (has_hdmi != had_hdmi) {
			had_hdmi = has_hdmi;

			Entry* entry = top->entries->items[top->selected];
			LOG_info("restarting after HDMI change... (%s)\n", entry->path);
			saveLast(entry->path);
			sleep(4); // Brief pause for HDMI to stabilize
			quit = 1;
		}
	}

	if (version)
		SDL_FreeSurface(version);

	Menu_quit();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
}