#include "tsTransportStream.h"
#include <cstring>  // dla memcpy
//=============================================================================================================================================================================
// xTS_PacketHeader
//=============================================================================================================================================================================



/// @brief Reset - reset all TS packet header fields
void xTS_PacketHeader::Reset()
{
	m_SB = 0;
	m_E = 0;
	m_S = 0;
	m_T = 0;
	m_PID = 0;
	m_TSC = 0;
	m_AFC = 0;
	m_CC = 0;
}

/**
  @brief Parse all TS packet header fields
  @param Input is pointer to buffer containing TS packet
  @return Number of parsed bytes (4 on success, -1 on failure)
 */
int32_t xTS_PacketHeader::Parse(const uint8_t* Input)
{
	const uint32_t* HeadPtr = (const uint32_t*)Input;
	uint32_t Head = xSwapBytes32(*HeadPtr);

	m_SB = (Head & 0b11111111000000000000000000000000) >> 24; // w ten sposob
	if (m_SB != 0x47) return NOT_VALID;

	m_E = (Head & 0b00000000100000000000000000000000) >> 23;
	m_S = (Head & 0b00000000010000000000000000000000) >> 22;
	m_T = (Head & 0b00000000001000000000000000000000) >> 21;
	m_PID = (Head & 0b00000000000111111111111100000000) >> 8;
	m_TSC = (Head & 0b00000000000000000000000011000000) >> 6;
	m_AFC = (Head & 0b00000000000000000000000000110000) >> 4;
	m_CC = (Head & 0b00000000000000000000000000001111);

	return 4;
}

/// @brief Print all TS packet header fields
void xTS_PacketHeader::Print() const
{
	printf("SB=%d E=%d S=%d T=%d PID=%d TSC=%d AFC=%d CC=%d",
		m_SB, m_E, m_S, m_T, m_PID, m_TSC, m_AFC, m_CC);
}

//=============================================================================================================================================================================
//AF PARSER SECTION
//=============================================================================================================================================================================

/// @brief Reset - reset all TS packet header fields
void xTS_AdaptationField::Reset()
{
	m_AdaptationFieldControl = 0;
	m_AdaptationFieldLength = 0;
	m_Discontinuity = m_RandomAccess = m_ElementaryStreamPriority = m_PCR_flag = m_OPCR_flag = m_SplicingPointFlag = m_TransportPrivateDataFlag = m_AdaptationFieldExtensionFlag = 0;
	PCR_base = OPCR_base = PCR_extension = OPCR_extension = 0;
}
/**
@brief Parse adaptation field
@param PacketBuffer is pointer to buffer containing TS packet
@param AdaptationFieldControl is value of Adaptation Field Control field of
corresponding TS packet header
@return Number of parsed bytes (length of AF or -1 on failure)
*/
int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl)
{
	m_AdaptationFieldControl = AdaptationFieldControl;
	m_AdaptationFieldLength = PacketBuffer[4]; // because header is 32 bits long

	uint8_t AF = PacketBuffer[5];

	m_Discontinuity = (AF & 0b10000000) >> 7;
	m_RandomAccess = (AF & 0b01000000) >> 6;
	m_ElementaryStreamPriority = (AF & 0b00100000) >> 5;
	m_PCR_flag = (AF & 0b00010000) >> 4;
	m_OPCR_flag = (AF & 0b00001000) >> 3;
	m_SplicingPointFlag = (AF & 0b00000100) >> 2;
	m_TransportPrivateDataFlag = (AF & 0b00000010) >> 1;
	m_AdaptationFieldExtensionFlag = (AF & 0b00000001);

	// return 1; // parsed AF
	uint8_t offset = 6;

	if (m_PCR_flag == 1) {
		PCR_base = PCR_base | (PacketBuffer[offset] << 25);
		PCR_base = PCR_base | (PacketBuffer[offset + 1] << 17);
		PCR_base = PCR_base | (PacketBuffer[offset + 2] << 9);
		PCR_base = PCR_base | (PacketBuffer[offset + 3] << 1);
		PCR_base = PCR_base | ((PacketBuffer[offset + 4] >> 7) & 0b1);

		PCR_extension = (PacketBuffer[offset + 4] & 0b1) << 8;
		PCR_extension = PCR_extension | PacketBuffer[offset + 5];

		PCR = (PCR_base * xTS::BaseToExtendedClockMultiplier) + PCR_extension;

		offset = offset + 6;
	}

	if (m_OPCR_flag == 1) {
		OPCR_base = OPCR_base | (PacketBuffer[offset] << 25);
		OPCR_base = OPCR_base | (PacketBuffer[offset + 1] << 17);
		OPCR_base = OPCR_base | (PacketBuffer[offset + 2] << 9);
		OPCR_base = OPCR_base | (PacketBuffer[offset + 3] << 1);
		OPCR_base = OPCR_base | ((PacketBuffer[offset + 4] >> 7) & 0b1);

		OPCR_extension = (PacketBuffer[offset + 4] & 0b1) << 8;
		OPCR_extension = OPCR_extension | PacketBuffer[offset + 5];

		OPCR = (OPCR_base * xTS::BaseToExtendedClockMultiplier) + OPCR_extension;
		offset = offset + 6;
	}

	if (m_SplicingPointFlag == 1) {
		SpliceCountDown = PacketBuffer[offset];
		offset = offset + 1;
	}

	if (m_TransportPrivateDataFlag) {
		uint8_t TransportPrivateDataLength = PacketBuffer[offset];
		offset = offset + 2 + TransportPrivateDataLength;
	}

	if (m_AdaptationFieldExtensionFlag) {
		uint8_t AdaptationFieldExtensionLength = PacketBuffer[offset];
		offset = offset + 1 + AdaptationFieldExtensionLength;
	}


	//calculating stuffing bytes (-1 cuz offset -4 cuz header)
	StuffingBytes = m_AdaptationFieldLength - (offset - 1 - 4);

	return 1;
}
/// @brief Print all TS packet header fields
void xTS_AdaptationField::Print() const
{
	printf("AF: L=%3d DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
		m_AdaptationFieldLength, m_Discontinuity, m_RandomAccess, m_ElementaryStreamPriority, m_PCR_flag, m_OPCR_flag, m_SplicingPointFlag, m_TransportPrivateDataFlag, m_AdaptationFieldExtensionFlag);

	if (m_PCR_flag == 1) {
		double PCR_time = (double)PCR / xTS::ExtendedClockFrequency_Hz;
		printf(" PCR=%08u (Time=%.6lfs)", PCR, PCR_time);
	}

	if (m_OPCR_flag == 1) {
		double OPCR_time = (double)OPCR / xTS::ExtendedClockFrequency_Hz;
		printf(" OPCR=%08X (Time=%.6lfs)", OPCR, OPCR_time);
	}

	if (StuffingBytes > 0) {
		printf(" Stuffing=%u", StuffingBytes);
	}
	else
	{
		printf("Stuffing = 0");
	}

}
//=============================================================================================================================================================================
//PES//

