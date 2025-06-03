#include "tsCommon.h"
#include "tsTransportStream.h"


#include <cstdio>
#include <cstdlib>

//=============================================================================================================================================================================

int main(int argc, char* argv[], char* envp[])
{

    FILE* file = fopen(argv[1], "rb");

    if (!file)
    {
        fprintf(stderr, "couldnt open file\n");
        return EXIT_FAILURE;
    }

    xTS_PacketHeader    TS_PacketHeader;

    int32_t TS_PacketId = 0;

    const int32_t PacketSize = xTS::TS_PacketLength;

    uint8_t PacketBuffer[PacketSize];

    xPES_Assembler PES_Assembler;
    PES_Assembler.Init(136);

    //AF SECTION
    xTS_AdaptationField TS_AdaptationField;
    int licznik = 0;
    while (fread(PacketBuffer, 1, PacketSize, file) == PacketSize)
    {
        TS_PacketHeader.Reset();
        if (TS_PacketHeader.Parse(PacketBuffer) == NOT_VALID) {
            fprintf(stderr, "Invalid packet at ID: %d\n", TS_PacketId);
        }



        if (TS_PacketHeader.getPID() == 136) {
            printf("%010d ", TS_PacketId);
            TS_PacketHeader.Print();

            if (TS_PacketHeader.hasAdaptationField()) {
                TS_AdaptationField.Reset();
                TS_AdaptationField.Parse(PacketBuffer, TS_PacketHeader.getAFC());
                printf(" ");
                TS_AdaptationField.Print();
            }
            // PES assembler
            xPES_Assembler::eResult result = PES_Assembler.AbsorbPacket(
                PacketBuffer, &TS_PacketHeader, &TS_AdaptationField);

            switch (result) {
            case xPES_Assembler::eResult::StreamPackedLost:
                printf(" PcktLost");
                break;
            case xPES_Assembler::eResult::AssemblingStarted:
                printf(" Started ");
                PES_Assembler.PrintPESH();
                break;
            case xPES_Assembler::eResult::AssemblingContinue:
                printf(" Continue");
                break;
            case xPES_Assembler::eResult::AssemblingFinished:
                printf(" Finished PES: Len=%d", PES_Assembler.getNumPacketBytes());
                break;
            default:
                break;
            }

            printf("\n");
            licznik++;
        }

        TS_PacketId++;

    }

    fclose(file);

    return EXIT_SUCCESS;
}


//=============================================================================================================================================================================
