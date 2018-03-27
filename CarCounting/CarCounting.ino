#include <DistanceGP2Y0A21YK.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <QueueList.h>
#include <TimeLib.h>
#include "SdFat.h"

//#define _DEBUG_
#define _SD_

// General use definitions and declarations
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  "NEED_TIME"    // ASCII bell character requests a time sync message 


/*-------------------------------------------------------------------------*/
// Variable declaration and definition

DistanceGP2Y0A21YK Dist0;
DistanceGP2Y0A21YK Dist1;
SoftwareSerial BTSerial(7, 8);  // SET PINS APPROPRIATELY
SdFat SD;
SdFile saveFile;
SdFile idFile;
SdFile root;

// Device specific information
String DeviceID;
String DeviceLoc;
String DeviceDir;
String DevComment;

// Timer
elapsedMillis timer0;
elapsedMillis timerDelta0;
bool timer0up;
bool timerDelta0up;
String nowT;
String state;

// Regarding the sensor
int firstSensor;
int distance0;
int distance1;
int lastDist;
int avgDist;
int numDist;
int tDelta;

// Regarding BLE/Serial
String msgSer;
bool BLEOn;

// Regarding the SD card
QueueList<String> queue;
String msgData;
String msgSD;
#ifdef _SD_
  String filename;
#endif

// SET PINS APPROPRIATELY
int sensorPin0 = A1;
int sensorPin1 = A3;
int powLedPin = 2;
int errLedPin = 5;
#ifdef _SD_
  const int chipSelect = 10;
  const int cardDetect = 6;
#endif

/*-------------------------------------------------------------------------*/
// Helper and time functions

// Serial printer
void serPrint(String str) {
  (BLEOn) ? BTSerial.println(str) : Serial.println(str);
}

String retState() {
  String retState = state;
  retState += " ID=";
  retState += DeviceID;
  retState += " LOC=";
  retState += DeviceLoc;
  retState += " DIR=";
  retState += DeviceDir;
  retState += " COMMENT=";
  retState += DevComment;
  return retState;
}

// Return state
void returnState() {
  if(Serial.find("?")) {
     Serial.println(retState());
     BLEOn = false;
  } else if(BTSerial.find("?")) {
     BTSerial.println(retState());
     BLEOn = true;
  }
}

// Error function
void errState(String err) {
  state = err;
  while(true) {
    returnState();
    digitalWrite(errLedPin, !digitalRead(errLedPin));
    delay(1000); 
  }
}

// Serial sync
void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1518739200; // Feb 16 2018

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // Check that it's a valid time greater than default
       setTime(pctime); // Sync Arduino clock
     }
     Serial.println("TIME_ACK");
     BLEOn = false;
  } else if(BTSerial.find(TIME_HEADER)) {
     pctime = BTSerial.parseInt();
     if( pctime >= DEFAULT_TIME) { // Check that it's a valid time greater than default
       setTime(pctime); // Sync Arduino clock
     }
     BTSerial.println("TIME_ACK");
     BLEOn = true;
  } else if(Serial.find("?")) {
     Serial.println(TIME_REQUEST);
     BLEOn = false;
  } else if(BTSerial.find("?")) {
     BTSerial.println(TIME_REQUEST);
     BLEOn = true;
  }
  
}

// Send time request symbol (BEL, ASCII 7)
time_t requestSync()
{
  Serial.println(TIME_REQUEST);
  BTSerial.println(TIME_REQUEST);
  return 0;
}

/*-------------------------------------------------------------------------*/
// SD functions

