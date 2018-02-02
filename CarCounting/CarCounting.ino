#include <DistanceGP2Y0A21YK.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <QueueList.h>
#include <TimeLib.h>
#include <SD.h>

#define _DEBUG_

// General use definitions and declarations
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

DistanceGP2Y0A21YK Dist;
SoftwareSerial BTSerial(2, 3);  // SET PINS APPROPRIATELY
File saveFile;

// Device specific information
String DeviceID;
String DeviceLoc;
String DeviceDir;
String DevComment;
// NEED TO STORE TO FILE ON SD FOR RETRIEVAL

// Timer
elapsedMillis timer0;
bool timer0up;
bool synced;

// Regarding the sensor
int distance;
int firstDist;
int lastDist;
int avgDist;

// Regarding BLE/Serial
String msgSer;
int state = 0;

// Regarding the SD card
QueueList<String> queue;
String msgSD;
String filename;

// SET PINS APPROPRIATELY
int sensorPin = A1;
const int chipSelect = 4;

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

// SD functions
void SDInit() {
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Initialization Failed!");
    return;
  }
  Serial.println("SD Initialization Done.");

  // SAVE DEVICE VARIABLES

  filename = DeviceID;
  filename += "_";
  filename = DeviceLoc;
  filename += "_";
  filename = DeviceDir;
  filename += "_";
  filename += now(); 
  filename += ".txt";
  char temp[filename.length()+1];
  filename.toCharArray(temp, sizeof(temp));
  saveFile = SD.open(temp, FILE_WRITE);
}

void swapFiles() {
  saveFile.close();
  filename = DeviceID;
  filename += "_";
  filename = DeviceLoc;
  filename += "_";
  filename = DeviceDir;
  filename += "_";
  filename += now(); 
  filename += ".txt";
  char temp[filename.length()+1];
  filename.toCharArray(temp, sizeof(temp));
  saveFile = SD.open(temp, FILE_WRITE);
}

// BLE connection
void setParamsCommands() {
  while (Serial.available() > 0)   {
    msgSer = Serial.readString();
  }

  if (msgSer.indexOf("ID=") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif

          msgSer.remove(0, 2);
          DeviceID = msgSer;
  }
  else if (msgSer.indexOf("LOC=") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif

          msgSer.remove(0, 3);
          DeviceLoc = msgSer;
  }
  else if (msgSer.indexOf("DIR=") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif

          msgSer.remove(0, 3);
          DeviceDir = msgSer;
  }
  else if (msgSer.indexOf("COMMENT=") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif

          msgSer.remove(0, 7);
          DevComment = msgSer;
  }
}

void runCommands() {
  while (Serial.available() > 0)   {
    msgSer = Serial.readString();
  }

  if (msgSer.indexOf("START_RUNNING") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif
  } else if (msgSer.indexOf("STOP_RUNNING") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif
  } else if (msgSer.indexOf("RETURN_DATA") > 0) {
          #ifdef _DEBUG_
            Serial.print(">>Received command: ");
            Serial.println(msgSer);
          #endif
  }
}

// Main setup and loop
void setup() {
  Serial.begin(9600);
  BTSerial.begin(38400);
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

  // CHECK FOR DEVICE VARIABLES ON DISK

  while (DeviceID == "" && DeviceLoc == "" && DeviceDir == "") {
    setParamsCommands();
  }
  
  SDInit();
}

void loop() {
  distance = Dist.getDistanceCentimeter();

  // Valid distances are 10 cm to 40 cm
  if (distance > 10) {
    if (distance < 40) {
      // Are we already counting time?
      if (timer0up == false) {
        timer0up = true;
        timer0 = 0;
        msgSD = now();
        msgSD += ", ";
        firstDist = distance;
      } else {
        lastDist = distance;
        // Collect every 10 samples
      }
    }
  } else {
    if (timer0up == true) {
      // Was the length of time valid (more than 100 ms)?
      if (timer0 > 100) {
        msgSD += timer0;
        msgSD += ", ";
        avgDist = (firstDist + lastDist) / 2;
        msgSD += avgDist;
        queue.push(msgSD);
        }
        timer0up = false; 
    } else {
      // Take the opportunity to write to SD
      if (!queue.isEmpty()) {
        if (saveFile) {
          saveFile.println(queue.pop());
          
          // Check if the file is greater than 1 MB
          if (saveFile.size() > 1048576) {
            swapFiles();
          }
        } else {
          // If the file didn't open, print an error
          #ifdef _DEBUG_
            Serial.println("Error opening SD file");
          #endif
          
          Serial.println(queue.pop());
        }
      }
    }
  }
}


