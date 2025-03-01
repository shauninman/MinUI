// heavily modified from the Onion original: https://github.com/OnionUI/Onion/tree/main/src/batmon
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <defines.h>
#include <api.h>

#include <sqlite3.h>
#include <batmondb.h>

#define CHECK_BATTERY_TIMEOUT_S 15 // s - check battery percentage every 15s

// Battery logs
#define FILO_MIN_SIZE 1000
#define MAX_DURATION_BEFORE_UPDATE 600

static bool quit = false;
static bool is_suspended = false;

int battery_current_state_duration = 0;
int best_session_time = 0;
char *device_model = NULL;

void register_handler();
void sigintHandler(int signum) {
    switch (signum)
    {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    case SIGSTOP:
        is_suspended = true;
        break;
    case SIGCONT:
        is_suspended = false;
        break;
    default:
        break;
    }
    // reregister
    register_handler();
}

void register_handler() {
    struct sigaction sa;
    sa.sa_handler = sigintHandler;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaction(SIGINT, &sa, 0);

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaction(SIGTERM, &sa, 0);

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSTOP);
    sigaction(SIGSTOP, &sa, 0);

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGCONT);
    sigaction(SIGCONT, &sa, 0);
}

void cleanup(void)
{
    remove("/tmp/percBat");
}

void update_current_duration(void)
{
    sqlite3 *bat_log_db = open_battery_log_db();

    if (bat_log_db != NULL)
    {
        const char *sql = "SELECT * FROM bat_activity WHERE device_serial = ? ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);

        if (rc == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, device_model, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {
                int current_duration = sqlite3_column_int(stmt, 3);
                int new_duration = current_duration + battery_current_state_duration;

                const char *update_sql = "UPDATE bat_activity SET duration = ? WHERE id = ?";

                sqlite3_stmt *update_stmt;
                rc = sqlite3_prepare_v2(bat_log_db, update_sql, -1, &update_stmt, 0);

                if (rc == SQLITE_OK)
                {
                    sqlite3_bind_int(update_stmt, 1, new_duration);
                    sqlite3_bind_int(update_stmt, 2, sqlite3_column_int(stmt, 0));

                    // Exécuter la mise à jour
                    rc = sqlite3_step(update_stmt);

                    battery_current_state_duration = 0;
                    sqlite3_finalize(stmt);
                    sqlite3_finalize(update_stmt);
                }
            }
        }
        close_battery_log_db(bat_log_db);
    }
}

