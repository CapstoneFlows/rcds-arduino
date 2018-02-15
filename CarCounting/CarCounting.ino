#include <DistanceGP2Y0A21YK.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <QueueList.h>
#include <TimeLib.h>
#include "SdFat.h"

#define _DEBUG_
#define _SD_

// General use definitions and declarations
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 


/*-------------------------------------------------------------------------*/
// Variable declaration and definition

DistanceGP2Y0A21YK Dist;
SoftwareSerial BTSerial(7, 8);  // SET PINS APPROPRIATELY
SdFat SD;
File saveFile;
File idFile;
File root;

// Device specific information
String DeviceID;
String DeviceLoc;
String DeviceDir;
String DevComment;

// Timer
elapsedMillis timer0;
bool timer0up;
bool active_toggle;

// Regarding the sensor
int distance;
int lastDist;
int avgDist;
int numDist;

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
int sensorPin = A1;
#ifdef _SD_
  const int chipSelect = 10;
  const int cardDetect = 6;
#endif

/*-------------------------------------------------------------------------*/
// Time and SD helper functions

// Serial sync
void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  } else if(BTSerial.find(TIME_HEADER)) {
     pctime = BTSerial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}

time_t requestSync()
{
  Serial.write(TIME_REQUEST);
  BTSerial.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}

#ifdef _SD_
  // SD functions
  void SDInit() {
    bool complete = SD.begin();
    delay(100);
    if (!complete) {
      #ifdef _DEBUG_
        Serial.println("SD Initialization Failed!");
      #endif
      return;
    }
    #ifdef _DEBUG_
      Serial.println("SD Initialization Done.");
    #endif
  
    // SAVE DEVICE VARIABLES
    String ID_FileName = "Device_Information.txt";
    char tempID[ID_FileName.length()+1];
    ID_FileName.toCharArray(tempID, sizeof(tempID));
    if (!SD.exists(tempID)) {
      idFile = SD.open(tempID, FILE_WRITE);
      if (idFile) {
      idFile.println(DeviceID);
      idFile.println(DeviceLoc);
      idFile.println(DeviceDir);
      idFile.println(DevComment);
      idFile.close();
      } else {
        #ifdef _DEBUG_
          Serial.println("Error opening SD ID file");
        #endif
        idFile.close();
      }
    }
  
    filename = DeviceID;
    filename += "_";
    filename += DeviceLoc;
    filename += "_";
    filename += DeviceDir;
    filename += "_";
    filename += String(int(now())); 
    filename += ".txt";

    #ifdef _DEBUG_
      Serial.println(filename);
    #endif

    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile = SD.open(temp, FILE_WRITE);
    if (!SD.exists(temp)) {
        #ifdef _DEBUG_
          Serial.println("Error opening SD data file");
        #endif
        saveFile.close();
    }
  }
  
  void swapFiles() {
    saveFile.close();
    filename = DeviceID;
    filename += "_";
    filename = DeviceLoc;
    filename += "_";
    filename = DeviceDir;
    filename += "_";
    filename += String(int(now())); 
    filename += ".txt";
    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile = SD.open(temp, FILE_WRITE);
  }
#endif

/*-------------------------------------------------------------------------*/
// Communication and control helper functions

