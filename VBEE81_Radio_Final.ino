/*
 Project: ValBal EE 8.1 Radio Code
 Created: 08/26/2017 12:33:25 PM
 Author:  Aria Tedjarati

 Description:
 APRS, SSTV, GFSK (2-way) radio communication as a payload board for ValBal EE 8.1 system

 Credits:
 Femtoloon and Iskender Kushan's SSTV code.

 Status:
 07/09/2017 Starting
 08/26/2017 Hardware confirmed working, SSTV, .wav file works, GFSK works, power works.  
 08/27/2017 APRS works.  All hardware is complete. The board is fully functional.
 09/17/2017 Test code for the range test is written.

 TODO as of 09/17/2017:
 Interface with mainboard, write full flight code for entire system.
*/

/*************************************************************************************************************************************************************************************************/

// Libraries
#include <Wire.h>
#include <SD.h>
#include <Audio.h>
#include <SerialFlash.h>
#include <RH_RF24.h>
#include <aprs.h>

/*************************************************************************************************************************************************************************************************/

// Pins
#define   PAYLOAD_1          4
#define   SCAP_PG            27
#define   EN_5V              30
#define   GFSK_SDN           6
#define   GFSK_IRQ           2
#define   GFSK_GATE          5
#define   GFSK_GPIO_0        21
#define   GFSK_GPIO_1        20
#define   GFSK_GPIO_2        7
#define   GFSK_GPIO_3        8
#define   GFSK_CS            25
#define   BOOST_EN           16
#define   SD_CS              23
#define   PAYLOAD_4          3
#define   DORJI_SLP          15
#define   DORJI_SQ           28
#define   DORJI_GATE         31        // Dorji on/off switch.  Write high for off, low for on.
#define   DORJI_PSEL         32        // Dorji power select pin. Write high for high power, low for low power.
#define   DORJI_AUD          A0        // Dorji audio output to MCU.
#define   DORJI_PTT          17
#define   V_SPRCAP           A10       // Supercapacitor voltage analog input.
#define   BAT_V              A11       // Battery voltage analog input.
#define   DORJI_DAC          A14       // DAC output to Dorji
#define   UART_DORJI         Serial1
#define   UART_MAINBOARD     Serial2

/*************************************************************************************************************************************************************************************************/

// Objects
File                     logfile;                    // SD Card Logfile
AudioPlaySdWav           playWav1;                   // SSTV Audio File
AudioOutputAnalog        audioOutput;
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;
RH_RF24                  rf24(GFSK_CS, GFSK_IRQ, GFSK_SDN);

/*************************************************************************************************************************************************************************************************/

// Constants and Tuneable Variables
const int     DORJI_BAUD =             9600;
const int     DRA818V_PTT_DELAY =      150;             // Milliseconds to wait after PTT to transmit
const int     DRA818V_COMMAND_DELAY =  250;             // Milliseconds to wait after sending a UART command
const int     DORJI_WAKEUP_INT =       5000;            // Milliseconds to wake dorji up
#define       S_CALLSIGN               "KK6MIR"         // FCC Callsign
#define       S_CALLSIGN_ID            1                // 11 is usually for balloons
#define       D_CALLSIGN               "APRS"           // Destination callsign: APRS (with SSID=0) is usually okay.
#define       D_CALLSIGN_ID            0                // Desination ID
#define       SYMBOL_TABLE             '/'              // Symbol Table: '/' is primary table '\' is secondary table
#define       SYMBOL_CHAR              'v'              // Primary Table Symbols: /O=balloon, /-=House, /v=Blue Van, />=Red Car

struct PathAddress addresses[] = {
  { (char *)D_CALLSIGN, D_CALLSIGN_ID },  // Destination callsign
  { (char *)S_CALLSIGN, S_CALLSIGN_ID },  // Source callsign
  { (char *)NULL, 0 }, // Digi1 (first digi in the chain)
  { (char *)NULL, 0 }  // Digi2 (second digi in the chain)
};

typedef enum {
        WAIT_FOR_COMMAND,
        SSTV_CHARGING,
        SSTV_CONFIG,
        TRANSMIT_SSTV,
        APRS_CHARGING,
        APRS_CONFIG,
        TRANSMIT_APRS,
        GFSK_CONFIG,
        TRANSMIT_GFSK
} states;

states        currentState;
states        previousState;
states        nextState;

int configuration = 1;

/*************************************************************************************************************************************************************************************************/