void xPES_PacketHeader::Reset()
{
	m_PacketStartCodePrefix = 0;
	m_StreamId = 0;
	m_PacketLength = 0;
	m_PES_header_data_length = 0;
}

int32_t xPES_PacketHeader::Parse(const uint8_t* Input, uint32_t DataLength)
{
	if (DataLength < 6) return NOT_VALID;

	// Parsowanie podstawowych 6 bajtow
	m_PacketStartCodePrefix = (Input[0] << 16) | (Input[1] << 8) | Input[2];
	m_StreamId = Input[3];
	m_PacketLength = (Input[4] << 8) | Input[5];

	// Sprawdzenie czy to prawidlowy start code
	if (m_PacketStartCodePrefix != 0x000001) {
		return NOT_VALID;
	}

	int32_t offset = 6;

	// Sprawdzenie czy stream_id wskazuje na obecnosc dodatkowych pol
	if (m_StreamId != eStreamId_program_stream_map &&
		m_StreamId != eStreamId_padding_stream &&
		m_StreamId != eStreamId_private_stream_2 &&
		m_StreamId != eStreamId_ECM &&
		m_StreamId != eStreamId_EMM &&
		m_StreamId != eStreamId_program_stream_directory &&
		m_StreamId != eStreamId_DSMCC_stream &&
		m_StreamId != eStreamId_ITUT_H222_1_type_E) {

		if (DataLength < offset + 3) return NOT_VALID;


		// Dlugosc danych naglowka
		m_PES_header_data_length = Input[offset + 2];



	}

	return offset; //tu jest cos zle chyba nie ten offset mam zwracac
}

int32_t xPES_PacketHeader::getHeaderLength() const
{
	if (m_StreamId == eStreamId_program_stream_map ||
		m_StreamId == eStreamId_padding_stream ||
		m_StreamId == eStreamId_private_stream_2 ||
		m_StreamId == eStreamId_ECM ||
		m_StreamId == eStreamId_EMM ||
		m_StreamId == eStreamId_program_stream_directory ||
		m_StreamId == eStreamId_DSMCC_stream ||
		m_StreamId == eStreamId_ITUT_H222_1_type_E) {
		return 6; // Tylko podstawowe pola
	}

	return 9 + m_PES_header_data_length; // 6 + 3 + dlugosc dodatkowych danych
}

