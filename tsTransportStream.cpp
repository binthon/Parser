#include "tsTransportStream.h"
#include <iostream>

//=============================================================================================================================================================================
// xTS_PacketHeader
//=============================================================================================================================================================================


/// @brief Reset - reset all TS packet header fields
void xTS_PacketHeader::Reset()
{
  m_SB = 0;
  m_E = 0;
  m_S = 0;
  m_P = 0;
  m_PID = 0;
  m_TSC = 0;
  m_AF = 0;
  m_CC = 0;
}

/**
  @brief Parse all TS packet header fields
  @param Input is pointer to buffer containing TS packet
  @return Number of parsed bytes (4 on success, -1 on failure) 
 */
int32_t xTS_PacketHeader::Parse(const uint8_t* Input) {
  if (Input == nullptr) return -1;

  const uint32_t* HeadPtr = (const uint32_t*)Input;
  uint32_t Head = xSwapBytes32(*HeadPtr);

  m_SB = (uint8_t)(Head >> 24);
  if (m_SB != 0x47) return -1;

  m_E = (uint8_t)((Head >> 23) & 0x01);
  m_S = (uint8_t)((Head >> 22) & 0x01);
  m_P = (uint8_t)((Head >> 21) & 0x01);
  m_PID = (uint16_t)((Head >> 8) & 0x1FFF); 
  m_TSC = (uint8_t)((Head >> 6) & 0x03);
  m_AF = (uint8_t)((Head >> 4) & 0x03);
  m_CC = (uint8_t)(Head & 0x0F);

  return 4;
}

/// @brief Print all TS packet header fields
void xTS_PacketHeader::Print() const
{
  std::cout << "TS: ";
  std::cout << "SB=" << (int)m_SB << " ";
  std::cout << "E=" << (int)m_E << " ";
  std::cout << "S=" << (int)m_S << " ";
  std::cout << "P=" << (int)m_P << " ";
  std::cout << "PID=" << (int)m_PID << " ";
  std::cout << "TSC=" << (int)m_TSC << " ";
  std::cout << "AF=" << (int)m_AF << " ";
  std::cout << "CC=" << (int)m_CC << " ";
}

/// @brief Reset - reset all AF packet header fields
void xTS_AdaptationField::Reset()
{
  m_AdaptationFieldControl = 0;
  m_AdaptationFieldLength = 0;
  m_DC = 0;
  m_RA = 0;
  m_SP = 0;
  m_PR = 0;
  m_OR = 0;
  m_SF = 0;
  m_TP = 0;
  m_EX = 0;
}

/**
@brief Parse adaptation field
@param PacketBuffer is pointer to buffer containing TS packet
@param AdaptationFieldControl is value of Adaptation Field Control field of
corresponding TS packet header
@return Number of parsed bytes (length of AF or -1 on failure)
*/
int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer)
{
  if (PacketBuffer == nullptr) return -1;

  const uint8_t* AFPtr = PacketBuffer + 4; 
  m_AdaptationFieldLength = *AFPtr++;
  if (m_AdaptationFieldLength == 0) return -1;

  m_DC = (*AFPtr >> 7) & 0x01;
  m_RA = (*AFPtr >> 6) & 0x01;
  m_SP = (*AFPtr >> 5) & 0x01;
  m_PR = (*AFPtr >> 4) & 0x01;
  m_OR = (*AFPtr >> 3) & 0x01;
  m_SF = (*AFPtr >> 2) & 0x01;
  m_TP = (*AFPtr >> 1) & 0x01;
  m_EX = *AFPtr++ & 0x01;

  return m_AdaptationFieldLength + 1; 
}

/// @brief Print all TS packet header fields
void xTS_AdaptationField::Print() const
{
  //print print print
  std::cout << "AF: ";
  std::cout << "L=" << (int)getAdaptationFieldLength() << " ";
  std::cout << "DC=" << (int)m_DC << " ";
  std::cout << "RA=" << (int)m_RA << " ";
  std::cout << "SP=" << (int)m_SP << " ";
  std::cout << "PR=" << (int)m_PR << " ";
  std::cout << "OR=" << (int)m_OR << " ";
  std::cout << "SF=" << (int)m_SF << " ";
  std::cout << "TP=" << (int)m_TP << " ";
  std::cout << "EX=" << (int)m_EX << " ";
}

/// @brief Reset - reset all PES packet header fields
void xPES_PacketHeader::Reset()
{
  m_PacketStartCodePrefix = 0;
  m_StreamId = 0;
  m_PacketLength = 0;
}

int32_t xPES_PacketHeader::Parse(const uint8_t* Input)
{
    if (Input == nullptr) return -1;

    m_PacketStartCodePrefix = (Input[0] << 16) | (Input[1] << 8) | Input[2];
    if (m_PacketStartCodePrefix != 0x000001) {
        return -1;
    }

    m_StreamId = Input[3];

    m_PacketLength = (Input[4] << 8) | Input[5];

    return 6; 
}


