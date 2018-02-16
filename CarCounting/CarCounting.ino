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
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 


/*-------------------------------------------------------------------------*/
// Variable declaration and definition

DistanceGP2Y0A21YK Dist;
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
     Serial.println("\n");
     Serial.println("TIME_ACK");
     BLEOn = false;
  } else if(BTSerial.find(TIME_HEADER)) {
     pctime = BTSerial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
     BTSerial.println("\n");
     BTSerial.println("TIME_ACK");
     BLEOn = true;
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
      (BLEOn) ? BTSerial.println("SD_INIT_ERROR") : Serial.println("SD_INIT_ERROR");
      exit(-1);
    }
    #ifdef _DEBUG_
      Serial.println("SD Initialization Done.");
    #endif
  }

  void SDVarInit(bool SDDevVarsSaved) {
    if (!SDDevVarsSaved) {
      // Save device variables if not saved
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
            Serial.println("Saved device variables to file successfully.");
          #endif
        } else {
          #ifdef _DEBUG_
            Serial.println("Error opening SD ID file");
          #endif
          idFile.close();
          (BLEOn) ? BTSerial.println("SD_VAR_ERROR") : Serial.println("SD_VAR_ERROR");
          exit(-1);
        }
      }
    }
  }

  void SDFileInit() {
    filename = String(int(now())); 
    filename += ".csv";

    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile.open(temp, O_WRITE | O_CREAT);
    if (!saveFile.isOpen()) {
        #ifdef _DEBUG_
          Serial.println("Error opening SD data file: "+filename);
        #endif
        saveFile.close();
        (BLEOn) ? BTSerial.println("SD_FILE_ERROR") : Serial.println("SD_FILE_ERROR");
        exit(-1);
    } else {
        #ifdef _DEBUG_
          Serial.println("Opened data file successfully.");
        #endif
    }
  }
  
  void swapFiles() {
    saveFile.close();
    filename = String(int(now())); 
    filename += ".csv";
    char temp[filename.length()+1];
    filename.toCharArray(temp, sizeof(temp));
    saveFile.open(temp, O_WRITE | O_CREAT);
    if (saveFile.isOpen()) {
        #ifdef _DEBUG_
          Serial.println("Error swapping SD data file");
        #endif
        (BLEOn) ? BTSerial.println("SD_SWAP_ERROR") : Serial.println("SD_SWAP_ERROR");
        saveFile.close();
        active_toggle = false;
    } else {
        #ifdef _DEBUG_
          Serial.println("Swapped data file successfully.");
        #endif
    }
  }
#endif

/*-------------------------------------------------------------------------*/
// Communication and control helper functions

