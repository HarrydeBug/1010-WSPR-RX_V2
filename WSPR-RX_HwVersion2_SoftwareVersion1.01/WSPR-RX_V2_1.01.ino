
//Software for Zachtek for the WSPR-RX Hardware Version2 product with product number 1010.   
//The Version 2 of the hardware has been simplified so there are no buttons or jumpers to set the frequency or the be used in the Calibration phase.
//As a consequence the calibration and Frequency selection is done usign serial commands on the serial port.
//At boot time the Arduino checks the EEPROM and if it finds calibration data for the crystal and LO Frequency it will initialize with this data. 
//
//The Version of this software is stored in the constant "softwareversion" and is displayed on the Serialport att startup
//
//To compile you need to install several libraries, se below in the #include section what they are and download them using The Library manager in the Arduino IDE or use the direct URL
//For Arduino Pro Mini 3.3V 8MHz.
//
//  Version History
//  1.01 initial Release
//
//The Si5351 programming is base on Hans Summer Demo code. https://www.qrp-labs.com/synth/si5351ademo.html

#include <EEPROM.h>   // Arduino EEPROM lib.
#include <si5351.h>   //EtherKit
#include <CmdBuffer.hpp>          //https://github.com/neomilium/arduino-CmdParser
#include <CmdCallback.hpp>        //https://github.com/neomilium/arduino-CmdParser
#include <CmdParser.hpp>          //https://github.com/neomilium/arduino-CmdParser

CmdCallback<7> cmdCallback; // Number of commands listed below

char strSetBand[]   ="SETBAND";
char strSetRef[]    ="SETREF";
char strIncRef[]    ="INCREF";
char strDecRef[]    ="DECREF";
char strCalOut[]    ="CALOUT";
char strSave[]      ="SAVE";
char strHelp[]      ="HELP";

// Data structures
struct S_GadgetData
{
  uint32_t RefFreq;      //The exact frequency in Hertz of the Crystal Reference as determined by calibration
  uint32_t LOFreq;       //The local oscillator Frequency in Hertz
};

//Constants
#define I2C_START 0x08
#define I2C_START_RPT 0x10
#define I2C_SLA_W_ACK 0x18
#define I2C_SLA_R_ACK 0x40
#define I2C_DATA_ACK 0x28
#define I2C_WRITE 0b11000000
#define I2C_READ 0b11000001
#define SI5351A_H
#define SI_CLK0_CONTROL 16 // Register definitions
#define SI_CLK1_CONTROL 17
#define SI_CLK2_CONTROL 18
#define SI_SYNTH_PLL_A 26
#define SI_SYNTH_PLL_B 34
#define SI_SYNTH_MS_0 42
#define SI_SYNTH_MS_1 50
#define SI_SYNTH_MS_2 58
#define SI_PLL_RESET 177
#define SI_R_DIV_1 0b00000000 // R-division ratio definitions
#define SI_R_DIV_2 0b00010000
#define SI_R_DIV_4 0b00100000
#define SI_R_DIV_8 0b00110000
#define SI_R_DIV_16 0b01000000
#define SI_R_DIV_32 0b01010000
#define SI_R_DIV_64 0b01100000
#define SI_R_DIV_128 0b01110000
#define SI_CLK_SRC_PLL_A 0b00000000
#define SI_CLK_SRC_PLL_B 0b00100000

#define RXFREQ17m            18104600UL   //17m    18.104,600MHz
#define RXFREQ20m            14095600UL   //20m    14.095,600MHz 
#define RXFREQ30m            10138700UL   //30m    10.138,700MHz  
#define RXFREQ40m             7038600UL   //40m     7.038,600MHz 
#define RXFREQ80m             3568600UL   //80m     3.568,600MHz
#define RXFREQ160m            1836600UL   //160m    1.836,600MHz
#define RXFREQ630m             474200UL   //630m      474.200kHz
#define RXFREQ2190m            136000UL   //2190m     136.000kHz

//Global Variables
S_GadgetData GadgetData;                //A datastructure that holds all relevant data for the reciver like LO frequency etc
const char softwareversion[] = "1.01" ; //Version of this program, sent to serialport at startup
 


