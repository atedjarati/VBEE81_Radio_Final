/*
 Project: ValBal EE Radio Receiver Code
 Created: 09/17/2017 12:33:25 PM
 Author:  Aria Tedjarati

 Description:
 GFSK Radio Receiver

 Credits:
 Femtoloon.

 Status:
 09/17/2017 Test code for the range test is written.

 TODO as of 09/17/2017:
 Interface with RPi, write full flight code for entire system.
*/

/*************************************************************************************************************************************************************************************************/

// Libraries
#include <Wire.h>
#include <SD.h>
#include <RH_RF24.h>

/*************************************************************************************************************************************************************************************************/

// Pins
#define   GFSK_SDN           19
#define   GFSK_IRQ           16
#define   GFSK_GATE          22
#define   GFSK_GPIO_0        21
#define   GFSK_GPIO_1        20
#define   GFSK_GPIO_2        17
#define   GFSK_GPIO_3        18
#define   GFSK_CS            15
#define   SD_CS              23
#define   LED                6

/*************************************************************************************************************************************************************************************************/

// Objects
File                     logfile;                    // SD Card Logfile
RH_RF24                  rf24(GFSK_CS, GFSK_IRQ, GFSK_SDN);

/*************************************************************************************************************************************************************************************************/

typedef enum {
        GFSK_CONFIG,
        TRANSMIT_GFSK
} states;

states        currentState;
states        previousState;
states        nextState;

int     configuration =       1;
int     nAddresses =          0;

/*************************************************************************************************************************************************************************************************/

