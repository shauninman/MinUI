// heavily modified from the Onion original: https://github.com/OnionUI/Onion/blob/main/src/playActivity/playActivityDB.h
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <defines.h>
#include <utils.h>
#include <sqlite3.h>

#include "gametimedb.h"

#define CMD_TO_RUN "/tmp/next"
#define ROM_NOT_FOUND -1

#define GAMETIME_LOG_PATH SHARED_USERDATA_PATH
#define GAMETIME_LOG_FILE GAMETIME_LOG_PATH "/game_logs.sqlite"

sqlite3* play_activity_db_open(void)
{
    mkdir(GAMETIME_LOG_PATH, 0777);
    bool db_exists = exists(GAMETIME_LOG_FILE);
    if (!db_exists)
        touch(GAMETIME_LOG_FILE);

    sqlite3 *game_log_db = NULL;

    if (sqlite3_open(GAMETIME_LOG_FILE, &game_log_db) != SQLITE_OK) {
        printf("%s\n", sqlite3_errmsg(game_log_db));
        play_activity_db_close(game_log_db);
        return NULL;
    }

    if (!db_exists) {
        sqlite3_exec(game_log_db,
                     "DROP TABLE IF EXISTS rom;"
                     "CREATE TABLE rom(id INTEGER PRIMARY KEY, type TEXT, name TEXT, file_path TEXT, image_path TEXT, created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                     "CREATE UNIQUE INDEX rom_id_index ON rom(id);",
                     NULL, NULL, NULL);
        sqlite3_exec(game_log_db,
                     "DROP TABLE IF EXISTS play_activity;"
                     "CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                     "CREATE INDEX play_activity_rom_id_index ON play_activity(rom_id);",
                     NULL, NULL, NULL);
    }

    return game_log_db;
}

void play_activity_db_close(sqlite3* game_log_db)
{
    sqlite3_close(game_log_db);
    game_log_db = NULL;
}

void free_play_activities(PlayActivities *pa_ptr)
{
    for (int i = 0; i < pa_ptr->count; i++) {
        free(pa_ptr->play_activity[i]->first_played_at);
        free(pa_ptr->play_activity[i]->last_played_at);
        free(pa_ptr->play_activity[i]->rom);
        free(pa_ptr->play_activity[i]);
    }
    free(pa_ptr->play_activity);
    free(pa_ptr);
}

void get_rom_image_path(char *rom_file, char *out_image_path)
{
    if (suffixMatch(rom_file, ".p8") || suffixMatch(rom_file, ".png")) {
        snprintf(out_image_path, STR_MAX - 1, ROMS_PATH "/%s", rom_file);
    }
    
    char *clean_rom_name = removeExtension(baseName(rom_file));
    // this assumes all media resides in a top-level .media folder
    //char *rom_folder = strtok(rom_file, "/");
    //snprintf(out_image_path, STR_MAX - 1, ROMS_PATH "/%s/.media/%s.png", rom_folder, clean_rom_name);
    // this assumes that roms in subfolders have corresponding game art in
    // a .media folder in the respective subfolder
    char rom_folder_path[MAX_PATH];
    folderPath(rom_file, rom_folder_path);

    snprintf(out_image_path, STR_MAX - 1, ROMS_PATH "/%s/.media/%s.png", rom_folder_path, clean_rom_name);
    //LOG_debug("out_image_path: %s\n", out_image_path);
    free(clean_rom_name);
}

int play_activity_db_transaction(sqlite3* game_log_db, int (*exec_transaction)(sqlite3*))
{
    int retval;
    retval = exec_transaction(game_log_db);
    return retval;
}

int play_activity_db_execute(char *sql)
{
    //LOG_info("play_activity_db_execute(%s)\n", sql);
    sqlite3* game_log_db = play_activity_db_open();
    int rc = sqlite3_exec(game_log_db, sql, NULL, NULL, NULL);
    play_activity_db_close(game_log_db);
    return rc;
}