// BLE connection
void setParamsCommands() {
  if (Serial.available() > 0) {
    msgSer = Serial.readString();
  }
  if (BTSerial.available() > 0) {
    msgSer = BTSerial.readString();
  }

  if (msgSer.indexOf("ID=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 3);
    DeviceID = msgSer.trim();
  }
  else if (msgSer.indexOf("LOC=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceLoc = msgSer.trim();
  }
  else if (msgSer.indexOf("DIR=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceDir = msgSer.trim();
  }
  else if (msgSer.indexOf("COMMENT=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 8);
    DevComment = msgSer.trim();
  }
  msgSer = "";
}

#ifdef _SD_
void rmFiles(File dir) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    if (entry.isDirectory()) {
      rmFiles(entry);
    } else {
      char temp[strlen(entry.name())+1];
      strncpy(temp, entry.name(), strlen(temp));
      entry.close();
      SD.remove(temp);
    }
    entry.close();
  }
}

void dumpFiles(File dir, bool BLEOn) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }
    if (entry.isDirectory()) {
      dumpFiles(entry, BLEOn);
    } else {
      String temp = "BEGIN_FILE="+String(entry.name());
      if (BLEOn) {
        BTSerial.println(temp);
        while (entry.available()) {
          BTSerial.write(entry.read());
        }
        BTSerial.println("END_FILE");
      } else {
        Serial.println(temp);
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
    active_toggle = true;
  } else if (msgSer.indexOf("STOP_RUNNING") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
    if (saveFile) {
      saveFile.close();
      #ifdef _DEBUG_
        Serial.println("Closed save file.");
      #endif
    }
    #endif
    active_toggle = false;
  } else if (msgSer.indexOf("RETURN_DATA") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
      if (BLEOn) {
        BTSerial.println("BEGIN_TRANSFER");
      } else {
        Serial.println("BEGIN_TRANSFER");
      }
      root = SD.open("/");
      dumpFiles(root, BLEOn);
      if (BLEOn) {
        BTSerial.println("END_TRANSFER");
      } else {
        Serial.println("END_TRANSFER");
      }
    #endif
  } else if (msgSer.indexOf("RESET_DEVICE") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
      root = SD.open("/");
      rmFiles(root);
    #endif
    
    DeviceID = DeviceLoc = DeviceDir = DevComment = "";
  }
  msgSer = "";
}


/*-------------------------------------------------------------------------*/
// Main setup

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Dist.begin(sensorPin);

  setSyncProvider(requestSync);  // Set function to call when sync required
  while (timeStatus() == timeNotSet) {
    requestSync();
    if (Serial.available() || BTSerial.available()) {
      processSyncMessage();
    }
  }
  timer0up = false;
  timer0 = 0;
  
  DeviceID = "";
  DeviceLoc = "";
  DeviceDir = "";
  DevComment = "";

  #ifdef _DEBUG_
    Serial.println("Time synchronized.");
  #endif
  
  #ifdef _SD_
    // CHECK FOR DEVICE VARIABLES ON DISK
    String ID_FileName = "Device_Information.txt";
    char tempID[ID_FileName.length()+1];
    ID_FileName.toCharArray(tempID, sizeof(tempID));
    if (SD.exists(tempID)) {
      idFile = SD.open(tempID, FILE_READ);
      if (idFile) {
        DeviceID = idFile.readStringUntil('\n');
        DeviceLoc = idFile.readStringUntil('\n');
        DeviceDir = idFile.readStringUntil('\n');
        DevComment = idFile.readStringUntil('\n');
        idFile.close();
      } else {
        #ifdef _DEBUG_
          Serial.println("Error opening SD ID file");
        #endif
        idFile.close();
      }
    }
  #endif

  while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
    if (Serial.available() || BTSerial.available()) {
      setParamsCommands();
    }
  }

  #ifdef _DEBUG_
    Serial.println("Parameters set.");
  #endif
  
  #ifdef _SD_
    SDInit();
  #endif
}


/*-------------------------------------------------------------------------*/
// Main loop

void loop() {
  // Check if we received command to start running
  if (active_toggle) {
    distance = Dist.getDistanceCentimeter();
    distance += 10;
    //Serial.println(String(distance));
  
    // Valid distances are 10 cm to 40 cm
    if (distance > 10) {
      if (distance < 40) {
        // Are we already counting time?
        if (timer0up == false) {
          timer0up = true;
          timer0 = 0;
          msgData = String(int(now()));
          msgData += ", ";
          avgDist = distance;
          numDist = 1;
        } else {
          lastDist = distance;
          numDist += 1;
          // Collect every 10 samples
          if (numDist % 10 == 0) {
            avgDist += lastDist;
          }
        }
      }
    } else {
      if (timer0up == true) {
        // Was the length of time valid (more than 100 ms)?
        if (timer0 > 100) {
          msgData += timer0;
          msgData += ", ";
          avgDist = (avgDist + lastDist) / (numDist / 10 + 1);
          msgData += avgDist;
          queue.push(msgData);
          }
          timer0up = false; 
      } else {
        // Take the opportunity to write to SD
        if (!queue.isEmpty()) {
          if (saveFile) {
            msgSD = queue.pop();
            #ifdef _DEBUG_
              Serial.println(msgSD);
            #endif
            saveFile.println(msgSD);

            #ifdef _SD_
              // Check if the file is greater than 1 MB
              if (saveFile.size() > 1048576) {
                swapFiles();
              }
            #endif
          } else {
            // If the file didn't open, print an error
            #ifdef _DEBUG_
              Serial.println("No save file to write to!");
            #endif
          }
        } else {
          // Now it's safe to check to stop running
          runCommands();
        }
      }
    }
  } else {
    // Check if we received command
    runCommands();
  }
}