void setup() {
    pinMode(GFSK_GATE, OUTPUT);
    pinMode(GFSK_SDN,OUTPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    GFSKOff();
    SPI.setSCK(13);       // SCK on pin 13
    SPI.setMOSI(11);      // MOSI on pin 11
    SPI.setMISO(12);      // MOSI on pin 11
    SPI.setDataMode(SPI_MODE0); 
    SPI.setClockDivider(SPI_CLOCK_DIV2);  // Setting clock speed to 8mhz, as 10 is the max for the rfm22
    SPI.begin();
    Serial.begin(115200);
    Serial.println("ValBal 8.1 Radio Receiver Board, Summer 2017");   
    analogReadResolution(12);
    analogReference(INTERNAL);
    setupSDCard(); 
    delay(3000);
}

/*************************************************************************************************************************************************************************************************/

void loop() {
  switch (currentState){
    case GFSK_CONFIG:
      GFSKOn();
      Serial.println("Talking to GFSK");
      Serial.println(rf24.init());
      Serial.println("GFSK Init complete");
      uint8_t buf[8];
      if (!rf24.command(RH_RF24_CMD_PART_INFO, 0, 0, buf, sizeof(buf))) {
        Serial.println("SPI ERROR");
      } else {
        Serial.println("SPI OK");
      }
      if (!rf24.setFrequency(433.5)) {
        Serial.println("setFrequency failed");
      } else {
        Serial.println("Frequency set to 433.5 MHz");
      }
      Serial.println("Please set the configuration.  Once you set the configuration, the receiver will stay in this configuration for 90 seconds, then you will be brought back to this page.");
      Serial.println("1 = FSK 500 bps");
      Serial.println("2 = FSK 5   kbps");
      Serial.println("3 = FSK 50  kbps");
      Serial.println("4 = FSK 150 kbps");
      Serial.println("5 = GFSK 500 bps");
      Serial.println("6 = GFSK 5 kbps");
      Serial.println("7 = GFSK 50 kbps");
      Serial.println("8 = GFSK 150 kbps");
      Serial.println("9 = OOK 5 kbps");
      Serial.println("10 = OOK 10 kbps");

      while(!Serial.available()){
      }

      if(Serial.available()){
        configuration = Serial.parseInt();
      }
      
      if (configuration == 1) rf24.setModemConfig(rf24.FSK_Rb0_5Fd1);    // FSK  500 bps   
      if (configuration == 2) rf24.setModemConfig(rf24.FSK_Rb5Fd10);     // FSK  5   kbps
      if (configuration == 3) rf24.setModemConfig(rf24.FSK_Rb50Fd100);   // FSK  50  kbps
      if (configuration == 4) rf24.setModemConfig(rf24.FSK_Rb150Fd300);  // FSK  150 kbps
      if (configuration == 5) rf24.setModemConfig(rf24.GFSK_Rb0_5Fd1);   // GFSK 500 bps  
      if (configuration == 6) rf24.setModemConfig(rf24.GFSK_Rb5Fd10);    // GFSK 5   kbps
      if (configuration == 7) rf24.setModemConfig(rf24.GFSK_Rb50Fd100);  // GFSK 50  kbps
      if (configuration == 8) rf24.setModemConfig(rf24.GFSK_Rb150Fd300); // GFSK 150 kbps  
      if (configuration == 9) rf24.setModemConfig(rf24.OOK_Rb5Bw30);     // OOK  5   kbps
      if (configuration == 10) rf24.setModemConfig(rf24.OOK_Rb10Bw40);   // OOK  10  kbps
      
      rf24.setTxPower(0x4f); //0x00 to 0x4f. 0x10 is default
      logfile.print("Receiving at 433.5 MHz in configuration "); 
      logfile.println(configuration);
      logfile.flush();
      logfile.print("Receieved Boolean");
      logfile.print(",");
      logfile.print("Length");
      logfile.print(",");
      logfile.print("Message");
      logfile.print(",");
      logfile.print("RSSI");
      logfile.print(",");
      logfile.print("BER");
      logfile.print(",");
      logfile.println("ByteER");
      logfile.flush();
      

      Serial.print("Receiving at 433.5 MHz in configuration "); 
      Serial.println(configuration);
      
      delay(1000);
      nextState = TRANSMIT_GFSK;
      break;
      
      case TRANSMIT_GFSK:   
        double loopStart = millis();
        Serial.print("Starting to receive transmissions for 90 seconds at configuration ");
        Serial.println(configuration);
        while ((millis()-loopStart) < 90000){
          uint8_t data[200] = {0};
          char dataa[200] = {'a'};
          uint8_t leng = 200;
          boolean tru = rf24.recv(data, &leng);
          if (tru){
            Serial.println("Received message is below:");
            for (int i = 0; i < leng; i++){
              dataa[i] = data[i];
              Serial.print(dataa[i]);
            }
            Serial.println();
            Serial.print(" Length :");
            Serial.println(leng);
          }
          uint8_t ref[] = "Hello This is to test the ValBal radio receiver and transmitter.  If you get this message completely this board worked.  Hooray!! 0123456789!@#$%^&*()_+ DONE";
          
          if (tru){
            logfile.print(tru);
            logfile.print(",");
            logfile.print(leng);
            logfile.print(",");
            for(int k = 0; k < 200; k++){
              logfile.print(dataa[k]);
            }
            uint8_t lastRssi = (uint8_t)rf24.lastRssi();
            logfile.print(lastRssi);
            int biterr = 0;
            int byteerr = 0;
            int len = strlen((const char*)ref);
            for (int i = 0; i < len; i++){
              if (dataa[i] != ref[i]) byteerr++;
              uint8_t sim = dataa[i] ^ ref[i];
              for (int kk=0; kk<8; kk++) {
                if (sim & 1) biterr++;
                sim >>= 1;
              }
            }
            logfile.print(",");
            logfile.print(((float)biterr)/(len*8.)*100);
            logfile.print(",");
            logfile.println(((float)byteerr)/(len)*100);
            logfile.flush();
            Serial.println("For this transmission: ");
            Serial.print("  Bit error rate: ");
            Serial.print(((float)biterr)/(len*8.)*100);
            Serial.println(" %");
            Serial.print("  Byte error rate: ");
            Serial.print(((float)byteerr)/(len)*100);
            Serial.println(" %");
            Serial.println();
            Serial.print(" LastRssi: ");
            Serial.println(lastRssi);
            
          }
      }
      nextState = GFSK_CONFIG;
      break;
  }
  
  //update the state machine
  previousState = currentState;
  currentState = nextState;

}

/*************************************************************************************************************************************************************************************************/

void setupSDCard() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Error: Card failed, or not present");
  }
  Serial.println("Card Initialitzed");
  //Serial.println(SD.open("SSTV_TEST-2.WAV"));
    
  char filename[] = "LOGGER00.CSV";
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = i / 10 + '0';
    filename[7] = i % 10 + '0';
    if (! SD.exists(filename)) {   // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }
  if (!logfile) {
    Serial.println ("ERROR: COULD NOT CREATE FILE");
  } else {
    Serial.print("Logging to: "); Serial.println(filename);
  }
}


/*************************************************************************************************************************************************************************************************/

void GFSKOff(){
  digitalWrite(GFSK_GATE, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void GFSKOn(){
  digitalWrite(GFSK_GATE, LOW);
}

