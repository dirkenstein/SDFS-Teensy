/*
  Listfiles

  This example shows how print out the files in a
  directory on a SD card

  The circuit:
   SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4

  created   Nov 2010
  by David A. Mellis
  modified 9 Apr 2012
  by Tom Igoe
  modified 2 Feb 2014
  by Scott Fitzgerald

  This example code is in the public domain.

*/
#include <SPI.h>
#include "SdFat.h"
#include "SDFS.h"
#include <TimeLib.h>
using namespace fs;

FS * SdFs;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  SPI.begin();
  pinMode(10, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH);
  Serial.println("Initializing SD card...");
  SdFs = new FS(fs::FSImplPtr(new sdfs::SDFSImpl()));
  SdFs->setConfig(SDFSConfig (new SdSpiConfig(10, SHARED_SPI, 40000000)));

  bool eep = SdFs->begin();
  Serial.println (eep);
  if (!eep) {
    Serial.println("initialization failed!");
    Serial.flush();
  }
  delay(500);
  Serial.println("initialization done.");
  Serial.flush();
  
}

void loop() {
  Serial.println("loooop");
  delay(500);
  Serial.println("opening root");
  fs::File root = SdFs->open("/", "r");
  Serial.println("opened root");
  Serial.flush();
  printDirectory(root, 0);
  //root.close();
  Serial.println("done!");
  // nothing happens after setup finishes.]
}

void printDirectory(fs::File &dir, int numTabs) {
  if (!dir) {
    Serial.printf("File not open\n");
    return;
  }
  while (true) {
    Serial.println("opening next file");
    fs::File entry =  dir.openNextFile();
    Serial.println("opened next file");
    if (! entry) {
      Serial.printf(" no next\n");
      break;
    }

    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }    

    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.printf("%s %d\n", __FILE__, __LINE__);
      Serial.println("/");
    
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.print(entry.size(), DEC);
      time_t cr = entry.getCreationTime();
      time_t lw = entry.getLastWrite();
      Serial.printf("\tCREATION: %d-%02d-%02d %02d:%02d:%02d", year(cr), month(cr),  day(cr), hour(cr), minute(cr), second(cr));
      Serial.printf("\tLAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", year(lw), month(lw),  day(lw), hour(lw), minute(lw), second(lw));
    }

    entry.close();
  }
}
