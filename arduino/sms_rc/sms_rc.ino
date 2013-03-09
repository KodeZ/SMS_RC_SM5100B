/*
    Copyright 2013 Morgan Tørvolt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


    This program uses the SHT1x library, which you can find here:
    https://github.com/practicalarduino/SHT1x
*/

#include <SoftwareSerial.h>
#include <string.h>
#include <SHT1x.h>


#define MAX_STR 200
#define MAX_LOG 100

#define SIND 0x53494e44
#define CMGL 0x434d474C
#define CMGR 0x434d4752
#define CMGD 0x434d4744
#define CMTI 0x434d5449

SoftwareSerial cellSerial(2, 3); // RX, TX
SHT1x sht1x( 10, 11 );

uint8_t sendReceiveString[ MAX_STR ]; // For general usage. Can be used for both send and receive message, but will then be overwritten.
uint16_t temperatureLog[ MAX_LOG ]; // For general usage. Can be used for both send and receive message, but will then be overwritten.
int logSaveCounter = 0;
uint32_t nextLog = 0;

inline uint8_t ctoi( uint8_t c )
{
  if( c >= 0x41 && c <= 0x46 )
  {
    return c - 0x37;
  }
  if( c <= 0x39 && c >= 0x30 )
  {
    return c - 0x30;
  }
  return 0;
}

inline uint8_t itoc( uint8_t c )
{
  if( c >= 0x0A )
  {
    return c + 0x37;
  }
  return c + 0x30;
}

void convertHexToPdu( uint8_t* hex, int len, uint8_t* message )
{
  for( int i = 0; i < len * 2; ++i )
  {
    // Serial.print( "Here:" );
    // Serial.print( i );
    // Serial.print( " char:" );
    // Serial.print( ( hex[ i / 2 ] >> ( ( ( i + 1 ) % 2 ) * 4 ) ) & 0x0f );
    // Serial.print( " val:" );
    // Serial.write( itoc( ( hex[ i / 2 ] >> ( ( ( i + 1 ) % 2 ) * 4 ) ) & 0x0f ) );
    // Serial.println( "" );
    message[ i ] = itoc( ( hex[ i / 2 ] >> ( ( ( i + 1 ) % 2 ) * 4 ) ) & 0x0f );
  }
}

int convertPduToHex( uint8_t* message, uint8_t* hex )
{
  Serial.println( (char*)message );
  int i = 0;
  while( message[ i ] != 0 )
  {
    if( i % 2 == 1 )
    {
      hex[ i / 2 ] = hex[ i / 2 ] << 4;
    }
    else
    {
      hex[ i / 2 ] = 0;
    }
    hex[ i / 2 ] |= ctoi( message[ i ] );
    ++i;
  }
  if( i % 2 == 1 )
  {
    hex[ i / 2 ] = hex[ i / 2 ] << 4;
  }
  return ( i + 1 ) / 2;
}

bool receiveExpected( uint8_t byte, unsigned int timeout )
{
  uint32_t t = millis();
  while( !cellSerial.available() && millis() - t < timeout );

  if( millis() - t >= timeout )
  {
    return false;
  }
  uint8_t b = cellSerial.read();
  if( b != byte )
  {
    return false;
  }
  return true;
}

bool waitForReceive( uint8_t byte, unsigned int timeout )
{
  uint32_t t = millis();
  uint8_t b = !byte;
  while( b != byte )
  {
    while( !cellSerial.available() && millis() - t < timeout );

    if( millis() - t >= timeout )
    {
      return false;
    }
    b = cellSerial.read();
  }
  return true;
}

bool receiveString( uint8_t* result, unsigned int timeout )
{
  int i = 0;
  uint32_t t = millis();
  result[ 0 ] = 0;

  if( waitForReceive( '\r', timeout ) && receiveExpected( '\n', timeout ) )
  {
    while( i < MAX_STR )
    {
      while( !cellSerial.available() && millis() - t < timeout );
      if( millis() - t >= timeout )
      {
        return false;
      }
      t = millis(); // reset timeout whenever char is received
      uint8_t b = cellSerial.read();
      if( b == '\r' )
      {
        result[ i ] = 0;
        return true;
      }
      result[ i ] = b;
      ++i;
    }
    result[ MAX_STR - 1 ] = 0;
    return true;
  }
  return false;
}

bool sendSms( char* number, char* message )
{
  int i = 0;
  sendReceiveString[ i++ ] = '0'; // Use phone SMSC ?
  sendReceiveString[ i++ ] = '0'; // Use phone SMSC ?

  sendReceiveString[ i++ ] = '0';
  sendReceiveString[ i++ ] = '0';

  sendReceiveString[ i++ ] = 0x00;
  sendReceiveString[ i++ ] = 0x00;
  sendReceiveString[ i++ ] = 0x00;
  sendReceiveString[ i++ ] = 0x00;
  sendReceiveString[ i++ ] = 0x00;
  sendReceiveString[ i++ ] = 0x00;

  //checkCommand( "", "OK" );
  return true;
}