sqlite3_stmt *play_activity_db_prepare(sqlite3* game_log_db, char *sql)
{
    //LOG_info("play_activity_db_prepare(%s)\n", sql);
    if (game_log_db == NULL) {
        printf("DB is not open");
        return NULL;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(game_log_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("%s: %s\n", sqlite3_errmsg(game_log_db), sqlite3_sql(stmt));
    }
    return stmt;
}

int play_activity_get_total_play_time(void)
{
    int total_play_time = 0;
    char *sql =
        "SELECT SUM(play_time_total) FROM (SELECT SUM(play_time) AS play_time_total FROM play_activity GROUP BY rom_id) "
        "WHERE play_time_total > 60;";
    sqlite3_stmt *stmt;

    sqlite3* game_log_db = play_activity_db_open();
    stmt = play_activity_db_prepare(game_log_db, sql);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total_play_time = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    play_activity_db_close(game_log_db);

    return total_play_time;
}

PlayActivities *play_activity_find_all(void)
{
    PlayActivities *play_activities = NULL;
    char *sql =
        "SELECT * FROM ("
        "    SELECT rom.id, rom.type, rom.name, rom.file_path, "
        "           COUNT(play_activity.ROWID) AS play_count_total, "
        "           SUM(play_activity.play_time) AS play_time_total, "
        "           SUM(play_activity.play_time)/COUNT(play_activity.ROWID) AS play_time_average, "
        "           datetime(MIN(play_activity.created_at), 'unixepoch') AS first_played_at, "
        "           datetime(MAX(play_activity.created_at), 'unixepoch') AS last_played_at "
        "    FROM rom LEFT JOIN play_activity ON rom.id = play_activity.rom_id "
        "    GROUP BY rom.id) "
        "WHERE play_time_total > 0 "
        "ORDER BY play_time_total DESC;";
    sqlite3_stmt *stmt;

    sqlite3* game_log_db = play_activity_db_open();

    stmt = play_activity_db_prepare(game_log_db, sql);

    int play_activity_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        play_activity_count++;
    }
    sqlite3_reset(stmt);

    play_activities = (PlayActivities *)malloc(sizeof(PlayActivities));
    play_activities->count = play_activity_count;
    play_activities->play_time_total = 0;
    play_activities->play_activity = (PlayActivity **)malloc(sizeof(PlayActivity *) * play_activities->count);

    for (int i = 0; i < play_activities->count; i++) {
        if (sqlite3_step(stmt) != SQLITE_ROW)
            break;

        PlayActivity *entry = play_activities->play_activity[i] = (PlayActivity *)malloc(sizeof(PlayActivity));
        ROM *rom = play_activities->play_activity[i]->rom = (ROM *)malloc(sizeof(ROM));
        entry->first_played_at = NULL;
        entry->last_played_at = NULL;

        rom->id = sqlite3_column_int(stmt, 0);
        rom->type = strdup((const char *)sqlite3_column_text(stmt, 1));
        rom->name = strdup((const char *)sqlite3_column_text(stmt, 2));
        if (sqlite3_column_text(stmt, 3) != NULL) {
            rom->file_path = strdup((const char *)sqlite3_column_text(stmt, 3));
            rom->image_path = malloc(STR_MAX * sizeof(char));
            memset(rom->image_path, 0, STR_MAX);
            get_rom_image_path(rom->file_path, rom->image_path);
        }

        entry->play_count = sqlite3_column_int(stmt, 4);
        entry->play_time_total = sqlite3_column_int(stmt, 5);
        entry->play_time_average = sqlite3_column_int(stmt, 6);
        if (sqlite3_column_text(stmt, 8) != NULL) {
            entry->first_played_at = strdup((const char *)sqlite3_column_text(stmt, 7));
        }
        if (sqlite3_column_text(stmt, 9) != NULL) {
            entry->last_played_at = strdup((const char *)sqlite3_column_text(stmt, 8));
        }

        play_activities->play_time_total += entry->play_time_total;
    }

    sqlite3_finalize(stmt);
    play_activity_db_close(game_log_db);

    return play_activities;
}

void __ensure_rel_path(char *rel_path, const char *rom_path)
{
    if (!pathRelativeTo(rel_path, ROMS_PATH, rom_path)) {
        if (strstr(rom_path, "../../Roms/") != NULL) {
            strcpy(rel_path, splitString(strdup((const char *)rom_path), "../../Roms/"));
        }
        else {
            strcpy(rel_path, replaceString2(strdup((const char *)rom_path), ROMS_PATH "/", ""));
        }
    }
}