#ifdef _SD_

  // Function to start up SD card
  void SDInit() {
    bool complete = SD.begin();
    delay(100);   // Needs time to start up before checking that it completed
    if (!complete) {
      errState("SD_INIT_ERROR");
    }
    #ifdef _DEBUG_
      Serial.println(">>SD Initialization Done.");
    #endif
  }

  // Save system variables to file if not saved
  void SDVarInit(bool SDDevVarsSaved) {
    if (!SDDevVarsSaved) {
      char ID_FileName[] = "Device_Information.txt";
      if (!SD.exists(ID_FileName)) {
        bool success = idFile.open(ID_FileName, O_WRITE | O_CREAT);
        if (success) {
          idFile.println(DeviceID);
          idFile.println(DeviceLoc);
          idFile.println(DeviceDir);
          idFile.println(DevComment);
          idFile.close();
          #ifdef _DEBUG_
            Serial.println(">>Saved device variables to file successfully.");
          #endif
        } else {
          idFile.close();
          errState("SD_VAR_ERROR");
        }
      }
    }
  }

  // Initialize data file to write to
  void SDFileInit() {
    filename = String(int(now())); 
    filename += ".csv";

    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile.open(temp, O_WRITE | O_CREAT);
    if (!saveFile.isOpen()) {
        #ifdef _DEBUG_
          Serial.println(">>Error opening SD data file: "+filename);
        #endif
        saveFile.close();
        errState("SD_FILE_ERROR");
    } else {
        #ifdef _DEBUG_
          Serial.println(">>Opened data file successfully.");
        #endif
    }
  }

  // Swap data files when one gets too big
  void swapFiles() {
    saveFile.close();
    filename = String(int(now())); 
    filename += ".csv";
    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile.open(temp, O_WRITE | O_CREAT);
    if (saveFile.isOpen()) {
        saveFile.close();
        errState("SD_SWAP_ERROR");
    } else {
        #ifdef _DEBUG_
          Serial.println(">>Swapped data file successfully.");
        #endif
    }
  }

  // Wipe the SD card and remove all files
  void rmFiles() {
    if(BLEOn) {
      if (!SD.wipe(&BTSerial)) {
        SD.errorHalt("Wipe failed.");
        errState("SD_RESET_ERROR");
      }
      if (!SD.begin()) {
        SD.errorHalt("Second init after reset failed.");
        errState("SD_REINIT_ERROR");
      }
      BTSerial.println("RESET_COMPLETE");
    } else {
      if (!SD.wipe(&Serial)) {
        SD.errorHalt("Wipe failed.");
        errState("SD_RESET_ERROR");
      }
      if (!SD.begin()) {
        SD.errorHalt("Second init after reset failed.");
        errState("SD_REINIT_ERROR");
      }
      Serial.println("RESET_COMPLETE");
    }
  }

  // Return the filename and contents of all files on the SD card
  void dumpFiles(SdFile dir) {
    SdFile entry;
    while (entry.openNext(SD.vwd(), O_READ)) {
      if (entry.isDir()) {
        dumpFiles(entry);
      } else {
        if (BLEOn) {
          BTSerial.print("BEGIN_FILE=");
          entry.printName(&BTSerial);
          BTSerial.print('\n');
          while (entry.available()) {
            BTSerial.write(entry.read());
          }
          BTSerial.println("END_FILE");
        } else {
          Serial.print("BEGIN_FILE=");
          entry.printName(&Serial);
          Serial.print('\n');
          while (entry.available()) {
            Serial.write(entry.read());
          }
          Serial.println("END_FILE");        
        }
        entry.close();
      }
    }
  }
  
#endif

/*-------------------------------------------------------------------------*/
// Communication and control helper functions