void setup() {
  Serial.begin (9600);
  while (!Serial);//Wait for Serialport to be initialized properly
  Serial.print(F("Zachtek HF WSPR-RX Hardware version 2, Software version: "));
  Serial.println(softwareversion);
  Serial.println(F("Initializing.."));
  //Tie Serial commands to Functions
  cmdCallback.addCmd(strSetBand, &functSetBand);
  cmdCallback.addCmd(strSetRef, &functSetRef);
  cmdCallback.addCmd(strIncRef, &functIncRef);
  cmdCallback.addCmd(strDecRef, &functDecRef);
  cmdCallback.addCmd(strCalOut, &functCalOut);
  cmdCallback.addCmd(strSave, &functSave);
  cmdCallback.addCmd(strHelp, &functHelp);
  
  i2cInit();
  if (LoadFromEPROM()==false) //Read configuration from EEPROM
  { //if Configuration and Calibration has not been run set some default values.
    GadgetData.RefFreq=25000000UL;  //Set Crystal Frequency to 25MHz 
    GadgetData.LOFreq=10000000UL;   //Set LO to 10MHz
    Serial.println (F("The receiver is not configured !"));
    Serial.println (F(""));
    PrintRefData();
  }
  SetLO();
  PrintRXData();
  Serial.println (F("Type HELP for information on commands"));
}

void loop()
{
//Do nothing but check for Serial commands as everything else was done in the Setup routine
 CmdBuffer<26> myBuffer;
 CmdParser     myParser;

  // Automatic handling of incoming comands on the serial port.
  cmdCallback.loopCmdProcessing(&myParser, &myBuffer, &Serial);
}

//Serial Command SetBand
void functSetBand(CmdParser *myParser) {
boolean ValidInput=true;
int Band;  
  Band = StrTouint64_t(myParser->getCmdParam(1));
   switch (Band) {
    case 2190:
      GadgetData.LOFreq=RXFREQ2190m;
      break;
    case 630:
      GadgetData.LOFreq=RXFREQ630m;
      break;
    case 160:
      GadgetData.LOFreq=RXFREQ160m;
      break;  
    case 80:
      GadgetData.LOFreq=RXFREQ80m;
      break;  
    case 40:
      GadgetData.LOFreq=RXFREQ40m;
      break;  
    case 30:
      GadgetData.LOFreq=RXFREQ30m;
      break;  
    case 20:
      GadgetData.LOFreq=RXFREQ20m;
      break;  
    case 17:
      GadgetData.LOFreq=RXFREQ17m;
      break;                                
    default:
    Serial.println(F("Invalid input!"));
    ValidInput=false;
      break;
  }   
  if (ValidInput) { 
    PrintRXData();
    SetLO();
  }
}

//Serial Command SetRef
void functSetRef(CmdParser *myParser) {
boolean ValidInput=true;  
int Ref;  
  Ref = StrTouint64_t(myParser->getCmdParam(1));  
  switch (Ref) {
    case 25:
      GadgetData.RefFreq=25000000UL;
      break;
    case 26:
      GadgetData.RefFreq=26000000UL;
      break;
    case 27:
      GadgetData.RefFreq=27000000UL;
      break;  
    default:
      Serial.println(F("Invalid input!"));
      ValidInput=false;
      break;
  }   
  if (ValidInput) {
    PrintRefData ();
    SetLO();
  }
}

//Serial Command IncRef
void functIncRef(CmdParser *myParser) {
int IncValue;  
  IncValue =StrTouint64_t(myParser->getCmdParam(1));
  if (IncValue >0 && IncValue <1000001){
    GadgetData.RefFreq=GadgetData.RefFreq + IncValue;
    PrintRefData ();
    SetLO();
  }
  else
  {
   Serial.println(F("Invalid input!"));   
  }
}

//Serial Command DecRef
void functDecRef(CmdParser *myParser) {
int DecValue;  
  DecValue=StrTouint64_t(myParser->getCmdParam(1)); 
  if (DecValue >0 && DecValue <1000001){ 
    GadgetData.RefFreq=GadgetData.RefFreq - DecValue;
    PrintRefData ();
    SetLO();
  }
  else
  {
   Serial.println(F("Invalid input!"));   
  }
}