void setup() {
    pinMode(BOOST_EN, OUTPUT);
    pinMode(EN_5V, OUTPUT);
    pinMode(DORJI_GATE, OUTPUT);
    pinMode(GFSK_GATE, OUTPUT);
    pinMode(DORJI_SLP, OUTPUT);
    pinMode(DORJI_PTT, OUTPUT);
    pinMode(DORJI_PSEL, OUTPUT);
    pinMode(GFSK_SDN,OUTPUT);
    SupercapChargerOff();
    FiveVOff();
    DorjiOff();
    GFSKOff();
    DorjiSleep();
    DorjiPTTOff();
    DorjiLowPower();
    Wire.begin();//I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_400);
    //lowBoostPower();
    highBoostPower();
    SPI.setSCK(13);       // SCK on pin 13
    SPI.setMOSI(11);      // MOSI on pin 11
    SPI.setMISO(12);      // MOSI on pin 11
    SPI.setDataMode(SPI_MODE0); 
    SPI.setClockDivider(SPI_CLOCK_DIV2);  // Setting clock speed to 8mhz, as 10 is the max for the rfm22
    SPI.begin();
    Serial.begin(115200);
    Serial.println("ValBal 8.1 Radio Board, Summer 2017");   
    analogReadResolution(12);
    analogReference(INTERNAL);
    setupSDCard(); 
    AudioMemory(8);
    UART_DORJI.begin(DORJI_BAUD);
    aprs_setup(50, // number of preamble flags to send
      DORJI_PTT, // Use PTT pin
      500, // ms to wait after PTT to transmit
      0, 0 // No VOX ton
    );
    delay(3000);
}

/*************************************************************************************************************************************************************************************************/
int nAddresses = 0;
void loop() {
  switch (currentState){
    case WAIT_FOR_COMMAND:
      //if (digitalRead(PAYLOAD_1) == HIGH) nextState = SSTV_CHARGING;
      nextState = GFSK_CONFIG;//SSTV_CHARGING;
      break;
      
    case SSTV_CHARGING:
      SupercapChargerOn(); // Turn on sprcap charger
      while(superCapVoltage() <= 5.0) {
        delay(1000);
        Serial.println("Waiting for supercap to charge.  It is currently at: ");
        Serial.print(superCapVoltage()); Serial.println(" volts.");
      }
      FiveVOn();  // Turn on 5V line
      delay(500);
      nextState = SSTV_CONFIG;
      break;
      
    case SSTV_CONFIG:
      Serial.println("Configuring Dorji for SSTV transmit");
      DorjiOn();
      delay(1000);
      DorjiSleep();
      delay(1000);
      DorjiWake();
      delay(5000);
      DorjiLowPower();
      delay(1000);
      UART_DORJI.print("AT+DMOCONNECT\r\n"); //handshake command
      delay(1000);
      UART_DORJI.print("AT+DMOSETGROUP=1,144.5000,144.5000,0000,4,0000\r\n"); //set tx and rx frequencies 
      delay(1000);
      DorjiSleep();
      delay(3000);
      DorjiWake();
      delay(5000);
      nextState = TRANSMIT_SSTV;
      break;
      
    case TRANSMIT_SSTV:
      Serial.println("Transmitting SSTV");
      DorjiPTTOn(); // enable push to talk
      delay(1000);
      playFile("SSTVV.WAV");
      DorjiPTTOff(); // disable push to talk
      DorjiSleep(); //sleep sweet dorji <3
      delay(1000);
      DorjiOff();
      FiveVOff();
      nextState = APRS_CHARGING;
      break;
    
    case APRS_CHARGING:
      SupercapChargerOn(); // Turn on sprcap charger
      while(superCapVoltage() <= 5.0) {
        delay(1000);
        Serial.println("Waiting for supercap to charge.  It is currently at: ");
        Serial.print(superCapVoltage()); Serial.println(" volts.");
      }
      FiveVOn();  // Turn on 5V line
      delay(500);
      nextState = APRS_CONFIG;
      break;
      
    case APRS_CONFIG:
      Serial.println("Configuring Dorji for APRS transmit");
      nAddresses = 4;
      addresses[2].callsign = "WIDE1";
      addresses[2].ssid = 1;
      addresses[3].callsign = "WIDE2";
      addresses[3].ssid = 2;
      DorjiOn();
      delay(1000);
      DorjiSleep();
      delay(1000);
      DorjiWake();
      delay(5000);
      DorjiHighPower();
      delay(1000);
      UART_DORJI.print("AT+DMOCONNECT\r\n"); //handshake command
      delay(1000);
      UART_DORJI.print("AT+DMOSETGROUP=1,144.3900,144.3900,0000,4,0000\r\n"); //set tx and rx frequencies 
      delay(1000);
      DorjiSleep();
      delay(3000);
      DorjiWake();
      delay(5000);
      nextState = TRANSMIT_APRS;
      break;
      
    case TRANSMIT_APRS:
      for (int i = 0; i < 5; i++) {
        aprs_send(addresses, nAddresses
                , 27, 5, 50
                , 37.4380159, -122.1888185 // degrees
                , 0 // meters
                , 0
                , 0
                , SYMBOL_TABLE
                , SYMBOL_CHAR
                , "VB EE Radio v8.1");
        delay(1000);
      }
      DorjiOff();
      FiveVOff();
      SupercapChargerOff();
      nextState = GFSK_CONFIG;
      break;
      
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
//      uint16_t deviceTypee2 = ((buf[1] << 8) | buf[2]);
//      Serial.print("device type = ");
//      Serial.println(deviceTypee2, HEX);
      if (!rf24.setFrequency(433.5)) {
        Serial.println("setFrequency failed");
      } else {
        Serial.println("Frequency set to 433.5 MHz");
      }
      Serial.println("Please set the configuration.  Once you set the configuration, 10 messages in that configuration will be sent to the receiver, and then you will be brough back to this page.");
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
      logfile.print("Transmitting ten times at 433.5 MHz in configuration "); 
      logfile.println(configuration);
      logfile.flush();
      
      delay(1000);
      nextState = TRANSMIT_GFSK;
      break;
      
      case TRANSMIT_GFSK:   
//        while (true){
//          uint8_t data[100] = {0};
//          char dataa[100] = {'a'};
//          uint8_t leng;
//          boolean tru = rf24.recv(data,&leng);Serial.println(tru);
//          for (int i = 0; i < 100; i++){
//            dataa[i]=data[i];
//          Serial.print(dataa[i]);
//          }Serial.println();
//          delay(100);
//        }
      uint8_t data[] = "Hello This is to test the ValBal radio receiver and transmitter.  If you get this message completely this board worked.  Hooray!! 0123456789!@#$%^&*()_+ DONE";
      Serial.print("Starting 10 transmissions at configuration ");
      Serial.println(configuration);
      for (int k = 0; k < 10; k++){
        rf24.send(data, sizeof(data));
        rf24.waitPacketSent();
        Serial.print("Transmission #");
        Serial.print(k);
        Serial.println(" complete.");
        delay(3000);
      }
      GFSKOff();
      nextState = WAIT_FOR_COMMAND;
      break;
  }
  
  //update the state machine
  currentState = nextState;
  previousState = currentState;
}

