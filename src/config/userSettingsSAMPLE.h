#ifndef USER_SETTINGS
#define USER_SETTINGS

// WiFi settings
// These must be customized for your network. It should be self explanitory.
#define WIFI_NETWORK            "INSERT_YOURS"
#define WIFI_PASSWORD           "INSERT_YOURS"
#define WIFI_TIMEOUT_MS         20000 // 20 second WiFi connection timeout
#define WIFI_RECOVER_TIME_MS    30000 // Wait 30 seconds after a failed connection attempt
#define HOSTNAME                "TipsyBrewKettle" // also used for domain name

// NTP settings
const char* ntpServer =             "us.pool.ntp.org";
const long  gmtOffset_sec =         -18000;     // US East Coast (Adjust for your locale)
const int   daylightOffset_sec =    3600;

// Temperature settings (all Celcius)
const float targetPreheat =         60.0;
const float targetTemp =            100.0;
#define TEMP_READ_FREQ              15000       // The temperature reading is stored once every this many ms
#define NUM_TEMP_READINGS           30

const float timeoutPump =           300000;     // maximum runtime of pump in milliseconds (safety cutoff)
const float timeoutHeat =           540000;     // maximum runtime of kettle in milliseconds (the builtin functionality of the kettle should render this useless, but better safe than sorry)

// Servo positions for the kettle arm
// You may have to tweak these values as your kettle, servo, and mount may effect them
#define KETTLE_ON 74
#define KETTLE_NEUTRAL 50
#define KETTLE_OFF 32

// OTA settings
#define OTA_ID          "TBK1"
#define OTA_USER        "admin"
#define OTA_PASS        "tipsybrewrules"

#define VERSION         "07.2023-1"

#endif