//Serial Command CalOut
void functCalOut(CmdParser *myParser) {
  GadgetData.LOFreq=10000000UL;
  PrintRXData();
  SetLO();
}

void functSave(CmdParser *myParser) {
 SaveToEPROM ();
 PrintRefData();
 PrintRXData();
 Serial.println(F("Saved to EEPROM")); 
}


void functHelp (CmdParser *myparser) {
 HelpText ();
}



void softReset(){
  asm volatile ("  jmp 0");
}

void SetLO()
{
  si5351aSetFrequency(GadgetData.LOFreq*100UL);//Set LO freq, convert to centiHertz as used by Si5351 routine
}

void PrintRXData()
{
  Serial.print (F("Receive frequency set to is set to "));
  Serial.print (GadgetData.LOFreq);
  Serial.println (F("Hz"));
 
}

void PrintRefData()
{
  Serial.print(F("Reference Ocillator is set to "));
  Serial.print(GadgetData.RefFreq);
  Serial.println(F("Hz"));
}

unsigned long GetEEPROM_CRC(void) {

  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < sizeof(GadgetData) ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

bool LoadFromEPROM (void)
{
  unsigned long CRCFromEEPROM, CalculatedCRC;
  EEPROM.get(0, GadgetData);                      //Load all the data from EEPROM
  EEPROM.get(sizeof(GadgetData), CRCFromEEPROM);  //Load the saved CRC
  CalculatedCRC = GetEEPROM_CRC();                //Calculate the CRC of the saved data
  return (CRCFromEEPROM == CalculatedCRC);        //If  Stored and Calculated CRC are the same return true
}


void SaveToEPROM ()
{
  unsigned long CRCFromEEPROM;
  EEPROM.put(0, GadgetData);         //Save all the data to EEPROM at adress0
  CRCFromEEPROM = GetEEPROM_CRC ();  //Calculate CRC on the saved data
  EEPROM.put(sizeof(GadgetData), CRCFromEEPROM); //Save the CRC after the data
}



void HelpText () {

  Serial.println(F("Type one of the following serial commands to configure the Receiver: "));
  Serial.println(F(" : SETBAND ...  ,sets the receiver to receive on a specific band. Valid values are the following:"));
  Serial.println(F(" :          2190=2190m band   (136.000kHz)"));
  Serial.println(F(" :          630= 630m band    (474.200kHz)"));
  Serial.println(F(" :          160= 160 band   (1.836,600MHz)"));
  Serial.println(F(" :          80=  80m band   (3.568,600MHz)"));
  Serial.println(F(" :          40=  40m band   (7.038,600MHz)"));
  Serial.println(F(" :          30=  30m band  (10.138,700MHz)"));
  Serial.println(F(" :          20=  20m band  (14.095,600MHz)"));
  Serial.println(F(" :          17=  17m band  (18.104,600MHz)"));
  Serial.println(F(" : E.g 'Setband 20' sets receiver to the 20m band"));
  Serial.println(F(" : SETREF ...  , set the reference crystal frequency in MHz, valid values are 25, 26 or 27 for 25 to 27MHz crystals"));
  Serial.println(F(" : INCREF ...  , Increments the reference frequency in Hz, valid values are 1 to 1000.000"));
  Serial.println(F(" : DECREF ...  , Decrements the reference frequency in Hz, valid values are 1 to 1000.000"));
  Serial.println(F(" : CALOUT      , Sets the LO to 10MHz for calibration purposes"));
  Serial.println(F(" : SAVE      ,(SAVE) Saves the current settings to EEPROM"));
  Serial.println(F(" : HELP      ,(HELP) prints this information"));
  Serial.println(F(" :          Dont forget to end your command line with New Line and Carriage Return characters."));
 }


uint64_t  StrTouint64_t (String InString)
{
  uint64_t y = 0;

  for (int i = 0; i < InString.length(); i++) {
    char c = InString.charAt(i);
    if (c < '0' || c > '9') break;
    y *= 10;
    y += (c - '0');
  }
  return y;
}
 
String  uint64ToStr (uint64_t p_InNumber, boolean p_LeadingZeros)
{
  char l_HighBuffer[7]; //6 digits + null terminator char
  char l_LowBuffer[7]; //6 digits + null terminator char
  char l_ResultBuffer [13]; //12 digits + null terminator char
  String l_ResultString = "";
  uint8_t l_Digit;

  sprintf(l_HighBuffer, "%06lu", p_InNumber / 1000000L); //Convert high part of 64bit unsigned integer to char array
  sprintf(l_LowBuffer, "%06lu", p_InNumber % 1000000L); //Convert low part of 64bit unsigned integer to char array
  l_ResultString = l_HighBuffer;
  l_ResultString = l_ResultString + l_LowBuffer; //Copy the 2 part result to a string

  if (!p_LeadingZeros) //If leading zeros should be removed
  {
    l_ResultString.toCharArray(l_ResultBuffer, 13);
    for (l_Digit = 0; l_Digit < 12; l_Digit++ )
    {
      if (l_ResultBuffer[l_Digit] == '0')
      {
        l_ResultBuffer[l_Digit] = ' '; // replace zero with a space character
      }
      else
      {
        break; //We have found all the leading Zeros, exit loop
      }
    }
    l_ResultString = l_ResultBuffer;
    l_ResultString.trim();//Remove all leading spaces
  }
  return l_ResultString;
}


uint8_t i2cStart()
{
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

  while (!(TWCR & (1 << TWINT))) ;

  return (TWSR & 0xF8);
}

void i2cStop()
{
  TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);

  while ((TWCR & (1 << TWSTO))) ;
}