// Receive commands to set system variables
void setParamsCommands() {
  if (Serial.available() > 0) {
    msgSer = Serial.readString();
    BLEOn = false;
  }
  if (BTSerial.available() > 0) {
    msgSer = BTSerial.readString();
    BLEOn = true;
  }

  if (msgSer.indexOf("ID=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 3);
    DeviceID = msgSer.trim();
    serPrint("ID_ACK");
  }
  else if (msgSer.indexOf("LOC=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceLoc = msgSer.trim();
    serPrint("LOC_ACK");
  }
  else if (msgSer.indexOf("DIR=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceDir = msgSer.trim();
    serPrint("DIR_ACK");
  }
  else if (msgSer.indexOf("COMMENT=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 8);
    DevComment = msgSer.trim();
    serPrint("COMMENT_ACK");
  } else if (msgSer.indexOf("?") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    serPrint(retState());
  } else if (msgSer.indexOf("T") == 0) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    serPrint("TIME_SYNCED");
  }
  
  msgSer = "";
}

// Receive commands to control state and workflow
void runCommands() {
  if (Serial.available() > 0) {
    msgSer = Serial.readString();
    BLEOn = false;
  }
  if (BTSerial.available() > 0) {
    msgSer = BTSerial.readString();
    BLEOn = true;
  }

  if (msgSer.indexOf("START_RUNNING") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
      if (!saveFile.isOpen()) {
        SDFileInit();
      }
    #endif
    state = "RUNNING";
    serPrint("START_ACK");
  } else if (msgSer.indexOf("STOP_RUNNING") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
    if (saveFile.isOpen()) {
      saveFile.close();
      #ifdef _DEBUG_
        Serial.println(">>Closed save file.");
      #endif
    }
    #endif
    state = "READY";
    serPrint("STOP_ACK");
  } else if (msgSer.indexOf("RETURN_DATA") > -1) {
    if (state == "RUNNING") {
      serPrint("RETURN_ACK");
      serPrint("NOT_STOPPED");
    } else {
      #ifdef _DEBUG_
        Serial.print(">>Received command: ");
        Serial.println(msgSer);
      #endif
      serPrint("RETURN_ACK");
      #ifdef _SD_
        if (BLEOn) {
          BTSerial.println("BEGIN_TRANSFER");
        } else {
          Serial.println("BEGIN_TRANSFER");
        }
        root.open("/");
        SD.vwd()->rewind();
        dumpFiles(root);
        if (BLEOn) {
          BTSerial.println("END_TRANSFER");
        } else {
          Serial.println("END_TRANSFER");
        }
      #endif
    }
  } else if (msgSer.indexOf("SET_VARS") > -1) {
    if (state == "RUNNING") {
      serPrint("SET_ACK");
      serPrint("NOT_STOPPED");
    } else {
      #ifdef _DEBUG_
        Serial.print(">>Received command: ");
        Serial.println(msgSer);
      #endif
      serPrint("SET_ACK");
      DeviceID = DeviceLoc = DeviceDir = DevComment = "";
      state = "NEED_VARS";
      
      while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
        if (Serial.available() || BTSerial.available()) {
          setParamsCommands();
        }
      }
      
      #ifdef _SD_
        SDVarInit(false);
        SDFileInit();
      #endif

      state = "READY";
    } 
  } else if (msgSer.indexOf("RESET_DEVICE") > -1) {
    if (state == "RUNNING") {
      serPrint("RESET_ACK");
      serPrint("NOT_STOPPED");
    } else {
      #ifdef _DEBUG_
        Serial.print(">>Received command: ");
        Serial.println(msgSer);
      #endif
      serPrint("RESET_ACK");
      #ifdef _SD_
        rmFiles();
      #endif
      
      DeviceID = DeviceLoc = DeviceDir = DevComment = "";
      state = "NEED_VARS";
      
      while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
        if (Serial.available() || BTSerial.available()) {
          setParamsCommands();
        }
      }
    
      #ifdef _DEBUG_
        Serial.println(">>Parameters set.");
      #endif
      
      #ifdef _SD_
        SDVarInit(false);
        SDFileInit();
      #endif

      state = "READY";
    }
  } else if (msgSer.indexOf("?") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    serPrint(retState());
  } else if (msgSer.indexOf("T") == 0) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    serPrint("TIME_SYNCED");
  }
  
  msgSer = "";
}

/*-------------------------------------------------------------------------*/
// Main setup

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Dist0.begin(sensorPin0);
  Dist0.setAveraging(10);
  Dist1.begin(sensorPin1);
  Dist0.setAveraging(10);
  pinMode(powLedPin, OUTPUT);
  pinMode(errLedPin, OUTPUT);
  digitalWrite(powLedPin, HIGH);
  
  // Set function to call when sync required
  setSyncProvider(requestSync);
  while (timeStatus() == timeNotSet) {
    if (Serial.available() || BTSerial.available()) {
      processSyncMessage();
    }
  }
  timer0up = false;
  timerDelta0up = false;
  timer0 = 0;
  timerDelta0 = 0;
  firstSensor = -1;

  digitalWrite(errLedPin, HIGH);
  
  // Initialize SD card and check for device variables
  #ifdef _SD_
    // Start up SD card
    SDInit();
 
    bool SDDevVarsSaved = false;
  
    // Check for device variables on disk
    char line[50];
    int n;
    int var = 0;
    char ID_FileName[] = "Device_Information.txt";
    if (SD.exists(ID_FileName)) {
      bool success = idFile.open(ID_FileName, O_READ);
      if (success) {
        while ((n = idFile.fgets(line, sizeof(line))) > 0) {
          if (line[n-1] == '\n') {
            switch(var) {
              case 0:
                DeviceID = String(line).trim();
                var++;
                break;
              case 1:
                DeviceLoc = String(line).trim();
                var++;
                break;
              case 2:
                DeviceDir = String(line).trim();
                var++;
                break;
              case 3:
                DevComment = String(line).trim();
                var++;
                break;
            }
          }
        }
        idFile.close();
        SDDevVarsSaved = true;
        #ifdef _DEBUG_
          Serial.println(">>Successfully restored device variables.");
        #endif
      } else {
        #ifdef _DEBUG_
          Serial.println(">>Error opening SD ID file, will ask for new variables.");
        #endif
        idFile.close();
      }
    }
  #endif

  digitalWrite(errLedPin, LOW);

  if (!SDDevVarsSaved) {
    DeviceID = "";
    DeviceLoc = "";
    DeviceDir = "";
    DevComment = "";
    state = "NEED_VARS";
  }

  // Set device variables
  while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
    if (Serial.available() || BTSerial.available()) {
      setParamsCommands();
    }
  }

  digitalWrite(errLedPin, HIGH);
  
  #ifdef _DEBUG_
    Serial.println(">>Parameters set.");
  #endif

  // Initialize SD files
  #ifdef _SD_
    SDVarInit(SDDevVarsSaved);
    SDFileInit();
  #endif
  
  #ifdef _DEBUG_
    Serial.println("\n>>Time synchronized.");
  #endif
  
  digitalWrite(errLedPin, LOW);
  
  state = "READY";
}


