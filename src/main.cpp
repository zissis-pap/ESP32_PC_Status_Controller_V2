#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <main.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include <ArduinoJson.h>
#include <defines.h>
#include <enumerations.h>
#include <consts.h>
#include <credentials.h>
#include <login_page.h>
#include <server_index.h>
#include <string>

// ENABLE CORE 0
TaskHandle_t OnCore0;
void Core0(void *pvParameters); // Function to run on core 0

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES); // SPI hardware interface
WiFiClientSecure espClient;
WebServer server(80);
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, espClient);

/* VARIABLES*/
uint8_t PC_STATUS;
uint8_t LAST_PC_STATUS;
uint8_t SYSTEM_COMMAND; // Will be used
uint8_t NewMessageCounter = 0;
String print_data[BUFFER_LENGTH];
bool printing = FALSE;
SYS_STATUS SYSTEM_STATUS = NTP_SERVER_DOWN;
SYS_STATE  SYSTEM_STATE  = POWER_ON;

// Variables to save date and time
struct tm timeinfo;
String formattedDate;


void setup() 
{
   // Create a task that will be executed on core 0 in the Core0() function, with priority 1.
  xTaskCreatePinnedToCore
  (
    Core0,       /* Task function. */
    "OnCore0",   /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &OnCore0,    /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */ 

  // Set up Inputs and Outputs
  pinMode(PR_Relay, OUTPUT);
  pinMode(RS_Relay, OUTPUT);
  pinMode(BL_LED_Relay, OUTPUT);
  pinMode(WT_LED_RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(PR_Relay, HIGH);
  digitalWrite(RS_Relay, HIGH);
  digitalWrite(BL_LED_Relay, HIGH);
  digitalWrite(WT_LED_RELAY, HIGH);
  pinMode(PWR_SW, INPUT_PULLDOWN);
  pinMode(RST_SW, INPUT_PULLDOWN);
  pinMode(RST_State, INPUT_PULLDOWN);
  // Attach the two Interrupts
  EnableInterrupts();

  Serial.begin(115200); // Init serial port
  Serial.printf("PC CONTROLLER ");
  Serial.print(firmware_v);
  Serial.printf("\n");
  
  mx.begin();
  espClient.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  CheckWiFIConnection();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printString(".");
  #ifdef SERIAL_PRINTING
    Serial.printf("\nPC CONTROLLER STARTED\n");
  #endif
}

void loop() 
{
  ErrorResolver();
  ExecuteState();
}

void ErrorResolver(void)
{
  while(SYSTEM_STATUS != SYSTEM_OK)
  {
    #ifdef SERIAL_PRINTING
      Serial.printf("\nCHECK ERROR ID\n");
    #endif
    PrintSystemStatus();
    switch(CheckSystemError())
    {
      case SYSTEM_OK:
        #ifdef SERIAL_PRINTING
          Serial.printf("SYSTEM IS OK\n");
        #endif
        break;
      case WIFI_DISCONNECTED:
        #ifdef SERIAL_PRINTING
          Serial.printf("SYSTEM IS NOT CONNECTED TO THE WIFI NETWORK\n");
        #endif
        ConnectToWiFi();
        break;
      case NTP_SERVER_DOWN:
        #ifdef SERIAL_PRINTING
          Serial.printf("SYSTEM COULD NOT GET NETWORK TIME\n");
        #endif
        GetNetworkTime();
        break;
      default:
        #ifdef SERIAL_PRINTING
				  Serial.printf("UNKNOWN ERROR\n");
				#endif
        Error_Handler();
        break;
    }
  }
}

void ExecuteState(void)
{
  #ifdef SERIAL_PRINTING
    // Serial.printf("\nSYSTEM IS OK\n");
  #endif
  switch(SYSTEM_STATE)
  {
    case POWER_ON:
      #ifdef SERIAL_PRINTING
				Serial.printf("SYSTEM IS UP FOR FIRST TIME\n");
			#endif
      PowerONRoutine();
      break;
    case STATUS_CHANGE:
      #ifdef SERIAL_PRINTING
        Serial.printf("PC STATUS POSSBIBLY CHANGED\n");
      #endif
      PCStatusChange();
      break;
    case LISTENING:
      #ifdef SERIAL_PRINTING
				// Serial.printf("SYSTEM AWAITS COMMAND\n");
			#endif
      ListeningRoutine();
      break;
    case EXECUTE_COMMAND:
      #ifdef SERIAL_PRINTING
				Serial.printf("SYSTEM WILL EXECUTE COMMAND\n");
			#endif
      ExecuteSystemCommand();
      break;
    case FIRMWARE_UPDATE:
      #ifdef SERIAL_PRINTING
        Serial.printf("SYSTEM WILL ENTER FUOTA MODE\n");
      #endif
      FirmwareUpdateMode();
      break;
    default:
      #ifdef SERIAL_PRINTING
			  Serial.printf("CONDITIONAL ERROR OCCURRED\n");
			#endif
      Error_Handler();
      break;
  }
}

void PowerONRoutine(void)
{
  TranscodeScroll("MAXIMUS-III", 250);
  PC_STATUS = CheckPCState();
  LAST_PC_STATUS = PC_STATUS;
  mx.clear();
  bot.sendMessage(CHAT_ID, Greeting + WiFi.localIP().toString(), "");
  if(PC_STATUS == PC_ON)
  {
    Serial.printf("PC is ON!\n\n");
    bot.sendMessage(CHAT_ID, "MAXIMUS-III is ON", "");
  }
  else
  {
    Serial.printf("PC is OFF!\n\n");
    bot.sendMessage(CHAT_ID, "MAXIMUS-III is OFF", "");
  }
  SYSTEM_STATE = LISTENING;
}

void PCStatusChange(void)
{
  PC_STATUS = CheckPCState();
  if(PC_STATUS != LAST_PC_STATUS)
  {
    if(PC_STATUS == PC_ON)
    {
      #ifdef SERIAL_PRINTING
        Serial.printf("PC is now ON\n");
      #endif
      bot.sendMessage(CHAT_ID, "MAXIMUS-III is Powered On!", "");
      print_data[NewMessageCounter] = "Power ON";
      NewMessageCounter++;
      PC_STATUS = PC_ON;
    }
    else
    {
      #ifdef SERIAL_PRINTING
        Serial.printf("PC is now OFF\n");
      #endif
      bot.sendMessage(CHAT_ID, "MAXIMUS-III is Powered Off!", "");
      print_data[NewMessageCounter] = "Power OFF";
      NewMessageCounter++;
      PC_STATUS = PC_OFF;
    }
    LAST_PC_STATUS = PC_STATUS;
  }
  SYSTEM_STATE = LISTENING;
}

void ListeningRoutine(void)
{
  DisplayTime();
  ListenForNewMessages();
}

void ExecuteSystemCommand(void)
{
  switch(SYSTEM_COMMAND)
  {
    case TURN_OFF:
      #ifdef SERIAL_PRINTING
        Serial.printf("Executing POWER OFF command\n");
      #endif
      bot.sendMessage(CHAT_ID, "Power switch pressed", "");
      print_data[NewMessageCounter] = "Power switch pressed";
      NewMessageCounter++;
      RelayAction(PR_Relay, 500);
      break;
    case TURN_ON:
      #ifdef SERIAL_PRINTING
        Serial.printf("Executing POWER ON command\n");
      #endif
      bot.sendMessage(CHAT_ID, "Power switch pressed", "");
      print_data[NewMessageCounter] = "Power switch pressed";
      NewMessageCounter++;
      RelayAction(PR_Relay, 500);
      break;
    case HARD_RESET:
      #ifdef SERIAL_PRINTING
        Serial.printf("Executing RESET command\n");
      #endif
      bot.sendMessage(CHAT_ID, "Reset switch pressed", "");
      print_data[NewMessageCounter] = "Reset switch pressed";
      NewMessageCounter++;
      RelayAction(RS_Relay, 500);
      break;
    case FORCE_SHUTDOWN:
      #ifdef SERIAL_PRINTING
        Serial.printf("Executing HARD POWER OFF command\n");
      #endif
      RelayAction(PR_Relay, 5000);
      break;
    default:
      break;
  }
  SYSTEM_STATE = LISTENING;
}

void FirmwareUpdateMode(void)
{
  static uint8_t FUOTA_STATUS = FUOTA_FAIL;
  static uint32_t timer = millis();
  if(FUOTA_STATUS == FUOTA_FAIL)
  {
    FUOTA_STATUS = SetupFirmwareUpdate();
  } 
  if(FUOTA_STATUS == FUOTA_SUCCESS)
  {
    while(SYSTEM_STATE == FIRMWARE_UPDATE)
    {
      if((timer < millis() - 5000) && (!printing))
      {
        mx.clear();
        printString("FUOTA");
        timer = millis();
      }
      server.handleClient();
      ListenForNewMessages();
      delay(1);
    }
  }
  server.stop();
  FUOTA_STATUS = FUOTA_FAIL;
}

uint8_t SetupFirmwareUpdate(void)
{
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) //http://esp32.local
  { 
    #ifdef SERIAL_PRINTING
      Serial.println("Error setting up MDNS responder!");
    #endif
    print_data[NewMessageCounter] = "Error setting up MDNS responder!";
    NewMessageCounter++;
    SYSTEM_STATE = LISTENING;
    return FUOTA_FAIL;
  }
  else
  {
    #ifdef SERIAL_PRINTING
      Serial.println("mDNS responder started");
    #endif
    detachInterrupt(PWR_SW);
    detachInterrupt(RST_SW);
    detachInterrupt(RST_State);
    /*return index page which is stored in serverIndex */
    server.on("/", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", loginIndex);
    });
    server.on("/serverIndex", HTTP_GET, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });
    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, []() 
    {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() 
    {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) 
      {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) //start with max available size
        { 
          Update.printError(Serial);
        }
      } 
      else if (upload.status == UPLOAD_FILE_WRITE) 
      {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) 
        {
          Update.printError(Serial);
        }
      } 
      else if (upload.status == UPLOAD_FILE_END) 
      {
        if (Update.end(true)) 
        { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } 
        else 
        {
          Update.printError(Serial);
        }
      }
    });
    server.begin();
    return FUOTA_SUCCESS;
  }
}