uint8_t i2cByteSend(uint8_t data)
{
  TWDR = data;

  TWCR = (1 << TWINT) | (1 << TWEN);

  while (!(TWCR & (1 << TWINT))) ;

  return (TWSR & 0xF8);
}

uint8_t i2cByteRead()
{
  TWCR = (1 << TWINT) | (1 << TWEN);

  while (!(TWCR & (1 << TWINT))) ;

  return (TWDR);
}

uint8_t i2cSendRegister(uint8_t reg, uint8_t data)
{
  uint8_t stts;

  stts = i2cStart();
  if (stts != I2C_START) return 1;

  stts = i2cByteSend(I2C_WRITE);
  if (stts != I2C_SLA_W_ACK) return 2;

  stts = i2cByteSend(reg);
  if (stts != I2C_DATA_ACK) return 3;

  stts = i2cByteSend(data);
  if (stts != I2C_DATA_ACK) return 4;

  i2cStop();

  return 0;
}

uint8_t i2cReadRegister(uint8_t reg, uint8_t *data)
{
  uint8_t stts;

  stts = i2cStart();
  if (stts != I2C_START) return 1;

  stts = i2cByteSend(I2C_WRITE);
  if (stts != I2C_SLA_W_ACK) return 2;

  stts = i2cByteSend(reg);
  if (stts != I2C_DATA_ACK) return 3;

  stts = i2cStart();
  if (stts != I2C_START_RPT) return 4;

  stts = i2cByteSend(I2C_READ);
  if (stts != I2C_SLA_R_ACK) return 5;

  *data = i2cByteRead();

  i2cStop();

  return 0;
}

// Init TWI (I2C)
//
void i2cInit()
{
  TWBR = 92;
  TWSR = 0;
  TWDR = 0xFF;
  PRR = 0;
}

//
// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
//
void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
  uint32_t P1; // PLL config register P1
  uint32_t P2; // PLL config register P2
  uint32_t P3; // PLL config register P3

  P1 = (uint32_t)(128 * ((float)num / (float)denom));
  P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
  P2 = (uint32_t)(128 * ((float)num / (float)denom));
  P2 = (uint32_t)(128 * num - denom * P2);
  P3 = denom;

  i2cSendRegister(pll + 0, (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 1, (P3 & 0x000000FF));
  i2cSendRegister(pll + 2, (P1 & 0x00030000) >> 16);
  i2cSendRegister(pll + 3, (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 4, (P1 & 0x000000FF));
  i2cSendRegister(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 &
                  0x000F0000) >> 16));
  i2cSendRegister(pll + 6, (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 7, (P2 & 0x000000FF));
}

