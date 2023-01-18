#define firmware_v      "V0.02.2 - 18 Jan 23"
#define                 SERIAL_PRINTING

// SET DOT LED MATRIX DISPLAY PARAMETERS =============================================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW       // @EB-setup define the type of hardware connected
#define CLK_PIN         18                      // @EB-setup CLK / SCK pin number  (EB setup: yellow)
#define DATA_PIN        23                      // @EB-setup DIN / MOSI pin number (EB setup: brown)
#define CS_PIN          5                       // @EB-setup LOAD / SS pin number  (EB setup: green)
#define MAX_DEVICES     4                       // Define the number of devices we have in the chain and the hardware interface
#define CHAR_SPACING    1                       // pixels between characters
#define PRINTS(x)
#define DELAYTIME       50                      // in milliseconds

// SET GPIO PIN PARAMETERS ===========================================================================
#define PR_Relay        33                      // Connects to power relay channel
#define RS_Relay        27                      // Connects to reset relay channel
#define BL_LED_Relay    14                      // Connects to Blue LED relay channel - not yet implemented
#define WT_LED_RELAY    12                      // Connects to White LED relay channel - not yet implemented
#define PWR_SW          35                      // Connects to Power Switch with input pulldown
#define RST_SW          32                      // Connects to Reset Switch with input pulldown
#define RST_State       34                      // If HIGH it means the PC is ON
#define LED_BUILTIN     2

// NTP SERVER PARAMETERS =============================================================================
#define ntpServer           "pool.ntp.org"
#define gmtOffset_sec       7200
#define daylightOffset_sec  3600
#define host                "ESP32_PC_Controller"

// Global message buffers shared by Serial and Scrolling functions ===================================
#define BUFFER_LENGTH       32

/* MESSAGES */
#define Greeting        "Hello, Zissis\nMAXIMUS-III is now on power-grid!\nAssigned IP address to System Controller is "