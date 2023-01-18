#include "stdint.h"

typedef uint32_t    SYS_STATUS;
typedef uint8_t     SYS_STATE;

void ErrorResolver(void);
void ExecuteState(void);
uint8_t CheckPCState(void);
void PowerONRoutine(void);
void PCStatusChange(void);
void ListeningRoutine(void);
void ExecuteSystemCommand(void);
void FirmwareUpdateMode(void);
uint8_t SetupFirmwareUpdate(void);
void RelayAction(uint8_t select, uint16_t duration);
void PrintSystemStatus(void);
uint32_t CheckSystemError(void);
uint8_t CheckErrorID(uint32_t error);
void SetErrorID(uint32_t error);
void ClearErrorID(uint32_t error);
void CheckWiFIConnection(void);
void ConnectToWiFi(void);
void GetNetworkTime(void);
void EnableInterrupts(void);
void IRAM_ATTR Power(void);
void IRAM_ATTR Reset(void);
void IRAM_ATTR Status(void);
void Error_Handler(void);

// =================== TIME FUNCTIONS ======================================================================
void DisplayTime(void); 
void Time(void);

// ================== TELEGRAM FUNCTIONS ===================================================================
void ListenForNewMessages(void);
void HandleNewMessages(int numNewMessages);

// =================== DISPLAY FUNCTIONS ===================================================================
void delayAndClear(uint32_t delay_ms); 
void TranscodeScroll(String c, uint32_t delay_ms);              // Scrolls a string on the display and holds it for x millisecs
void scrollText(char *p);                                       // Function for text scrolling on DOT LED DISPLAY
void printText(uint8_t modStart, uint8_t modEnd, char *pMsg);   // Function for displaying Text
void printString(String s);                                     // Prints a short string on the display without scrolling