/*************************************************************************************************************************************************************************************************/

double superCapVoltage(){
  return (double)analogRead(V_SPRCAP) * 1.2 * 5.99 / (double)pow(2, 12);
}

/*************************************************************************************************************************************************************************************************/

void setupSDCard() {
  if (!SD.begin(SD_CS)) {
    Serial.print("Error: Card failed, or not present");
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

void playFile(const char *filenamew) {
  Serial.print("Playing SSTV file: ");
  Serial.println(filenamew);
  Serial.println(playWav1.play(filenamew));
  delay(5);
  while (playWav1.isPlaying()) {
  }
}

/*************************************************************************************************************************************************************************************************/

void highBoostPower(){
    Wire.beginTransmission(0x2E);
    Wire.write(byte(0x10));
    Wire.endTransmission();
}

/*************************************************************************************************************************************************************************************************/

void lowBoostPower(){
    Wire.beginTransmission(0x2E);
    Wire.write(byte(0x7F));
    Wire.endTransmission();
}

/*************************************************************************************************************************************************************************************************/

void SupercapChargerOff(){
  digitalWrite(BOOST_EN, LOW);
}

/*************************************************************************************************************************************************************************************************/

void SupercapChargerOn(){
  digitalWrite(BOOST_EN, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void FiveVOff(){
  digitalWrite(EN_5V, LOW);
}

/*************************************************************************************************************************************************************************************************/

void FiveVOn(){
  digitalWrite(EN_5V, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void DorjiOn(){
  digitalWrite(DORJI_GATE, LOW);
}

/*************************************************************************************************************************************************************************************************/

void DorjiOff(){
  digitalWrite(DORJI_GATE, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void DorjiSleep(){
  digitalWrite(DORJI_SLP, LOW);
}

/*************************************************************************************************************************************************************************************************/

void DorjiWake(){
  digitalWrite(DORJI_SLP, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void DorjiLowPower(){
  digitalWrite(DORJI_PSEL, LOW);
}

/*************************************************************************************************************************************************************************************************/

void DorjiHighPower(){
  digitalWrite(DORJI_PSEL, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void DorjiPTTOn(){
  digitalWrite(DORJI_PTT, LOW);
}

/*************************************************************************************************************************************************************************************************/

void DorjiPTTOff(){
  digitalWrite(DORJI_PTT, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void GFSKOff(){
  digitalWrite(GFSK_GATE, HIGH);
}

/*************************************************************************************************************************************************************************************************/

void GFSKOn(){
  digitalWrite(GFSK_GATE, LOW);
}