void xPES_PacketHeader::Print() const
{
	printf("PES: PSCP=%d SID=%d L=%d",
		(m_PacketStartCodePrefix == 0x000001) ? 1 : 0,
		m_StreamId,
		m_PacketLength);
}

//=============================================================================================================================================================================
// xPES_Assembler
//=============================================================================================================================================================================

xPES_Assembler::xPES_Assembler()
{
	m_PID = -1;
	m_Buffer = nullptr;
	m_BufferSize = 0;
	m_DataOffset = 0;
	m_LastContinuityCounter = -1;
	m_Started = false;
	m_OutputFile = nullptr;
}

xPES_Assembler::~xPES_Assembler()
{
	if (m_Buffer) {
		delete[] m_Buffer;
	}
	if (m_OutputFile) {
		fclose(m_OutputFile);
	}
}

void xPES_Assembler::Init(int32_t PID)
{
	m_PID = PID;
	m_BufferSize = 65536; // 64KB na pakiet PES bo 2^16
	m_Buffer = new uint8_t[m_BufferSize];
	xBufferReset();

	char filename[256];
	sprintf(filename, "PID%d.mp2", PID);
	m_OutputFile = fopen(filename, "wb");
	if (!m_OutputFile) {
		printf("Error: Cannot create output file %s\n", filename);
	}
	else {
		printf("Writing audio data to: %s\n", filename);
	}
}

void xPES_Assembler::xBufferReset()
{
	m_DataOffset = 0;
	m_Started = false;
	m_LastContinuityCounter = -1;
}

void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size)
{
	if (m_DataOffset + Size <= m_BufferSize) {
		memcpy(m_Buffer + m_DataOffset, Data, Size);
		m_DataOffset += Size;

		// write to file
		if (m_OutputFile && Size > 0) {
			fwrite(Data, 1, Size, m_OutputFile);
			fflush(m_OutputFile);  // Zapewnij zapis na dysk
		}
	}
}


int32_t xPES_Assembler::calculatePayloadOffset(const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField)
{
	int32_t offset = xTS::TS_HeaderLength; // 4 bajty naglowka TS

	if (PacketHeader->hasAdaptationField()) {
		offset += 1 + AdaptationField->getAdaptationFieldLength(); // 1 bajt dlugosci + AF
	}
	//tutaj nie wiem czy moze byc sytuacja ze w ogole nie ma payloaud?
	return offset;
}

xPES_Assembler::eResult xPES_Assembler::AbsorbPacket(const uint8_t* TransportStreamPacket,
	const xTS_PacketHeader* PacketHeader,
	const xTS_AdaptationField* AdaptationField)
{
	// Sprawdzenie PID
	if (PacketHeader->getPID() != m_PID) {
		return eResult::UnexpectedPID;
	}

	// Obliczenie offsetu do payload
	int32_t payloadOffset = calculatePayloadOffset(PacketHeader, AdaptationField);
	int32_t payloadSize = xTS::TS_PacketLength - payloadOffset;

	if (PacketHeader->getS()) { // Payload Unit Start Indicator = NOWY PAKIET PES

		// NOWY PAKIET PES - resetuj wszystko
		m_PESH.Reset();
		if (m_PESH.Parse(TransportStreamPacket + payloadOffset, payloadSize) == NOT_VALID) {
			m_DataOffset = 0;
			m_Started = false;
			m_LastContinuityCounter = -1;
			return eResult::StreamPackedLost;
		}

		payloadSize = payloadSize - m_PESH.getHeaderLength();

		// Reset bufora, ale ZACHOWAJ CC
		m_DataOffset = 0;
		m_Started = true;
		m_LastContinuityCounter = PacketHeader->getCC();

		xBufferAppend(TransportStreamPacket + payloadOffset + m_PESH.getHeaderLength(), payloadSize);

		return eResult::AssemblingStarted;
	}
	else { // KONTYNUACJA PAKIETU PES

		// Sprawdzenie czy started i CC
		if (m_Started) {
			int8_t expectedCC = (m_LastContinuityCounter + 1) % 16;
			if (PacketHeader->getCC() != expectedCC) {
				m_DataOffset = 0;
				m_Started = false;
				m_LastContinuityCounter = -1;
				return eResult::StreamPackedLost;
			}
		}
		else {
			return eResult::StreamPackedLost;
		}

		m_LastContinuityCounter = PacketHeader->getCC();
		xBufferAppend(TransportStreamPacket + payloadOffset, payloadSize);

		// Sprawdzenie kompletnoœci
		if (m_PESH.getPacketLength() > 0) {
			int32_t expectedTotalLength = m_PESH.getPacketLength() - (m_PESH.getHeaderLength() - 6);
			if (m_DataOffset >= expectedTotalLength) {
				return eResult::AssemblingFinished;
			}
		}

		return eResult::AssemblingContinue;
	}
}