/*-------------------------------------------------------------------------*/
// Main loop

void loop() {
  // Check if we received command to start running
  if (state == "RUNNING") {    
    distance0 = Dist0.getDistanceCentimeter();
    distance1 = Dist1.getDistanceCentimeter();
    distance0 += 10;
    distance1 += 10;
    #ifdef _DEBUG_
      Serial.print("Distance read: ");
      Serial.println(String(distance0));
      Serial.println(String(distance1));
    #endif
  
    // Valid distances are 10 cm to 40 cm
    if (distance0 > 10 && distance0 < 35 && firstSensor != 1) {
      // Are we already counting time on first sensor?
      if (timer0up == false) {
        firstSensor = 0;
        timer0up = true;
        timer0 = 0;
        timerDelta0up = true;
        timerDelta0 = 0;
        tDelta = -1;
        nowT = String(int(now()));
        msgData = DeviceID;
        msgData += ", ";
        msgData += nowT;
        msgData += ", ";
        msgData += firstSensor;
        msgData += ", ";
        avgDist = distance0;
        numDist = 1;
      } else {
        if (distance1 > 10 && distance1 < 35 && timerDelta0up == true) {
          tDelta = timerDelta0;
          timerDelta0up = false;
        }
        lastDist = distance0;
        numDist += 1;
        // Collect every 10 samples
        if (numDist % 10 == 0) {
          avgDist += lastDist;
        }
      }
    } else if (distance1 > 10 && distance1 < 40 && firstSensor != 0) {
      // Are we already counting time on second sensor?
      if (timer0up == false) {
        firstSensor = 1;
        timer0up = true;
        timer0 = 0;
        timerDelta0up = true;
        timerDelta0 = 0;
        tDelta = -1;
        nowT = String(int(now()));
        msgData = DeviceID;
        msgData += ", ";
        msgData += nowT;
        msgData += ", ";
        msgData += firstSensor;
        msgData += ", ";
        avgDist = distance1;
        numDist = 1;
      } else {
        if (distance0 > 10 && distance0 < 40 && timerDelta0up == true) {
          tDelta = timerDelta0;
          timerDelta0up = false;
        }
        lastDist = distance1;
        numDist += 1;
        // Collect every 10 samples
        if (numDist % 10 == 0) {
          avgDist += lastDist;
        }
      }
    } else if ((firstSensor == 0 && distance1 > 10 && distance1 < 40) || (firstSensor == 1 && distance0 > 10 && distance0 < 40)) {
      #ifdef _DEBUG_
        Serial.println("Waiting for car to pass");
      #endif
    } else {
      if (timer0up == true) {
        // Was the length of time valid (more than 100 ms)? Was there a delta?
        if (timer0 > 100 && tDelta > 0) {
          msgData += timer0;
          msgData += ", ";
          msgData += tDelta;
          msgData += ", ";
          avgDist = (avgDist + lastDist) / (numDist / 10 + 1);
          msgData += avgDist;
          queue.push(msgData);
          }
          timer0up = false;
          firstSensor = -1;
      } else {
        #ifdef _SD_
          // Not collecting data, take the opportunity to write some to SD
          if (!queue.isEmpty()) {
            if (saveFile.isOpen()) {
              msgSD = queue.pop();
              #ifdef _DEBUG_
                Serial.println(msgSD);
              #endif
              saveFile.println(msgSD);
  
              // Check if the file is greater than 1 MB
              if (saveFile.fileSize() > 1048576) {
                swapFiles();
              }
            } else {
              // If the file didn't open, print an error
              #ifdef _DEBUG_
                Serial.println(">>No save file to write to!");
              #endif
              errState("SD_WRITE_ERROR");
            }
          } else {
            // Now it's safe to check to stop running
            runCommands();
          }
        #endif
        
        #ifndef _SD_
          #ifdef _DEBUG_
            Serial.println(msgSD);
          #endif
        #endif
      }
    }
  } else {
    // Check if we received command
    runCommands();
  }
}


