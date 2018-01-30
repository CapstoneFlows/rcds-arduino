#include <DistanceGP2Y0A21YK.h>
#include <elapsedMillis.h>
#include <QueueList.h>
#include <TimeLib.h>
#include <SD.h>

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

DistanceGP2Y0A21YK Dist;
File saveFile;
const int DeviceID = 0;

elapsedMillis timer0;
bool timer0up;

int distance;
int firstDist;
int lastDist;
int avgDist;

QueueList<String> queue;
String msg;
String filename;

// SET PINS APPROPRIATELY
int sensorPin = A1;
const int chipSelect = 4;

void processSyncMessage() {
  unsigned long pctime;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}

time_t requestSync()
{
  Serial.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}

void SDInit() {
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Initialization Failed!");
    return;
  }
  Serial.println("SD Initialization Done.");

  filename = DeviceID;
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
  filename += now(); 
  filename += ".txt";
  char temp[filename.length()+1];
  filename.toCharArray(temp, sizeof(temp));
  saveFile = SD.open(temp, FILE_WRITE);
}

void setup() {
  Serial.begin(9600);
  Dist.begin(sensorPin);

  setSyncProvider( requestSync);  //set function to call when sync required
  while (timeStatus() == timeNotSet) {
    if (Serial.available()) {
      processSyncMessage();
    }
  }
  timer0up = false;
  timer0 = 0;
  
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
        msg = now();
        msg += ", ";
        firstDist = distance;
      } else {
        lastDist = distance;
      }
    }
  } else {
    if (timer0up == true) {
      // Was the length of time valid (more than 100 ms)?
      if (timer0 > 100) {
        msg += timer0;
        msg += ", ";
        avgDist = (firstDist + lastDist) / 2;
        msg += avgDist;
        queue.push(msg);
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
          //Serial.println("Error opening SD file");
          Serial.println(queue.pop());
        }
      }
    }
  }
}


