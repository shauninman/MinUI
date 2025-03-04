// heavily modified from the Onion original: https://github.com/OnionUI/Onion/tree/main/src/batmon
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <defines.h>
#include <utils.h>
#include <sqlite3.h>

#include <batmondb.h>

#define BATTERY_LOG_PATH SHARED_USERDATA_PATH
#define BATTERY_LOG_FILE BATTERY_LOG_PATH "/battery_logs.sqlite"

sqlite3* open_battery_log_db(void)
{
    mkdir(BATTERY_LOG_PATH, 0755);
    bool db_exists = exists(BATTERY_LOG_FILE);
    if (!db_exists)
        touch(BATTERY_LOG_FILE);

    sqlite3 *bat_log_db = NULL;

    if (sqlite3_open(BATTERY_LOG_FILE, &bat_log_db) != SQLITE_OK) {
        printf("%s\n", sqlite3_errmsg(bat_log_db));
        sqlite3_close(bat_log_db);
        bat_log_db = NULL;
        return NULL;
    }

    if (!db_exists) {
        sqlite3_exec(bat_log_db,
                     "DROP TABLE IF EXISTS bat_activity;"
                     "CREATE TABLE bat_activity(id INTEGER PRIMARY KEY, device_serial TEXT, bat_level INTEGER, duration INTEGER, is_charging INTEGER);"
                     "CREATE INDEX bat_activity_device_SN_index ON bat_activity(device_serial);",
                     NULL, NULL, NULL);
        sqlite3_exec(bat_log_db,
                     "DROP TABLE IF EXISTS device_specifics;"
                     "CREATE TABLE device_specifics(id INTEGER PRIMARY KEY, device_serial TEXT, best_session INTEGER);"
                     "CREATE INDEX device_specifics_index ON device_specifics(device_serial);",
                     NULL, NULL, NULL);
    }

    return bat_log_db;
}

void close_battery_log_db(sqlite3* bat_log_db)
{
    sqlite3_close(bat_log_db);
    bat_log_db = NULL;
}

int get_best_session_time(sqlite3* bat_log_db, const char* device)
{
    int best_time = 0;

    if (bat_log_db != NULL) {
        const char *sql = "SELECT * FROM device_specifics WHERE device_serial = ? ORDER BY id LIMIT 1;";
        sqlite3_stmt *stmt;

        int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, device, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                best_time = sqlite3_column_int(stmt, 2);
            }
            else {
                char *sql2 = sqlite3_mprintf("INSERT INTO device_specifics(device_serial, best_session) VALUES(%Q, %d);", device, 0);
                sqlite3_exec(bat_log_db, sql2, NULL, NULL, NULL);
                sqlite3_free(sql2);
            }
        }
        sqlite3_finalize(stmt);
    }
    
    return best_time;
}