int __db_insert_rom(sqlite3* game_log_db, const char *rom_type, const char *rom_name, const char *file_path, const char *image_path)
{
    int rom_id = ROM_NOT_FOUND;

    char rel_path[MAX_PATH];
    __ensure_rel_path(rel_path, file_path);

    char *sql = sqlite3_mprintf("INSERT INTO rom(type, name, file_path, image_path) VALUES(%Q, %Q, %Q, %Q);",
                                rom_type, rom_name, rel_path, image_path);
    sqlite3_exec(game_log_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sqlite3_stmt *stmt = play_activity_db_prepare(game_log_db, "SELECT id FROM rom WHERE ROWID = last_insert_rowid()");
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return rom_id;
}

void __db_update_rom(sqlite3* game_log_db, int rom_id, const char *rom_type, const char *rom_name, const char *file_path, const char *image_path)
{
    char rel_path[MAX_PATH];
    __ensure_rel_path(rel_path, file_path);

    char *sql = sqlite3_mprintf("UPDATE rom SET type = %Q, name = %Q, file_path = %Q, image_path = %Q WHERE id = %d;",
                                rom_type, rom_name, rel_path, image_path, rom_id);
    sqlite3_exec(game_log_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
}

int __db_get_orphan_rom_id(sqlite3* game_log_db, const char *rom_path)
{
    int rom_id = ROM_NOT_FOUND;
    char *_file_name = strdup(rom_path);
    const char *file_name = baseName(_file_name);
    char *rom_name = removeExtension(file_name);

    char *sql = sqlite3_mprintf("SELECT id FROM rom WHERE (name=%Q OR name=%Q) AND type='ORPHAN' LIMIT 1;", rom_name, file_name);
    sqlite3_stmt *stmt = play_activity_db_prepare(game_log_db, sql);
    sqlite3_free(sql);
    free(rom_name);
    free(_file_name);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return rom_id;
}

int __db_get_rom_id_by_path(sqlite3* game_log_db, const char *rom_path)
{
    int rom_id = ROM_NOT_FOUND;

    char rel_path[MAX_PATH];
    __ensure_rel_path(rel_path, rom_path);

    char *sql = sqlite3_mprintf("SELECT id FROM rom WHERE file_path=%Q LIMIT 1;", rel_path);
    sqlite3_stmt *stmt = play_activity_db_prepare(game_log_db, sql);
    sqlite3_free(sql);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return rom_id;
}

int __db_rom_find_by_file_path(sqlite3* game_log_db, const char *rom_path, bool create_or_update)
{
    //LOG_info("rom_find_by_file_path('%s')\n", rom_path);

    bool update_orphan = false;
    int rom_id = __db_get_rom_id_by_path(game_log_db, rom_path);

    if (rom_id == ROM_NOT_FOUND) {
        rom_id = __db_get_orphan_rom_id(game_log_db, rom_path);
        if (rom_id != ROM_NOT_FOUND) {
            update_orphan = true;
        }
    }

    if (update_orphan) {
        char *rom_name = removeExtension(baseName(rom_path));
        __db_update_rom(game_log_db, rom_id, "", rom_name, rom_path, "");
        free(rom_name);
    }
    else if (rom_id == ROM_NOT_FOUND && create_or_update) {
        char *rom_name = removeExtension(baseName(rom_path));
        rom_id = __db_insert_rom(game_log_db, "", rom_name, rom_path, "");
        free(rom_name);
    }

    return rom_id;
}

int play_activity_transaction_rom_find_by_file_path(const char *rom_path, bool create_or_update)
{
    int retval;
    sqlite3* game_log_db = play_activity_db_open();
    retval = __db_rom_find_by_file_path(game_log_db, rom_path, create_or_update);
    play_activity_db_close(game_log_db);
    return retval;
}

int play_activity_get_play_time(const char *rom_path)
{
    int play_time = 0;
    sqlite3* game_log_db = play_activity_db_open();
    int rom_id = __db_rom_find_by_file_path(game_log_db, rom_path, false);
    if (rom_id != ROM_NOT_FOUND) {
        char *sql = sqlite3_mprintf("SELECT SUM(play_time) FROM play_activity WHERE rom_id = %d;", rom_id);
        sqlite3_stmt *stmt = play_activity_db_prepare(game_log_db, sql);
        sqlite3_free(sql);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            play_time = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    play_activity_db_close(game_log_db);
    return play_time;
}

bool _get_active_rom_path(char *rom_path_out)
{
    char *ptr;
    char cmd[STR_MAX];
    getFile(CMD_TO_RUN, cmd, STR_MAX);
    trimTrailingNewlines(cmd);

    if (strlen(cmd) == 0) {
        return false;
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        *ptr = '\0';
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        strncpy(rom_path_out, ptr + 1, STR_MAX);
        return true;
    }

    return false;
}

int __db_get_active_closed_activity(sqlite3* game_log_db)
{
    int rom_id = ROM_NOT_FOUND;

    char rom_path[STR_MAX];
    if (!_get_active_rom_path(rom_path)) {
        return ROM_NOT_FOUND;
    }

    //LOG_info("Last closed active rom: %s\n", rom_path);

    if ((rom_id = __db_rom_find_by_file_path(game_log_db, rom_path, false)) == ROM_NOT_FOUND) {
        return ROM_NOT_FOUND;
    }

    char *sql = sqlite3_mprintf("SELECT * FROM play_activity WHERE rom_id = %d AND play_time IS NULL;", rom_id);
    sqlite3_stmt *stmt = play_activity_db_prepare(game_log_db, sql);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Activity is not closed
        rom_id = ROM_NOT_FOUND;
    }

    sqlite3_free(sql);
    sqlite3_finalize(stmt);

    return rom_id;
}

void play_activity_start(char *rom_file_path)
{
    //LOG_info("\n:: play_activity_start(%s)\n", rom_file_path);
    int rom_id = play_activity_transaction_rom_find_by_file_path(rom_file_path, true);
    if (rom_id == ROM_NOT_FOUND) {
        exit(1);
    }
    char *sql = sqlite3_mprintf("INSERT INTO play_activity(rom_id) VALUES(%d);", rom_id);
    play_activity_db_execute(sql);
    sqlite3_free(sql);
}

void play_activity_resume(void)
{
    //LOG_info("\n:: play_activity_resume()");
    sqlite3* game_log_db = play_activity_db_open();
    int rom_id = play_activity_db_transaction(game_log_db, __db_get_active_closed_activity);
    play_activity_db_close(game_log_db);
    if (rom_id == ROM_NOT_FOUND) {
        printf("Error: no active rom\n");
        exit(1);
    }
    char *sql = sqlite3_mprintf("INSERT INTO play_activity(rom_id) VALUES(%d);", rom_id);
    play_activity_db_execute(sql);
    sqlite3_free(sql);
}

void play_activity_stop(char *rom_file_path)
{
    //LOG_info("\n:: play_activity_stop(%s)\n", rom_file_path);
    int rom_id = play_activity_transaction_rom_find_by_file_path(rom_file_path, false);
    if (rom_id == ROM_NOT_FOUND) {
        exit(1);
    }
    char *sql = sqlite3_mprintf("UPDATE play_activity SET play_time = (strftime('%%s', 'now')) - created_at, updated_at = (strftime('%%s', 'now')) WHERE rom_id = %d AND play_time IS NULL;", rom_id);
    play_activity_db_execute(sql);
    sqlite3_free(sql);
}

void play_activity_stop_all(void)
{
    //LOG_info("\n:: play_activity_stop_all()");
    play_activity_db_execute(
        "UPDATE play_activity SET play_time = (strftime('%s', 'now')) - created_at, updated_at = (strftime('%s', 'now')) WHERE play_time IS NULL;"
        "DELETE FROM play_activity WHERE play_time < 0;");
}

void play_activity_list_all(void)
{
    //LOG_info("\n:: play_activity_list_all()");
    int total_play_time = play_activity_get_total_play_time();
    PlayActivities *pas = play_activity_find_all();

    printf("\n");

    for (int i = 0; i < pas->count; i++) {
        PlayActivity *entry = pas->play_activity[i];
        ROM *rom = entry->rom;
        char rom_name[STR_MAX];
        cleanName(rom_name, rom->name);
        char play_time[STR_MAX];
        serializeTime(play_time, entry->play_time_total);
        printf("%03d: %s (%s) [%s]\n", i + 1, rom_name, play_time, rom->type);
    }

    char total_str[25];
    serializeTime(total_str, total_play_time);
    printf("\nTotal: %s\n", total_str);

    free_play_activities(pas);
}