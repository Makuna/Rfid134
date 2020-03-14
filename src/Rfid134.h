/*-------------------------------------------------------------------------
Rfid134 library

Written by Michael C. Miller.

I invest time and resources providing this open source code,
please support me by dontating (see https://github.com/Makuna/DFMiniMp3)

-------------------------------------------------------------------------
This file is part of the Makuna/Rfid134 library.

Rfid134 is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Rfid134 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with DFMiniMp3.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/
#pragma once

enum Rfid134_Error
{
    Rfid134_Error_None,

    // from library
    Rfid134_Error_PacketSize = 0x81,
    Rfid134_Error_PacketEndCodeMissmatch,
    Rfid134_Error_PacketChecksum,
    Rfid134_Error_PacketChecksumInvert
};

struct Rfid134Reading
{
    uint16_t country;
    uint64_t id;
    bool isData;
    bool isAnimal;
    uint16_t reserved0;
    uint32_t reserved1;
};

template<class T_SERIAL_METHOD, class T_NOTIFICATION_METHOD> class Rfid134
{
public:
    Rfid134(T_SERIAL_METHOD& serial) :
        _serial(serial)
    {
    }

    void begin()
    {
        // due to design differences in (Software)SerialConfig that make the serial.begin
        // method inconsistent between implemenations, it is required that the sketch
        // call serial.begin() before call Rfid134.begin()
        // _serial.begin(9600, SERIAL_8N2);
        _serial.setTimeout(1000);
    }

    void loop()
    {
        while (_serial.available() >= Rfid134_Packet_SIZE)
        {
            Rfid134_Error error = readPacket();
            if (error != Rfid134_Error_None)
            {
                T_NOTIFICATION_METHOD::OnError(error);
            }
        }
    }


private:
    T_SERIAL_METHOD& _serial;

    enum DfMp3_Packet
    {
        Rfid134_Packet_StartCode,
        Rfid134_Packet_Id = 1,
        Rfid134_Packet_Country = 11,
        Rfid134_Packet_DataFlag = 15,
        Rfid134_Packet_AnimalFlag = 16,
        Rfid134_Packet_Reserved0 = 17,
        Rfid134_Packet_Reserved1 = 21,
        Rfid134_Packet_CheckSum = 27,
        Rfid134_Packet_CheckSumInvert = 28,
        Rfid134_Packet_EndCode = 29,
        Rfid134_Packet_SIZE
    };

    Rfid134_Error readPacket()
    {
        char packet[Rfid134_Packet_SIZE];

        packet[Rfid134_Packet_StartCode] = _serial.read();

        // check for the first byte being the packet start code
        if (packet[Rfid134_Packet_StartCode] != 0x02)
        {
            // just out of sync, ignore until we are synced up
            return Rfid134_Error_None;
        }

        uint8_t read;

        read = _serial.readBytes(&(packet[Rfid134_Packet_Id]), Rfid134_Packet_SIZE - 1);

        if (read != Rfid134_Packet_SIZE - 1)
        {
            return Rfid134_Error_PacketSize;
        }

        if (packet[Rfid134_Packet_EndCode] != 0x03)
        {
            return Rfid134_Error_PacketEndCodeMissmatch;
        }

        // calculate checksum
        uint8_t checksum = 0;
        for (uint8_t i = Rfid134_Packet_Id; i < Rfid134_Packet_CheckSum; i++)
        {
            checksum = checksum ^ packet[i];
        }

        // test checksum
        if (checksum != packet[Rfid134_Packet_CheckSum])
        {
            return Rfid134_Error_PacketChecksum;
        }

        if (static_cast<uint8_t>(~checksum) != static_cast<uint8_t>(packet[Rfid134_Packet_CheckSumInvert]))
        {
            return Rfid134_Error_PacketChecksumInvert;
        }

        Rfid134Reading reading;

        // convert packet into the reading struct
        reading.id = HexLsbAsciiToUint64(&(packet[Rfid134_Packet_Id]), Rfid134_Packet_Country - Rfid134_Packet_Id);
        reading.country = HexLsbAsciiToUint64(&(packet[Rfid134_Packet_Country]), Rfid134_Packet_DataFlag - Rfid134_Packet_Country);
        reading.isData = packet[Rfid134_Packet_DataFlag] == '1';
        reading.isAnimal = packet[Rfid134_Packet_AnimalFlag] == '1';
        reading.reserved0 = HexLsbAsciiToUint64(&(packet[Rfid134_Packet_Reserved0]), Rfid134_Packet_Reserved1 - Rfid134_Packet_Reserved0);
        reading.reserved1 = HexLsbAsciiToUint64(&(packet[Rfid134_Packet_Reserved1]), Rfid134_Packet_CheckSum - Rfid134_Packet_Reserved1);

        T_NOTIFICATION_METHOD::OnPacketRead(reading);

        return Rfid134_Error_None;
    }


    uint64_t HexLsbAsciiToUint64(char* text, uint8_t textSize)
    {
        uint64_t value = 0;
        uint8_t i = textSize;
        do 
        {
            i--;

            uint8_t digit = text[i];
            if (digit >= 'A')
            {
                digit = digit - 'A' + 10;
            }
            else
            {
                digit = digit - '0';
            }
            value = (value << 4) + digit;
        } while (i != 0);

        return value;
    }
};
