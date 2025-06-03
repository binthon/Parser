#include "tsCommon.h"
#include "tsTransportStream.h"
#include <fstream>
#include <vector>


//=============================================================================================================================================================================

int main(int argc, char *argv[ ], char *envp[ ])
{
  const uint16_t PID_TO_ASSEMBLE = 0x88;

  //open file
  std::ifstream input("example_new.ts", std::ios::binary);

  //check if file if opened
  if(!(input.is_open())){
    return EXIT_FAILURE;
  }

  FILE* outputFile = nullptr;
  if (PID_TO_ASSEMBLE == 0x88)
  {
    outputFile = fopen("PID136.mp2", "wb");
    if(!outputFile)
    {
      input.close();
      return EXIT_FAILURE;
    }
  }

  xTS_PacketHeader    TS_PacketHeader;
  xTS_AdaptationField TS_AdaptationField;
  xPES_Assembler    PES_Assembler;
  PES_Assembler.Init(PID_TO_ASSEMBLE);

  std::vector<uint8_t> TS_PacketBuffer(188);

  int32_t TS_PacketId = 0;
  while(!input.eof())
  {
    //read from file
    input.read(reinterpret_cast<char*>(TS_PacketBuffer.data()), TS_PacketBuffer.size());

    TS_PacketHeader.Reset();
    TS_AdaptationField.Reset();
    TS_PacketHeader.Parse(TS_PacketBuffer.data());
    if(TS_PacketHeader.hasAdaptationField())
    {
      TS_AdaptationField.Parse(TS_PacketBuffer.data());
    }

    printf("%010d ", TS_PacketId);
    TS_PacketHeader.Print();
    if(TS_PacketHeader.hasAdaptationField())
    {
      TS_AdaptationField.Print();
    }

    xPES_Assembler::eResult Result = PES_Assembler.AbsorbPacket(TS_PacketBuffer.data(), &TS_PacketHeader, TS_PacketHeader.hasAdaptationField() ? &TS_AdaptationField : nullptr);
    
    switch(Result)
    {
      case xPES_Assembler::eResult::StreamPackedLost : printf("PcktLost ");
      break;
      case xPES_Assembler::eResult::AssemblingStarted : printf("Started ");
      PES_Assembler.PrintPESH();
      break;
      case xPES_Assembler::eResult::AssemblingContinue : printf("Continue "); 
      break;
      case xPES_Assembler::eResult::AssemblingFinished : printf("Finished ");
      printf("PES: Len=%d", PES_Assembler.getNumPacketBytes());
      if(outputFile && PES_Assembler.getPacket() && PES_Assembler.getNumPacketBytes() > 0)
      {
        fwrite(PES_Assembler.getPacket(), 1, PES_Assembler.getNumPacketBytes(), outputFile);
      } 
      break;
      default: break;
    }
    int32_t totalHeaderLength = xTS::TS_HeaderLength;
    if(TS_PacketHeader.hasAdaptationField())
    {
      totalHeaderLength += TS_AdaptationField.getNumBytes();
    }
    int32_t payloadLength = xTS::TS_PacketLength - totalHeaderLength;
    printf(" HLen=%d PLen=%d", totalHeaderLength, payloadLength);
    printf("\n");

    TS_PacketId++;
  }
  
  //close file
  input.close();
  if (outputFile)
  {
    fclose(outputFile);
  }
  return EXIT_SUCCESS;
}

//=============================================================================================================================================================================