//
// Set up MultiSynth with integer Divider and R Divider
// R Divider is the bit value which is OR'ed onto the appropriate
// register, it is a #define in si5351a.h
//
void setupMultisynth(uint8_t synth, uint32_t Divider, uint8_t rDiv)
{
  uint32_t P1; // Synth config register P1
  uint32_t P2; // Synth config register P2
  uint32_t P3; // Synth config register P3

  P1 = 128 * Divider - 512;
  P2 = 0; // P2 = 0, P3 = 1 forces an integer value for the Divider
  P3 = 1;

  i2cSendRegister(synth + 0, (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 1, (P3 & 0x000000FF));
  i2cSendRegister(synth + 2, ((P1 & 0x00030000) >> 16) | rDiv);
  i2cSendRegister(synth + 3, (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 4, (P1 & 0x000000FF));
  i2cSendRegister(synth + 5, ((P3 & 0x000F0000) >> 12) | ((P2 &
                  0x000F0000) >> 16));
  i2cSendRegister(synth + 6, (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 7, (P2 & 0x000000FF));
}

//
// Switches off Si5351a output
// Example: si5351aOutputOff(SI_CLK0_CONTROL);
// will switch off output CLK0
//
void si5351aOutputOff(uint8_t clk)
{
  i2cSendRegister(clk, 0x80); // Refer to SiLabs AN619 to see
  //bit values - 0x80 turns off the output stage
}




//
// Set CLK0 output ON and to the specified frequency
// Frequency is in the range 10kHz to 150MHz and given in centiHertz (hundreds of Hertz)
// Example: si5351aSetFrequency(1000000200);
// will set output CLK0 to 10.000,002MHz
//
// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
void si5351aSetFrequency(uint64_t frequency) //Frequency is in centiHz
{

  int32_t FreqChange;
  uint64_t pllFreq;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t Divider;
  uint8_t rDiv;


  if (frequency > 100000000) { //If higher than 1MHz then set output divider to 1
    rDiv = SI_R_DIV_1;
    Divider = 90000000000ULL / frequency;// Calculate the division ratio. 900MHz is the maximum internal (expressed as deciHz)
    pllFreq = Divider * frequency; // Calculate the pllFrequency:
    mult = pllFreq / (GadgetData.RefFreq * 100UL); // Determine the multiplier to
    l = pllFreq % (GadgetData.RefFreq * 100UL); // It has three parts:
    f = l; // mult is an integer that must be in the range 15..90
    f *= 1048575; // num and denom are the fractional parts, the numerator and denominator
    f /= GadgetData.RefFreq; // each is 20 bits (range 0..1048575)
    num = f; // the actual multiplier is mult + num / denom
    denom = 1048575; // For simplicity we set the denominator to the maximum 1048575
    num = num / 100;
  }
  else // lower freq than 1MHz - set output Divider to 128
  {
    rDiv = SI_R_DIV_128;
    frequency = frequency * 128ULL; //Set base freq 128 times higher as we are dividing with 128 in the last output stage
    Divider = 90000000000ULL / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal
    pllFreq = Divider * frequency; // Calculate the pllFrequency:
    mult = pllFreq / (GadgetData.RefFreq * 100UL); // Determine the multiplier to
    l = pllFreq % (GadgetData.RefFreq * 100UL); // It has three parts:
    f = l; // mult is an integer that must be in the range 15..90
    f *= 1048575; // num and denom are the fractional parts, the numerator and denominator
    f /= GadgetData.RefFreq; // each is 20 bits (range 0..1048575)
    num = f; // the actual multiplier is mult + num / denom
    denom = 1048575; // For simplicity we set the denominator to the maximum 1048575
    num = num / 100;
  }

  // Set up PLL A with the calculated  multiplication ratio
  setupPLL(SI_SYNTH_PLL_A, mult, num, denom);

  // Set up MultiSynth Divider 0, with the calculated Divider.
  // The final R division stage can divide by a power of two, from 1..128.
  // reprented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
  // If you want to output frequencies below 1MHz, you have to use the
  // final R division stage
  setupMultisynth(SI_SYNTH_MS_0, Divider, rDiv);
  // Reset the PLL. 
    i2cSendRegister(SI_PLL_RESET, 0xA0);
  // Finally switch on the CLK0 output (0x4F)
  // and set the MultiSynth0 input to be PLL A
  i2cSendRegister(SI_CLK0_CONTROL, 0x4F | SI_CLK_SRC_PLL_A);
}