void RelayAction(uint8_t select, uint16_t duration)
{
  digitalWrite(select, LOW);
  if(duration) 
  {
    delay(duration);
  }
  digitalWrite(select, HIGH);
}

uint8_t CheckPCState(void)
{
  if(digitalRead(RST_State) == HIGH)
  {
    return PC_ON;
  }
  else
  {
    return PC_OFF;
  }
}

void PrintSystemStatus(void)
{
	char data[32] = "";
	sprintf(data, "ERROR CONTROLLER IS: %d\n", SYSTEM_STATUS);
	Serial.print(data);
}

void CheckWiFIConnection(void)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    SetErrorID(WIFI_DISCONNECTED);
  }
  else
  {
    ClearErrorID(WIFI_DISCONNECTED);
  }
}

void ConnectToWiFi(void)
{
  // Save last IP and send telegram message for new IP
  #ifdef SERIAL_PRINTING
    Serial.printf("Connecting to ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  #endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    #ifdef SERIAL_PRINTING
      Serial.printf(".");
    #endif
  }
  #ifdef SERIAL_PRINTING
    Serial.printf("\nWiFi connected\n");
    Serial.println(WiFi.localIP());
  #endif
  ClearErrorID(WIFI_DISCONNECTED);
  digitalWrite(LED_BUILTIN, HIGH);
}