// BLE connection
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
    (BLEOn) ? BTSerial.println("ID_ACK") : Serial.println("ID_ACK");
  }
  else if (msgSer.indexOf("LOC=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceLoc = msgSer.trim();
    (BLEOn) ? BTSerial.println("LOC_ACK") : Serial.println("LOC_ACK");
  }
  else if (msgSer.indexOf("DIR=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 4);
    DeviceDir = msgSer.trim();
    (BLEOn) ? BTSerial.println("DIR_ACK") : Serial.println("DIR_ACK");
  }
  else if (msgSer.indexOf("COMMENT=") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif

    msgSer.remove(0, 8);
    DevComment = msgSer.trim();
    (BLEOn) ? BTSerial.println("COMMENT_ACK") : Serial.println("COMMENT_ACK");
  }
  msgSer = "";
}

#ifdef _SD_
void rmFiles() {
  if(BLEOn) {
    if (!SD.wipe(&BTSerial)) {
      SD.errorHalt("Wipe failed.");
      BTSerial.println("SD_RESET_ERROR");
    }
    if (!SD.begin()) {
      SD.errorHalt("Second init after reset failed.");
      BTSerial.println("SD_REINIT_ERROR");
    }
    BTSerial.println("RESET_COMPLETE");
  } else {
    if (!SD.wipe(&Serial)) {
      SD.errorHalt("Wipe failed.");
      Serial.println("SD_RESET_ERROR");
    }
    if (!SD.begin()) {
      SD.errorHalt("Second init after reset failed.");
      Serial.println("SD_REINIT_ERROR");
    }
    Serial.println("RESET_COMPLETE");
  }
}

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
    active_toggle = true;
    (BLEOn) ? BTSerial.println("START_ACK") : Serial.println("START_ACK");
  } else if (msgSer.indexOf("STOP_RUNNING") > -1) {
    #ifdef _DEBUG_
      Serial.print(">>Received command: ");
      Serial.println(msgSer);
    #endif
    #ifdef _SD_
    if (saveFile.isOpen()) {
      saveFile.close();
      #ifdef _DEBUG_
        Serial.println("Closed save file.");
      #endif
    }
    #endif
    active_toggle = false;
    (BLEOn) ? BTSerial.println("STOP_ACK") : Serial.println("STOP_ACK");
  } else if (msgSer.indexOf("RETURN_DATA") > -1) {
    if (active_toggle) {
      (BLEOn) ? BTSerial.println("RETURN_ACK") : Serial.println("RETURN_ACK");
      (BLEOn) ? BTSerial.println("NOT_STOPPED") : Serial.println("NOT_STOPPED");
    } else {
      #ifdef _DEBUG_
        Serial.print(">>Received command: ");
        Serial.println(msgSer);
      #endif
      (BLEOn) ? BTSerial.println("RETURN_ACK") : Serial.println("RETURN_ACK");
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
  } else if (msgSer.indexOf("RESET_DEVICE") > -1) {
    if (active_toggle) {
      (BLEOn) ? BTSerial.println("RESET_ACK") : Serial.println("RESET_ACK");
      (BLEOn) ? BTSerial.println("NOT_STOPPED") : Serial.println("NOT_STOPPED");
    } else {
      #ifdef _DEBUG_
        Serial.print(">>Received command: ");
        Serial.println(msgSer);
      #endif
      (BLEOn) ? BTSerial.println("RESET_ACK") : Serial.println("RESET_ACK");
      #ifdef _SD_
        rmFiles();
        #ifdef _DEBUG_
          Serial.print("Reset device successfully.");
        #endif
      #endif
      
      DeviceID = DeviceLoc = DeviceDir = DevComment = "";
      (BLEOn) ? BTSerial.println("NEED_VARS") : Serial.println("NEED_VARS");
      
      while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
        if (Serial.available() || BTSerial.available()) {
          setParamsCommands();
        }
      }
    
      #ifdef _DEBUG_
        Serial.println("Parameters set.");
      #endif
      
      #ifdef _SD_
        SDVarInit(false);
        SDFileInit();
      #endif
      
      (BLEOn) ? BTSerial.println("READY") : Serial.println("READY");
    }
  }
  msgSer = "";
}


/*-------------------------------------------------------------------------*/
// Main setup

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Dist.begin(sensorPin);
  
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
          Serial.println("Successfully restored device variables.");
        #endif
      } else {
        #ifdef _DEBUG_
          Serial.println("Error opening SD ID file, will ask for new variables.");
        #endif
        idFile.close();
      }
    }
  #endif

  setSyncProvider(requestSync);  // Set function to call when sync required
  while (timeStatus() == timeNotSet) {
    requestSync();
    if (Serial.available() || BTSerial.available()) {
      processSyncMessage();
    }
  }
  timer0up = false;
  timer0 = 0;
  
  #ifdef _DEBUG_
    Serial.println("\nTime synchronized.");
  #endif

  if (!SDDevVarsSaved) {
    DeviceID = "";
    DeviceLoc = "";
    DeviceDir = "";
    DevComment = "";
    (BLEOn) ? BTSerial.println("NEED_VARS") : Serial.println("NEED_VARS");
  }

  while (DeviceID == "" || DeviceLoc == "" || DeviceDir == "" || DevComment == "") {
    if (Serial.available() || BTSerial.available()) {
      setParamsCommands();
    }
  }

  #ifdef _DEBUG_
    Serial.println("Parameters set.");
  #endif
  
  #ifdef _SD_
    SDVarInit(SDDevVarsSaved);
    SDFileInit();
  #endif

  (BLEOn) ? BTSerial.println("READY") : Serial.println("READY");
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
    if (distance > 10 && distance < 40) {
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
          if (saveFile.isOpen()) {
            msgSD = queue.pop();
            #ifdef _DEBUG_
              Serial.println(msgSD);
            #endif
            saveFile.println(msgSD);

            #ifdef _SD_
              // Check if the file is greater than 1 MB
              if (saveFile.fileSize() > 1048576) {
                swapFiles();
              }
            #endif
          } else {
            // If the file didn't open, print an error
            #ifdef _DEBUG_
              Serial.println("No save file to write to!");
            #endif
            (BLEOn) ? BTSerial.println("SD_WRITE_ERROR") : Serial.println("SD_WRITE_ERROR");
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