/// @brief Print all PES packet header fields
void xPES_PacketHeader::Print() const
{
  std::cout << "PES: ";
  std::cout << "PSCP=" << (int)m_PacketStartCodePrefix << " ";
  std::cout << "SID=" << (int)m_StreamId << " ";
  std::cout << "L=" << (int)m_PacketLength << " ";
}

xPES_Assembler::xPES_Assembler()
{
    m_PID = -1; 
    m_Buffer = nullptr; 
    m_BufferSize = 0;
    m_DataOffset = 0;
    m_LastContinuityCounter = -1; 
    m_Started = false; 
}

xPES_Assembler::~xPES_Assembler()
{
    if (m_Buffer != nullptr) {
        delete[] m_Buffer;
        m_Buffer = nullptr;
    }
}

void xPES_Assembler::Init(int32_t PID)
{
    m_PID = PID;
    xBufferReset(); 
    m_LastContinuityCounter = -1; 
    m_Started = false; 
}

xPES_Assembler::eResult xPES_Assembler::AbsorbPacket(const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField)
{
    if (TransportStreamPacket == nullptr || PacketHeader == nullptr) {
        return eResult::StreamPackedLost;
    }

    if (PacketHeader->getPID() != m_PID) {
        return eResult::UnexpectedPID;
    }

    if (PacketHeader->hasPayload()) { 
        if (m_LastContinuityCounter != -1) { 
            uint8_t expected_cc = (m_LastContinuityCounter + 1) % 16;
            if (PacketHeader->getContinuityCounter() != expected_cc && PacketHeader->getTransportScramblingControl() == 0x00) {
                 m_Started = false; 
                 m_LastContinuityCounter = PacketHeader->getContinuityCounter(); 
                 return eResult::StreamPackedLost;
            }
        }
        m_LastContinuityCounter = PacketHeader->getContinuityCounter();
    }

    const uint8_t* payloadStart = TransportStreamPacket + xTS::TS_HeaderLength;
    int32_t payloadLength = xTS::TS_PacketLength - xTS::TS_HeaderLength;

    if (PacketHeader->hasAdaptationField()) {
        if (AdaptationField == nullptr) { 
            return eResult::StreamPackedLost;
        }
        int32_t af_bytes_parsed = AdaptationField->getNumBytes(); 
        payloadStart += af_bytes_parsed;
        payloadLength -= af_bytes_parsed;
    }

    if (PacketHeader->getPayloadUnitStartIndicator() == 1) {
        if (m_Started) {
            m_DataOffset = m_BufferSize; 
            xBufferReset(); 
            m_Started = true; 
            if (m_DataOffset > 0) { 
                m_DataOffset = 0; 
            }
        }
        xBufferReset(); 
        m_Started = true; 
    } else {
        if (!m_Started) {
            if (payloadLength > 0) {
                 return eResult::StreamPackedLost; 
            }
            return eResult::AssemblingContinue; 
        }
    }
    if (payloadLength > 0) {
        xBufferAppend(payloadStart, payloadLength);
    }

    if (m_PESH.getPacketStartCodePrefix() != 0x000001 && m_BufferSize >= xTS::PES_HeaderLength) {
        if (m_PESH.Parse(m_Buffer) == xTS::PES_HeaderLength) {
            m_DataOffset = xTS::PES_HeaderLength; 
        } else {
            xBufferReset();
            m_Started = false;
            return eResult::StreamPackedLost;
        }
    }

    if (m_PESH.getPacketStartCodePrefix() == 0x000001) {
        if (m_PESH.getPacketLength() != 0) {
            if (m_BufferSize >= (m_PESH.getPacketLength() + xTS::PES_HeaderLength)) {
                m_DataOffset = m_PESH.getPacketLength() + xTS::PES_HeaderLength; 
                m_Started = false; 
                return eResult::AssemblingFinished;
            }
        } else {
        }
    }

    if (PacketHeader->getPayloadUnitStartIndicator() == 1 && m_Started) {
        return eResult::AssemblingStarted;
    }

    return eResult::AssemblingContinue; 
}

void xPES_Assembler::xBufferReset()
{
  m_Buffer = 0;
  m_BufferSize = 0;
  m_DataOffset = 0;
}

void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size)
{
    if (Data == nullptr || Size <= 0) return;

    uint8_t* newBuffer = new uint8_t[m_BufferSize + Size];
    if (newBuffer == nullptr) {
        std::cerr << "Error: Failed to allocate memory for PES buffer." << std::endl;
        return;
    }

    if (m_Buffer != nullptr) {
        memcpy(newBuffer, m_Buffer, m_BufferSize);
        delete[] m_Buffer; 
    }

    memcpy(newBuffer + m_BufferSize, Data, Size);

    m_Buffer = newBuffer;
    m_BufferSize += Size;
}


//=============================================================================================================================================================================