void handleSms( uint8_t* str )
{
  uint8_t hex[ MAX_STR ];
  int len = convertPduToHex( str, hex );

  int pos = hex[ 0 ] + 2; // SMSC len + 2 following bytes
  pos += 11 + ( hex[ pos ] + 1 ) / 2; // number of nibbles for phone number plus international indicator and

  if( hex[ pos ] > 0 )
  {
    switch( hex[ pos + 1 ] & 0x7F )
    {
    case 'A': // Norwegian for OFF is "AV"
    case 'a':
      {
        // Turn off
        Serial.println( "OFF!" );
        digitalWrite( 13, LOW );
        break;
      }
    case 'p': // Norwegian for ON is "PÅ"
    case 'P':
      {
        // Turn on
        Serial.println( "ON!" );
        digitalWrite( 13, HIGH );
        break;
      }
    default:
      {
        // Anything else sends status
        Serial.println( "Status" );
        break;
      }
    }
  }
}

void deleteSms( char* asciiIndex )
{
  Serial.print( "Delete: " );
  Serial.println( (char*)asciiIndex );
  char command[15] = "AT+CMGD=";
  for( unsigned int i = 0; i <= strlen( asciiIndex ); ++i )
  {
    command[ 8 + i ] = asciiIndex[ i ];
  }
  cellSerial.println( command );
}

bool receiveData( uint8_t* str, int timeout )
{
  if( receiveString( str, timeout ) )
  {
    Serial.print( "Received:" );
    Serial.println( (char*)str );

    switch( str[ 0 ] )
    {
    case 'O':
      {
        if( str[ 1 ] == 'K' )
        {
          //Serial.println( "AOK!" );
        }
        break;
      }
    case '+':
      {
        uint32_t type = str[ 1 ];
        type = ( type << 8 ) | str[ 2 ];
        type = ( type << 8 ) | str[ 3 ];
        type = ( type << 8 ) | str[ 4 ];
        //Serial.println( type, HEX );
        switch( type )
        {
        case SIND:
          {
            if( str[ 7 ] == '4' )
            {
              cellSerial.println( "AT+SIND=1023" );
              receiveData( sendReceiveString, 5000 );
              cellSerial.println( "AT+CMGF=0" );
              receiveData( sendReceiveString, 5000 );
              cellSerial.println( "AT+CNMI=3,1,1,1" );
              receiveData( sendReceiveString, 5000 );
            }
            break;
          }
        case CMGL:
        case CMGR:
          {
            // Find index
            char asciiIndex[ 10 ];
            int i = 7;
            while( str[ i ] != ',' )
            {
              asciiIndex[ i - 7 ] = str[ i ];
              ++i;
            }
            asciiIndex[ i - 7 ] = 0;
            // We must read another line to get the SMS content
            receiveString( str, timeout );
            // Now handle the sms
            handleSms( str );
            // Delete message
            deleteSms( asciiIndex );
            break;
          }
        case CMTI: // +CMTI: "SM",1
          {
            cellSerial.println( "AT+CMGL=4" ); // +CMGL=4
            break;
          }
        case CMGD:
          {
            break;
          }
        default:
          {
            Serial.println( "Unknown command" );
            break;
          }
        }
        break;
      }
    default:
      Serial.println( "Unknown message received" );
    }
    return true;
  }
  return false;
}

void readSensor( float &temperature, float &humiditiy )
{
  temperature = sht1x.readTemperatureC();
  humiditiy = sht1x.readHumidity();
}

void storeTemp()
{
  float t, h;
  readSensor( t, h );
  int16_t tmp = t;
  tmp = tmp << 8;
  tmp |= (uint16_t)( ( t - ( tmp >> 8 ) ) * 100.0 );
  temperatureLog[ logSaveCounter++ ] = tmp;
  if( logSaveCounter >= MAX_LOG )
  {
    logSaveCounter = 0;
    // TODO: send data to a server for logging?
  }
}

void setup()
{
  // Output indicator led and relay driver
  pinMode( 13, OUTPUT );
  pinMode( 14, OUTPUT );

  // Open serial communications and wait for port to open:
  Serial.begin( 9600 );

  // set the data rate for the SoftwareSerial port
  cellSerial.begin(9600);

  //waitUntilReady();
}

void loop() // run over and over
{
  if( millis() > nextLog )
  {
    nextLog += 3600000;
    storeTemp();
  }
  cellSerial.println( "AT" );
  receiveData( sendReceiveString, 5000 );
  receiveData( sendReceiveString, 25000 );
}
