/*
Based on
- DisplayReadings example fom INA library
  https://github.com/SV-Zanshin/INA
- QuickStart example form the SdFat library
  https://github.com/greiman/SdFat
- Object example on the CircularBuffer library
  https://github.com/rlogiacco/CircularBuffer
*/

#include <Arduino.h>

#include <INA.h>
INA_Class INA;

#include <SdFat.h>
SdFat SD;                                 // File system object.
File CSV;                                 // see config.h for FILENAME
ArduinoOutStream cout(Serial);            // stream to Serial

#include "config.h"                       // project configuration
class Record {
private:
  uint32_t time;
  uint32_t milliVolts[INA_COUNT];
  uint32_t microAmps[INA_COUNT];

public:
  Record(){};
  ~Record(){};
	Record(uint32_t time): time(time) {
    for (uint8_t i=0; i<INA_COUNT; i++) {
      milliVolts[i] = INA.getBusMilliVolts(i);
      microAmps[i] = INA.getBusMicroAmps(i);
    }
	}

  void header(Print* out) {
    out->print(F("millis"));
    for (uint8_t i=0; i<INA_COUNT; i++) {
      out->print(F(",ch"));out->print(i);out->print(F(" voltage [mV]"));
      out->print(F(",ch"));out->print(i);out->print(F(" current [uA]"));
    }
    out->println();
  }

  void print(Print* out) {
		out->print(time);
    for (uint8_t i=0; i<INA_COUNT; i++) {
      out->print(F(","));out->print(milliVolts[i]);
      out->print(F(","));out->print(microAmps[i]);
    }
    out->println();
	}
};

#include <CircularBuffer.h>
CircularBuffer<Record*, BUFFER_SIZE> buffer;

void setup() {
  Serial.begin(57600);                    // for ATmega328p 3.3V 8Mhz
  while(!Serial){ SysCall::yield(); }     // Wait for USB Serial
  if (!SD.begin(chipSelect, SPI_SPEED)) {
    SD.initErrorHalt();                   // errorcode/message to Serial
  }

  uint8_t INAfound = INA.begin(1,100000); // maxBusAmps,microOhmR
  while(INAfound != INA_COUNT){
    cout << F("ERROR: INA devices expected ") << INA_COUNT <<
            F(", found ") << INAfound << endl;
    delay(DELAY);
  }
  INA.setBusConversion(INA_CONVTIME);     // see config.h for value
  INA.setShuntConversion(INA_CONVTIME);   // see config.h for value
  INA.setAveraging(INA_SEMPLES);          // see config.h for value
  INA.setMode(INA_MODE_CONTINUOUS_BOTH);  // bus&shunt

  cout << F("INA devices on the I2C bus: ") << INA_COUNT << endl;
  for (uint8_t i=0; i<INA_COUNT; i++) {
    cout << F("ch") << i << F(": ") << INA.getDeviceName(i) << endl;
  }

  cout << F("Buffering ") <<
    BUFFER_SIZE << F(" records of ") <<
    (RECORD_SIZE*sizeof(uint32_t)) << F(" bytes before writing to SD:") <<
    FILENAME << endl;
}

void loop() {

  // buffer new data chunk
  uint32_t timestamp = millis();
  Record* record = new Record(timestamp);
  buffer.unshift(record);

  // dump buffer to CSV file
  if(buffer.isFull()){
    bool newfile = !SD.exists(FILENAME);
    CSV = SD.open(FILENAME, FILE_WRITE);
    if (!CSV) { SD.initErrorHalt(); }     // errorcode/message to Serial
    if (newfile){
      buffer.first()->header(&CSV);
    }
    while (!buffer.isEmpty()) {
      buffer.shift()->print(&CSV);
    }
    CSV.close();
  }

  // wait until next measurement time
  delay(millis()-timestamp+DELAY);
}
