/**
   @file getbootstate.c

   The getbootstate tool
   <p>
   Copyright (C) 2007-2010 Nokia Corporation.
   Copyright (C) 2011      Pali Rohár <pali.rohar@gmail.com>

   @author Peter De Schrijver <peter.de-schrijver@nokia.com>
   @author Semi Malinen <semi.malinen@nokia.com>
   @author Matias Muhonen <ext-matias.muhonen@nokia.com>

   This file is part of Dsme.

   Dsme is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Dsme is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Dsme.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <cal.h>
#include <sys/ioctl.h>

#define MAX_BOOTREASON_LEN   40
#define MAX_REBOOT_COUNT_LEN 40
#define MAX_SAVED_STATE_LEN  40

#define DEFAULT_MAX_BOOTS          10
#define DEFAULT_MAX_WD_RESETS       6

#define BOOT_LOOP_COUNT_PATH "/var/lib/dsme/boot_count"
#define SAVED_STATE_PATH     "/var/lib/dsme/saved_state"

#define BOOT_REASON_UNKNOWN         "unknown"
#define BOOT_REASON_SWDG_TIMEOUT    "swdg_to"
#define BOOT_REASON_SEC_VIOLATION   "sec_vio"
#define BOOT_REASON_32K_WDG_TIMEOUT "32wd_to"
#define BOOT_REASON_POWER_ON_RESET  "por"
#define BOOT_REASON_POWER_KEY       "pwr_key"
#define BOOT_REASON_MBUS            "mbus"
#define BOOT_REASON_CHARGER         "charger"
#define BOOT_REASON_USB             "usb"
#define BOOT_REASON_SW_RESET        "sw_rst"
#define BOOT_REASON_RTC_ALARM       "rtc_alarm"
#define BOOT_REASON_NSU             "nsu"

#define BOOT_MODE_UPDATE_MMC "update"
#define BOOT_MODE_LOCAL      "local"
#define BOOT_MODE_TEST       "test"
#define BOOT_MODE_NORMAL     "normal"

#define DEFAULT_CMDLINE_PATH "/proc/cmdline"
#define BOOT_REASON_PATH     "/proc/bootreason"
#define COMPONENT_PATH       "/proc/component_version"
#define MAX_LINE_LEN         1024

#define GETBOOTSTATE_PREFIX "getbootstate: "


// TWL_4030_MADC definitions from kernel module twl4030-madc.c
#define TWL4030_MADC_PATH              "/dev/twl4030-adc"
#define TWL4030_MADC_IOC_MAGIC         '`'
#define TWL4030_MADC_IOCX_ADC_RAW_READ _IO(TWL4030_MADC_IOC_MAGIC, 0)

struct twl4030_madc_user_parms {
    int channel;
    int average;
    int status;
    unsigned short int result;
};


static void log_msg(char* format, ...) __attribute__ ((format (printf, 1, 2)));

/**
 * get value from /proc/cmdline
 * @return 0 upon success, -1 otherwise
 * @note expected format in cmdline key1=value1 key2=value2, key3=value3
 *       key-value pairs separated by space or comma.
 *       value after key separated with equal sign '='
 **/
static int get_cmd_line_value(char* get_value, int max_len, char* key)
{
    const char* cmdline_path;
    FILE*       cmdline_file;
    char        cmdline[MAX_LINE_LEN];
    int         ret = -1;
    int         keylen;
    char*       key_and_value;
    char*       value;

    cmdline_path = getenv("CMDLINE_PATH");
    cmdline_path = cmdline_path ? cmdline_path : DEFAULT_CMDLINE_PATH;

    cmdline_file = fopen(cmdline_path, "r");
    if(!cmdline_file) {
        log_msg("Could not open %s\n", cmdline_path);
        return -1;
    }

    if (fgets(cmdline, MAX_LINE_LEN, cmdline_file)) {
        key_and_value = strtok(cmdline, " ,");
        keylen = strlen(key);
        while (key_and_value != NULL) {
            if(!strncmp(key_and_value, key, keylen)) {
                value = strtok(key_and_value, "=");
                value = strtok(NULL, "=");
                if (value) {
                    strncpy(get_value, value, max_len);
                    get_value[max_len-1] = 0;
                    ret = 0;
                }
                break;
            }
            key_and_value = strtok(NULL, " ,");
        }
    }
    fclose(cmdline_file);
    return ret;
}