void GetNetworkTime(void)
{
   // Init and get the time
  if(!getLocalTime(&timeinfo))
  {
    #ifdef SERIAL_PRINTING
      Serial.printf("Failed to obtain time\n");
    #endif
    SetErrorID(NTP_SERVER_DOWN);
    return;
  }
  else
  {
    #ifdef SERIAL_PRINTING
      Serial.printf("Network time loaded\n");
      Serial.print(&timeinfo, "%A, %B %d %Y - %H:%M:%S\n\n");
    #endif
    ClearErrorID(NTP_SERVER_DOWN);
  }
}

/**
  * @brief  Reads the SYSTEM_STATUS variable
  * @retval the first found error ID
  */
uint32_t CheckSystemError(void)
{
	for(uint8_t i = 0; i < 32; i++)
	{
		if(SYSTEM_STATUS&(1<<i))
		{
			return(1 << i);
		}
	}
	return 0;
}

/**
  * @brief  Checks the SYSTEM_STATUS for a specific ID
  * @retval TRUE if the error is found or FALSE if not found
  */
uint8_t CheckErrorID(uint32_t error)
{
	if((SYSTEM_STATUS&error) == error)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
  * @brief  Adds a specific Error ID to the SYSTEM_STATUS variable
  * @retval none
  */
void SetErrorID(uint32_t error)
{
	SYSTEM_STATUS = SYSTEM_STATUS | error;
}

/**
  * @brief  Removes a specific Error ID to the SYSTEM_STATUS variable
  * @retval none
  */
void ClearErrorID(uint32_t error)
{
	SYSTEM_STATUS &= ~error;
}

// =================== TIME FUNCTIONS ====================================
void DisplayTime(void) 
{
  static uint32_t checktime {0};
  char data[8];
  if ((checktime <= millis() - 5000) && (!printing)) 
  {
    #ifdef SERIAL_PRINTING
      Serial.printf("System will update its time\n");
    #endif
    GetNetworkTime();
    if(CheckErrorID(NTP_SERVER_DOWN) == FALSE)
    {
      sprintf(data, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      printString(data);
      checktime = millis();
    }
  }
}

// ================ TELEGRAM FUNCTIONS ===================================
void ListenForNewMessages(void)
{
  int botRequestDelay = 500;
  static uint32_t lastTimeBotRan;
  if (millis() > lastTimeBotRan + botRequestDelay)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) 
    {
      #ifdef SERIAL_PRINTING
        Serial.printf("New message arrived!\n");
      #endif
      HandleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void HandleNewMessages(int numNewMessages) 
{
  #ifdef SERIAL_PRINTING
    Serial.printf("Check for new messages\n");
    Serial.println(String(numNewMessages));
  #endif

  for (int i = 0; i < numNewMessages; i++) 
  {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID)
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    #ifdef SERIAL_PRINTING
      Serial.print(text + "\n\n");
    #endif
    String from_name = bot.messages[i].from_name;

    if (text == "/start") 
    {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/power_on to Power On the PC\n";
      welcome += "/power_off to Power Off the PC\n";
      welcome += "/reset to Reset the PC\n";
      welcome += "/force_power_off - to Force Power Off the PC";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/power_on") 
    {
      bot.sendMessage(chat_id, "Power ON command received", "");
      print_data[NewMessageCounter] = "Remote Power ON";
      NewMessageCounter++;
      RelayAction(PR_Relay, 500);
    }
    
    else if (text == "/power_off") 
    {
      bot.sendMessage(chat_id, "Power OFF command received", "");
      print_data[NewMessageCounter] = "Remote Power OFF";
      NewMessageCounter++;
      RelayAction(PR_Relay, 500);
    }
    
    else if (text == "/reset") 
    {
      bot.sendMessage(chat_id, "Reset command received", "");
      print_data[NewMessageCounter] = "Remote System Reset";
      NewMessageCounter++;
      RelayAction(RS_Relay, 500);
    }

    else if(text == "/force_power_off")
    {
      bot.sendMessage(chat_id, "Forced Shutdown command received", "");
      print_data[NewMessageCounter] = "Remote Forced Shutdown";
      NewMessageCounter++;
      RelayAction(PR_Relay, 5000);
    }

    else if(text == "/fuota")
    {
      bot.sendMessage(chat_id, "FUOTA mode command received", "");
      print_data[NewMessageCounter] = "FUOTA mode initiated";
      NewMessageCounter++;
      SYSTEM_STATE = FIRMWARE_UPDATE;
    }

    else if(text == "/exit_fuota")
    {
      bot.sendMessage(chat_id, "Exit FUOTA mode command received", "");
      print_data[NewMessageCounter] = "Exit FUOTA mode";
      EnableInterrupts();
      NewMessageCounter++;
      SYSTEM_STATE = LISTENING;
    }
    else
    {
      print_data[NewMessageCounter] = "Zissis says: " + text;
      NewMessageCounter++;
    }
  }
}


// ================= DISPLAY FUNCTIONS ===================================
void TranscodeScroll(String c, uint32_t  delay_ms) 
{
  uint16_t n = c.length(); 
  char char_array[n + 1]; 
  strcpy(char_array, c.c_str());
  scrollText(char_array);
  delay(delay_ms);
}

void delayAndClear(uint32_t delay_ms) 
{
  delay(delay_ms);
  mx.clear();
}

void scrollText(char *p) 
{
  uint8_t charWidth;
  uint8_t cBuf[8];  // Should be ok for all built-in fonts
  PRINTS("\nScrolling text");
  while (*p != '\0') {
    charWidth = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
    for (uint8_t i = 0; i <= charWidth; i++) // allow space between characters
    {
      mx.transform(MD_MAX72XX::TSL);
      if (i < charWidth) mx.setColumn(0, cBuf[i]);
      delay(DELAYTIME);
    }
  }
}

void printString(String s) 
{
  int n = s.length(); 
  char char_array[n + 1]; 
  strcpy(char_array, s.c_str());
  printText(0, MAX_DEVICES-1, char_array);
}

void printText(uint8_t modStart, uint8_t modEnd, char *pMsg) 
{
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;
  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  do {   // finite state machine to print the characters in the space available
    switch(state) 
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0') 
        {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }
        // retrieve the next character form the font file
        showLen = mx.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
        curLen = 0;
        state++;
        // !! deliberately fall through to next state to start displaying
      case 1: // display the next part of the character
        mx.setColumn(col--, cBuf[curLen++]);
        // done with font character, now display the space between chars
        if (curLen == showLen) 
        {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;
      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++; // fall through
      case 3:	// display inter-character spacing or end of message padding (blank columns)
        mx.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen) state = 0;
        break;
      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));
  mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void Core0(void *pvParameters) 
{
  // The while loop simulates the loop() function
  while(1) 
  {
    if(NewMessageCounter > 0)
    {
      printing = true;
      for(uint8_t i = 0; i < BUFFER_LENGTH; i++)
      {
        mx.clear();
        delay(10);
        TranscodeScroll(print_data[i], 250);
        print_data[i] = "";
        delay(250);
        if(print_data[i+1].length() == 0) break;
      }
      printing = false;
      NewMessageCounter = 0;
    }
    delay(1); // delay so that the core won't panic
  }
}

void EnableInterrupts(void)
{
  attachInterrupt(digitalPinToInterrupt(PWR_SW), Power, HIGH);
  attachInterrupt(digitalPinToInterrupt(RST_SW), Reset, HIGH);
  attachInterrupt(digitalPinToInterrupt(RST_State), Status, CHANGE);
}

void IRAM_ATTR Power(void) // Function to Power On or Off the system
{ 
  if(CheckPCState() == PC_ON)
  {
    SYSTEM_COMMAND = TURN_OFF;
  }
  else
  {
    SYSTEM_COMMAND = TURN_ON;
  }
  SYSTEM_STATE = EXECUTE_COMMAND;
} 

void IRAM_ATTR Reset(void) // Function to Restart the system
{ 
  SYSTEM_COMMAND = HARD_RESET;
  SYSTEM_STATE = EXECUTE_COMMAND;
}

void IRAM_ATTR Status(void) // Function that checks system state
{
  SYSTEM_STATE = STATUS_CHANGE;
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  #ifdef SERIAL_PRINTING
    Serial.printf("\nSYSTEM IS GOING DOWN FOR REBOOT\n");
  #endif
  ESP.restart();
}