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
#define CMGS 0x434d4753

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
  int i = 0;
  for( ; i < len * 2; ++i )
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
  message[ i++ ] = 0x1a; // Ctrl+z
  // message[ i++ ] = 0x1b; // ESC
  message[ i ] = 0;
}

int convertPduToHex( uint8_t* message, uint8_t* hex )
{
  //Serial.println( (char*)message );
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

void readSensor( float &temperature, float &humiditiy )
{
  temperature = sht1x.readTemperatureC();
  humiditiy = sht1x.readHumidity();
}

bool sendSms( const uint8_t* number, const char* message )
{
  Serial.println( "Starting SMS" );
  delay( 100 );
  uint8_t hex[ MAX_STR ];
  int i = 0;
  hex[ i++ ] = 0x00; // Use phone SMSC ?
  hex[ i++ ] = 0x11;

  hex[ i++ ] = 0x02;

  // Receiver phone number
  int length = ( number[ 0 ] + 1 ) / 2; // bytes

  for( int j = 0; j < ( ( number[ 0 ] + 1 ) / 2 ) + 2; ++j )
  {
    // Serial.println( number[ j ], HEX );
    hex[ i++ ] = number[ j ];
  }
  hex[ i++ ] = 0x00; // TP-PID
  hex[ i++ ] = 0x00; // TP-DCS
  hex[ i++ ] = 0x00; // Validity-period
  uint8_t len = strlen( message );
  hex[ i++ ] = len; // String length
  length += ( len - ( len >> 3 ) );
  for( unsigned int j = 0; j < strlen( message ); ++j )
  {
    if( j % 8 == 0 )
    {
      hex[ i ] = message[ j ];
    }
    else
    {
      hex[ i++ ] |= message[ j ] << ( 8 - ( j % 8 ) );
      hex[ i ] = message[ j ] >> ( j % 8 );
    }
  }
  ++i;

  convertHexToPdu( hex, i, sendReceiveString );
  // Serial.print( "AT+CMGS=" );
  // Serial.println( length );
  // Serial.println( (char*)sendReceiveString );
  cellSerial.print( "AT+CMGS=" );
  cellSerial.println( length );
  if( waitForReceive( '>', 5000 ) )
  {
    cellSerial.print( (char*)sendReceiveString );
    return true;
  }
  return false;
}

void handleSms( uint8_t* str )
{
  //Serial.println( (char*)str );
  uint8_t hex[ MAX_STR ];
  // int len =
  convertPduToHex( str, hex );

  int pos = hex[ 0 ] + 2; // SMSC len + 2 following bytes
  pos += 11 + ( hex[ pos ] + 1 ) / 2; // number of nibbles for phone number plus international indicator and

  float t, h;
  readSensor( t, h );

  int8_t th = t;
  int8_t tl = (uint8_t)( ( t - th ) * 100.0 );
  int8_t hh = h;
  int8_t hl = (uint8_t)( ( h - hh ) * 100.0 );

  if( hex[ pos ] > 0 )
  {
    switch( hex[ pos + 1 ] & 0x7F )
    {
    case 'A': // Norwegian for OFF is "AV"
    case 'a':
      {
        // Turn off
        //Serial.println( "OFF!" );
        digitalWrite( 13, LOW );

        String fixedString = String( "Turned OFF. Thermal reading: " );
        fixedString += th;
        fixedString += ".";
        if( tl < 10 )
        {
          fixedString += "0";
        }
        fixedString += tl;
        fixedString += " Humidity: ";
        fixedString += hh;
        fixedString += ".";
        if( hl < 10 )
        {
          fixedString += "0";
        }
        fixedString += hl;
        fixedString.toCharArray( (char*)sendReceiveString, MAX_STR );
        //Serial.println( (char*)sendReceiveString );
        sendSms( hex + ( hex[ 0 ] + 2 ), (char*)sendReceiveString );
        break;
      }
    case 'p': // Norwegian for ON is "PÅ"
    case 'P':
      {
        // Turn on
        //Serial.println( "ON!" );
        digitalWrite( 13, HIGH );

        String fixedString = String( "Turned ON. Thermal reading: " );
        fixedString += th;
        fixedString += ".";
        if( tl < 10 )
        {
          fixedString += "0";
        }
        fixedString += tl;
        fixedString += " Humidity: ";
        fixedString += hh;
        fixedString += ".";
        if( hl < 10 )
        {
          fixedString += "0";
        }
        fixedString += hl;
        fixedString.toCharArray( (char*)sendReceiveString, MAX_STR );
        //Serial.println( (char*)sendReceiveString );
        sendSms( hex + ( hex[ 0 ] + 2 ), (char*)sendReceiveString );
        break;
      }
    default:
      {
        // Anything else sends status
        //Serial.println( "Status" );
        break;
      }
    }
  }
}

void deleteAllSms()
{
  // Serial.print( "Delete: " );
  // Serial.println( (char*)asciiIndex );
  char command[15] = "AT+CMGD=1,4";
  Serial.println( command );
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
              cellSerial.println( "AT+CNMI=3,1,0,0" );
              receiveData( sendReceiveString, 5000 );
              // uint8_t hex[ 10 ] = { 0x0a, 0x91, 0x74, 0x09, 0x81, 0x94, 0x51 };
              // sendSms( hex, "Testing 123" );
            }
            break;
          }
        case CMGL:
        case CMGR:
          {
            // We must read another line to get the SMS content
            receiveString( str, timeout );
            // Now handle the sms
            handleSms( str );
            // Delete message
            //deleteAllSms();
            break;
          }
        case CMTI: // +CMTI: "SM",1
          {
            //Serial.println( "AT+CMGL=4" );
            cellSerial.println( "AT+CMGL=4" );
            break;
          }
        case CMGD:
          {
            break;
          }
        case CMGS:
          {
            deleteAllSms();
          }
        default:
          {
            // Serial.println( "Unknown command" );
            break;
          }
        }
        break;
      }
    default:
      ;
      // Serial.println( "Unknown message received" );
    }
    return true;
  }
  return false;
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
  //cellSerial.println( "AT" );
  receiveData( sendReceiveString, 5000 );
  receiveData( sendReceiveString, 5000 );
  receiveData( sendReceiveString, 5000 );
  receiveData( sendReceiveString, 5000 );
  receiveData( sendReceiveString, 5000 );
}

// 0011020A91740981945100000031D4BADC5D26839E4E17888A2ECBDB6136485E0E93D3EEB30E2493B9643210B2DE4E93D3F4BC0E24B3B96239
/*

AT+CMGS=49
0011020A91740981945100000032D4BADC5D26839E46A30B444597E5ED301B242F87C969F7590792C55C391A08596FA7C9697A5E0792D55CB61B


 */