// Behaviour from fremantle binary version:
// bootmode is read from file /proc/component_version
// file structure: <key> <spaces> <value>
//                 boot-mode <spaces> <bootmode>
static int get_bootmode(char* bootmode, int max_len)
{
    FILE* file;
    char  line[MAX_LINE_LEN];
    int   ret = -1;
    int   cmd;
    char* value;
    char* last;

    // Try also harmattan behaviour
    cmd = get_cmd_line_value(bootmode, max_len, "bootmode=");
    if (cmd == 0)
        return 0;

    file = fopen(COMPONENT_PATH, "r");
    if (!file) {
        log_msg("Could not open " COMPONENT_PATH " - %s\n", strerror(errno));
        return -1;
    }

    while (fgets(line, MAX_LINE_LEN, file)) {
        if (strncmp(line, "boot-mode", strlen("boot-mode")) == 0) {
            value = line + strlen("boot-mode");
            while (*value != 0 && *value != '\n' && *value <= ' ')
                value++;
            last = value;
            while (*last > ' ')
                last++;
            if (*last)
                *last = 0;
            strncpy(bootmode, value, max_len);
            bootmode[max_len-1] = 0;
            ret = 0;
            break;
        }
    }

    fclose(file);
    return ret;
}

// Behaviour from fremantle binary version:
// bootreason is read from file /proc/bootreason
static int get_bootreason(char* bootreason, int max_len)
{
    FILE* file;
    char* last;
    int   cmd;

    // Try also harmattan behaviour
    cmd = get_cmd_line_value(bootreason, max_len, "bootreason=");
    if (cmd == 0)
        return 0;

    file = fopen(BOOT_REASON_PATH, "r");
    if (!file) {
        log_msg("Could not open " BOOT_REASON_PATH " - %s\n", strerror(errno));
        return -1;
    }

    if(!fgets(bootreason, max_len, file))
        bootreason[0] = 0;

    last = bootreason;
    while (*last > ' ')
        last++;
    if (*last)
        *last = 0;

    fclose(file);
    return 0;
}

// Behaviour from fremantle binary version:
// parms structure from kernel module twl4030-madc.c
// parms: channel = 4, average = 1
// ioctl /dev/twl4030-adc 0x6000 &parms
// return: status = 0, result = BSI
static int get_bsi(void)
{
    int    fd;
    int    ret;
    struct twl4030_madc_user_parms par;

    fd = open(TWL4030_MADC_PATH, O_RDONLY);
    if (fd < 0) {
       fd = open("/tmp" TWL4030_MADC_PATH, O_RDONLY);
       if (fd < 0) {
          log_msg("Could not open " TWL4030_MADC_PATH " - %s\n", strerror(errno));
          return -1;
       }
    }

    par.channel = 4;
    par.average = 1;
    ret = ioctl(fd, TWL4030_MADC_IOCX_ADC_RAW_READ, &par);

    close(fd);

    if (ret != 0 || par.status != 0) {
       log_msg("Could not get battery type\n");
       return -1;
    }

    return par.result;
}

// Access cal partition on fremantle
// If enabled R&D mode return 1 otherwise 0
static int get_rdmode(void)
{
    struct cal*   cal;
    void*         ptr;
    char*         str;
    unsigned long len;
    int           ret;

    if (cal_init(&cal) < 0)
        return 0;

    ret = cal_read_block(cal, "r&d_mode", &ptr, &len, CAL_FLAG_USER);
    cal_finish(cal);

    if (ret < 0 || !ptr)
        return 0;

    str = (char *)ptr;

    if (len < 1 || !str[0])
        return 0;
    else
        return 1;
}

static void log_msg(char* format, ...)
{
    int     saved = errno; // preserve errno
    va_list ap;
    char    buffer[strlen(format) + strlen(GETBOOTSTATE_PREFIX) + 1];

    errno = saved;
    sprintf(buffer, "%s%s", GETBOOTSTATE_PREFIX, format);

    va_start(ap, format);
    vfprintf(stderr,buffer,ap);
    va_end(ap);
    errno = saved;
}


