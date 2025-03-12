// heavily modified from the Onion original: https://github.com/OnionUI/Onion/tree/main/src/batmon
#ifndef __gametime_db_h__
#define __gametime_db_h__

typedef struct ROM ROM;
typedef struct PlayActivity PlayActivity;
typedef struct PlayActivities PlayActivities;

struct ROM {
    int id;
    char *type;
    char *name;
    char *file_path;
    char *image_path;
};
struct PlayActivity {
    ROM *rom;
    int play_count;
    int play_time_total;
    int play_time_average;
    char *first_played_at;
    char *last_played_at;
};
struct PlayActivities {
    PlayActivity **play_activity;
    int count;
    int play_time_total;
};

sqlite3* play_activity_db_open(void);
void play_activity_db_close(sqlite3* ctx);
void free_play_activities(PlayActivities *pa_ptr);

// Main interface functions for read access
PlayActivities *play_activity_find_all(void);
//int play_activity_get_play_time(const char *rom_path);

// Main interface functions for write access
void play_activity_start(char *rom_file_path);
void play_activity_resume(void);
void play_activity_stop(char *rom_file_path);
void play_activity_stop_all(void);
void play_activity_list_all(void);

#endif // __gametime_db_h__