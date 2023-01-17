
#define SYSTEM_OK           0
#define WIFI_DISCONNECTED   1UL << 1
#define MQTT_ERROR          1UL << 2
#define NTP_SERVER_DOWN     1UL << 3


/* Basic logic enumerations */
enum {FALSE = 0, TRUE = 1};
enum {PC_OFF = 0, PC_ON = 1};
enum {POWER_ON = 0, SYSTEM_RESET = 1, STATUS_CHANGE = 2, LISTENING = 3, EXECUTE_COMMAND = 4};
enum {TURN_OFF = 0, TURN_ON = 1, HARD_RESET = 2, FORCE_SHUTDOWN = 3};