static int save_state(const char* state)
{
    FILE* saved_state_file;

    saved_state_file = fopen(SAVED_STATE_PATH ".new", "w");
    if(!saved_state_file) {
        log_msg("Could not open " SAVED_STATE_PATH ".new - %s\n",
                strerror(errno));
        return -1;
    }

    errno = 0;
    if(fwrite(state, 1, strlen(state), saved_state_file) <= 0) {
        log_msg("Could not write state" " - %s\n", strerror(errno));
        fclose(saved_state_file);
        return -1;
    }

    fflush(saved_state_file);
    if(fsync(fileno(saved_state_file)) < 0) {
        log_msg("Could not sync data" " - %s\n", strerror(errno));
        fclose(saved_state_file);
        return -1;
    }

    if(fclose(saved_state_file) < 0) {
        log_msg("Could not write state" " - %s\n", strerror(errno));
        return -1;
    }
    if(rename(SAVED_STATE_PATH ".new", SAVED_STATE_PATH)) {
        log_msg("Could not rename " SAVED_STATE_PATH ".new to "
                SAVED_STATE_PATH " - %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static char* get_saved_state(void)
{
    FILE* saved_state_file;
    char  saved_state[MAX_SAVED_STATE_LEN];
    char* ret;

    saved_state_file = fopen(SAVED_STATE_PATH, "r");
    if(!saved_state_file) {
        log_msg("Could not open " SAVED_STATE_PATH " - %s\n",
                strerror(errno));
        return "USER";
    }

    if(!fgets(saved_state, MAX_SAVED_STATE_LEN, saved_state_file)) {
        log_msg("Reading " SAVED_STATE_PATH " failed" " - %s\n",
                strerror(errno));
        fclose(saved_state_file);
        return "USER";
    }

    fclose(saved_state_file);

    ret = strdup(saved_state);

    if(!ret) {
        return "USER";
    } else {
        return ret;
    }
}


static void write_loop_counts(unsigned boots, unsigned wd_resets)
{
    FILE* f;

    if ((f = fopen(BOOT_LOOP_COUNT_PATH, "w")) == 0) {
        log_msg("Could not open " BOOT_LOOP_COUNT_PATH ": %s\n",
                strerror(errno));
    } else {
        if (fprintf(f, "%u %u", boots, wd_resets) < 0) {
            log_msg("Error writing " BOOT_LOOP_COUNT_PATH "\n");
        } else if (ferror(f) || fflush(f) == EOF) {
            log_msg("Error flushing " BOOT_LOOP_COUNT_PATH ": %s\n",
                    strerror(errno));
        } else if (fsync(fileno(f)) == -1) {
            log_msg("Error syncing " BOOT_LOOP_COUNT_PATH ": %s\n",
                    strerror(errno));
        }

        if (fclose(f) == EOF) {
            log_msg("Error closing " BOOT_LOOP_COUNT_PATH ": %s\n",
                    strerror(errno));
        }
    }
}

static void read_loop_counts(unsigned* boots, unsigned* wd_resets)
{
    *boots     = 0;
    *wd_resets = 0;

    FILE* f;

    if ((f = fopen(BOOT_LOOP_COUNT_PATH, "r")) == 0) {
        log_msg("Could not open " BOOT_LOOP_COUNT_PATH ": %s\n",
                strerror(errno));
    } else {
        if (fscanf(f, "%u %u", boots, wd_resets) != 2) {
            log_msg("Error reading file " BOOT_LOOP_COUNT_PATH "\n");
        }
        (void)fclose(f);
    }
}

static unsigned get_env(const char* name, unsigned default_value)
{
    const char* e = getenv(name);
    return e ? (unsigned)atoi(e) : default_value;
}

typedef enum {
    RESET_COUNTS    = 0x0,
    COUNT_BOOTS     = 0x1,
    COUNT_WD_RESETS = 0x2,
    COUNT_ALL    = (COUNT_BOOTS | COUNT_WD_RESETS)
} LOOP_COUNTING_TYPE;

static void check_for_boot_loops(LOOP_COUNTING_TYPE count_type,
                                 const char**       malf_info)
{
    unsigned      boots;
    unsigned      wd_resets;
    const char*   loop_malf_info = 0;
    unsigned      max_boots;
    unsigned      max_wd_resets;

    read_loop_counts(&boots, &wd_resets);

    // Obtain limits
    max_boots         = get_env("GETBOOTSTATE_MAX_BOOTS",
                                DEFAULT_MAX_BOOTS);
    max_wd_resets     = get_env("GETBOOTSTATE_MAX_WD_RESETS",
                                DEFAULT_MAX_WD_RESETS);

    // Check for too many frequent and consecutive reboots
    if (count_type & COUNT_BOOTS) {
        if (++boots > max_boots) {
            // Detected a boot loop
            loop_malf_info = "SOFTWARE unknown too frequent reboots";
            log_msg("%d reboots; loop detected\n", boots);
        } else {
            log_msg("Increased boot count to %d\n", boots);
        }
    } else {
        log_msg("Resetting boot counter\n");
        boots = 0;
    }

    // Check for too many frequent and consecutive WD resets
    if (count_type & COUNT_WD_RESETS) {
        if (++wd_resets > max_wd_resets) {
            // Detected a WD reset loop
            loop_malf_info = "SOFTWARE watchdog too frequent resets";
            log_msg("%d WD resets; loop detected\n", wd_resets);
        } else {
            log_msg("Increased WD reset count to %d\n", wd_resets);
        }
    } else {
        log_msg("Resetting WD reset counter\n");
        wd_resets = 0;
    }

    if (loop_malf_info) {
        // Malf detected;
        // reset counts so that a reboot can be attempted after the MALF
        boots     = 0;
        wd_resets = 0;

        // Pass malf information to the caller if it doesn't have any yet
        if (malf_info && !*malf_info) {
            *malf_info = loop_malf_info;
        }
    }

    write_loop_counts(boots, wd_resets);
}

static void return_bootstate(const char*        bootstate,
                             const char*        malf_info,
                             LOOP_COUNTING_TYPE count_type)
{
    // Only save "normal" bootstates (USER, ACT_DEAD)
    // Behaviour from fremantle binary:
    // Save SHUTDOWN bootstate too
    static const char* saveable[] = { "USER", "ACT_DEAD", "SHUTDOWN", 0 };
    int i;

    for (i = 0; saveable[i]; ++i) {
        if (!strncmp(bootstate, saveable[i], strlen(saveable[i]))) {
            save_state(saveable[i]);
            break;
        }
    }

    // Deal with possible startup loops
    check_for_boot_loops(count_type, &malf_info);

    // Print the bootstate to console and exit
    if (malf_info) {
        printf("%s %s\n", bootstate, malf_info);
    } else {
        puts(bootstate);
    }

    exit (0);
}

int main()
{
    char bootreason[MAX_BOOTREASON_LEN];
    char bootmode[MAX_BOOTREASON_LEN];
    int  rdmode;
    int  bsi;

    if(!get_bootmode(bootmode, MAX_BOOTREASON_LEN)) {
        if(!strcmp(bootmode, BOOT_MODE_UPDATE_MMC)) {
            log_msg("Update mode requested\n");
            return_bootstate("FLASH", 0, COUNT_BOOTS);
        }
        if(!strcmp(bootmode, BOOT_MODE_LOCAL)) {
            log_msg("LOCAL mode requested\n");
            return_bootstate("LOCAL", 0, COUNT_BOOTS);
        }
        if(!strcmp(bootmode, BOOT_MODE_TEST)) {
            log_msg("TEST mode requested\n");
            return_bootstate("TEST", 0, COUNT_BOOTS);
        }
    }


    if(get_bootreason(bootreason, MAX_BOOTREASON_LEN) < 0) {
        log_msg("Bootreason could not be read\n");
        return_bootstate("MALF",
                         "SOFTWARE bootloader no bootreason",
                         COUNT_BOOTS);
    }


    if (!strcmp(bootreason, BOOT_REASON_SEC_VIOLATION)) {
        log_msg("Security violation\n");
        // TODO: check if "software bootloader" is ok
        return_bootstate("MALF",
                         "SOFTWARE bootloader security violation",
                         COUNT_BOOTS);
    }


    bsi = get_bsi();
    rdmode = get_rdmode();


    // Behaviour from fremantle binary version:
    // BSI 32..85 - Service battery and LOCAL bootstate
    // BSI 87..176 - Test battery and TEST bootstate
    // BSI 280..568 - Normal battery and continue checking
    // Other BSI values and not in R&D mode - Show error and SHUTDOWN bootstate
    if (bsi >= 32 && bsi <= 85) {
        log_msg("Service battery detected\n");
        return_bootstate("LOCAL", 0, COUNT_BOOTS);
    } else if (bsi >= 87 && bsi <= 176) {
        log_msg("Test battery detected\n");
        return_bootstate("TEST", 0, COUNT_BOOTS);
    } else if (!rdmode && (bsi < 280 || bsi > 568)) {
        log_msg("Unknown battery detected. raw bsi value %d\n", bsi);
        return_bootstate("SHUTDOWN", 0, COUNT_BOOTS);
    }


    if (!strcmp(bootreason, BOOT_REASON_POWER_ON_RESET) ||
        !strcmp(bootreason, BOOT_REASON_SWDG_TIMEOUT)   ||
        !strcmp(bootreason, BOOT_REASON_32K_WDG_TIMEOUT))
    {
        char* saved_state;
        char* new_state;

        saved_state = get_saved_state();

        // We decided to select "USER" to prevent ACT_DEAD reboot loop
        new_state = "USER";

        log_msg("Unexpected reset occured (%s). "
                  "Previous bootstate=%s - selecting %s\n",
                bootreason,
                saved_state,
                new_state);

        LOOP_COUNTING_TYPE count;
        if (!strcmp(bootreason, BOOT_REASON_POWER_ON_RESET)) {
            count = RESET_COUNTS; // zero loop counters on power on reset
        } else {
            count = COUNT_ALL;
        }
        return_bootstate(new_state, 0, count);
    }

    if(!strcmp(bootreason,BOOT_REASON_SW_RESET))   {
        char* saved_state;

        /* User requested reboot.
         * Boot back to state where we were (saved_state).
         * But if normal mode was requested to get out of
         * special mode (like LOCAL or TEST),
         * then boot to USER mode
         */
        saved_state = get_saved_state();
        log_msg("User requested reboot (saved_state=%s, bootreason=%s)\n",
                saved_state,
                bootreason);
        if(strcmp(saved_state, "ACT_DEAD") &&
           strcmp(saved_state, "USER")     &&
           !strcmp(bootmode, BOOT_MODE_NORMAL))
        {
            log_msg("request was to NORMAL mode\n");
            return_bootstate("USER", 0, COUNT_BOOTS);
        } else {
            return_bootstate(saved_state, 0, COUNT_BOOTS);
        }
    }

    if(!strcmp(bootreason, BOOT_REASON_POWER_KEY)) {
        log_msg("User pressed power button\n");
        return_bootstate("USER", 0, RESET_COUNTS); // reset loop counters
    }
    if(!strcmp(bootreason, BOOT_REASON_NSU)) {
        log_msg("software update (NSU)\n");
        return_bootstate("USER", 0, COUNT_BOOTS);
    }

    if(!strcmp(bootreason, BOOT_REASON_CHARGER) ||
       !strcmp(bootreason, BOOT_REASON_USB))
    {
        log_msg("User attached charger\n");
        return_bootstate("ACT_DEAD", 0, COUNT_BOOTS);
    }

    if(!strcmp(bootreason, BOOT_REASON_RTC_ALARM)) {
        log_msg("Alarm wakeup occured\n");
        return_bootstate("ACT_DEAD", 0, COUNT_BOOTS);
    }

    log_msg("Unknown bootreason '%s' passed by nolo\n", bootreason);
    return_bootstate("MALF",
                     "SOFTWARE bootloader unknown bootreason to getbootstate",
                     COUNT_BOOTS);

    return 0; // never reached
}