void log_new_percentage(int new_bat_value, int is_charging)
{
    sqlite3 *bat_log_db = open_battery_log_db();

    if (bat_log_db != NULL)
    {
        char *sql = sqlite3_mprintf("INSERT INTO bat_activity(device_serial, bat_level, duration, is_charging) VALUES(%Q, %d, %d, %d);", device_model, new_bat_value, 0, is_charging);
        sqlite3_exec(bat_log_db, sql, NULL, NULL, NULL);
        sqlite3_free(sql);

        // FILO logic
        sqlite3_stmt *stmt;
        const char *count_sql = "SELECT COUNT(id) FROM bat_activity";

        int count = 0;

        if (sqlite3_prepare_v2(bat_log_db, count_sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (count > FILO_MIN_SIZE)
        {
            // Deletion of the 1st entry
            const char *delete_sql = "DELETE FROM bat_activity WHERE id = (SELECT MIN(id) FROM bat_activity);";
            sqlite3_prepare_v2(bat_log_db, delete_sql, -1, &stmt, 0);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    close_battery_log_db(bat_log_db);
}

int get_current_session_time(void)
{
    int current_session_duration = 0;

    sqlite3 *bat_log_db = open_battery_log_db();
    
    if (bat_log_db != NULL)
    {
        const char *sql = "SELECT * FROM bat_activity WHERE device_serial = ? AND is_charging = 1 ORDER BY id DESC LIMIT 1;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);

        if (rc == SQLITE_OK)
        {

            sqlite3_bind_text(stmt, 1, device_model, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {

                sqlite3_stmt *stmt_sum;
                const char *query_sum = "SELECT SUM(duration) FROM bat_activity WHERE device_serial = ? AND id > ?;";
                rc = sqlite3_prepare_v2(bat_log_db, query_sum, -1, &stmt_sum, NULL);
                if (rc == SQLITE_OK)
                {
                    sqlite3_bind_text(stmt_sum, 1, device_model, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt_sum, 2, sqlite3_column_int(stmt, 0));
                    while ((rc = sqlite3_step(stmt_sum)) == SQLITE_ROW)
                    {
                        current_session_duration = sqlite3_column_int(stmt_sum, 0);
                    }
                }
                sqlite3_finalize(stmt_sum);
            }
        }
        sqlite3_finalize(stmt);
    }
    close_battery_log_db(bat_log_db);
    
    return current_session_duration;
}

int set_best_session_time(int best_session)
{
    int is_success = 0;

    sqlite3* bat_log_db = open_battery_log_db();

    if (bat_log_db != NULL)
    {
        const char *sql = "SELECT * FROM device_specifics WHERE device_serial = ? ORDER BY id LIMIT 1;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);

        if (rc == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, device_model, -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW)
            {
                const char *update_sql = "UPDATE device_specifics SET best_session = ? WHERE id = ?";

                sqlite3_stmt *update_stmt;
                rc = sqlite3_prepare_v2(bat_log_db, update_sql, -1, &update_stmt, 0);

                if (rc == SQLITE_OK)
                {
                    sqlite3_bind_int(update_stmt, 1, best_session);
                    sqlite3_bind_int(update_stmt, 2, sqlite3_column_int(stmt, 0));

                    // Exécuter la mise à jour
                    rc = sqlite3_step(update_stmt);

                    sqlite3_finalize(stmt);
                    sqlite3_finalize(update_stmt);
                    is_success = 1;
                }
            }
        }
        close_battery_log_db(bat_log_db);
    }

    return is_success;
}

int main(int argc, char *argv[])
{
    device_model = PLAT_getModel();
    sqlite3 *bat_log_db = open_battery_log_db();
    if(bat_log_db != NULL) {
        best_session_time = get_best_session_time(bat_log_db, device_model);
        close_battery_log_db(bat_log_db);
    }

    FILE *fp;
    int old_percentage = -1;
    int lowest_percentage_after_charge = 500;
    atexit(cleanup);
    register_handler();
    int ticks = CHECK_BATTERY_TIMEOUT_S;

    struct {
        int is_charging;
        int charge;
    } pwr = {0};
    bool was_charging = false;

    while (!quit)
    {
        PLAT_getBatteryStatusFine(&pwr.is_charging, &pwr.charge);
        if (pwr.is_charging)
        {
            if (!was_charging)
            {
                // Charging just started
                lowest_percentage_after_charge = 500; // Reset lowest percentage before charge
                was_charging = true;
                update_current_duration();

                int session_time = get_current_session_time();
                LOG_info("Charging detected - Previous session duration = %d\n", session_time);

                if (session_time > best_session_time)
                {
                    LOG_info("Best session duration\n", 1);
                    set_best_session_time(session_time);
                    best_session_time = session_time;
                }
                log_new_percentage(pwr.charge, was_charging);
            }
        }
        else if (was_charging)
        {
            // Charging just stopped
            was_charging = false;
            lowest_percentage_after_charge = 500; // Reset lowest percentage after charge

            LOG_info(
                "Charging stopped: suspended = %d, perc = %d\n",
                is_suspended, pwr.charge);

            update_current_duration();
            log_new_percentage(pwr.charge, was_charging);
        }

        if (!is_suspended)
        {
            if (ticks >= CHECK_BATTERY_TIMEOUT_S)
            {
                LOG_info(
                    "battery check: suspended = %d, perc = %d\n",
                    is_suspended, pwr.charge);

                ticks = -1;
            }

            if (pwr.charge != old_percentage)
            {
                // This statement is not englobed in the previous one
                // in order to be launched once when batmon starts
                LOG_info(
                    "saving percBat: suspended = %d, perc = %d\n",
                    is_suspended, pwr.charge);
                old_percentage = pwr.charge;
                // Save battery percentage to file
                if ((fp = fopen("/tmp/percBat", "w+"))) {
                    fprintf(fp, "%d", pwr.charge);
                    fflush(fp);
                    fsync(fileno(fp));
                    fclose(fp);
                }
                // Current battery state duration addition
                update_current_duration();
                // New battery percentage entry
                log_new_percentage(pwr.charge, was_charging);
            }
        }
        else
        {
            ticks = -1;
        }

        if (battery_current_state_duration > MAX_DURATION_BEFORE_UPDATE)
            update_current_duration();

        sleep(1);
        battery_current_state_duration++;
        ticks++;
    }

    LOG_info("caught SIGTERM/SIGINT, quitting\n");

    // Current battery state duration addition
    update_current_duration();
    return EXIT_SUCCESS;
}