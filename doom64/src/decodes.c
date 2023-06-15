/* decodes.c */

#include "doomdef.h"

#include "graph.h"

/*=======*/
/* TYPES */
/*=======*/

typedef struct
{
    int var0;
    int var1;
    int var2;
    int var3;
    byte *write;
    byte *writePos;
    byte *read;
    byte *readPos;
} decoder_t;

/*=========*/
/* GLOBALS */
/*=========*/

static short ShiftTable[6] = {4, 6, 8, 10, 12, 14};

static int tableVar01[18];

static short DecodeTable[2524];
static short array01[1258];

static decoder_t decoder;
static byte *allocPtr;

static int OVERFLOW_READ;
static int OVERFLOW_WRITE;

/*
============================================================================

DECODE BASED ROUTINES

============================================================================
*/

/*
========================
=
= GetDecodeByte
=
========================
*/
static byte GetDecodeByte(void)
{
    if ((int)(decoder.readPos - decoder.read) >= OVERFLOW_READ)
    {
        return -1;
    }
    return *decoder.readPos++;
}

/*
========================
=
= WriteOutput
=
========================
*/
static void WriteOutput(byte outByte)
{
    if ((int)(decoder.writePos - decoder.write) >= OVERFLOW_WRITE)
    {
        I_Error("Overflowed output buffer");
    }
    *decoder.writePos++ = outByte;
}

/*
========================
=
= DecodeScan
=
========================
*/
static int DecodeScan(void)
{
    int resultbyte;

    resultbyte = decoder.var0;

    decoder.var0 = (resultbyte - 1);
    if ((resultbyte < 1))
    {
        resultbyte = GetDecodeByte();

        decoder.var1 = resultbyte;
        decoder.var0 = 7;
    }

    resultbyte = (0 < (decoder.var1 & 0x80));
    decoder.var1 = (decoder.var1 << 1);

    return resultbyte;
}

/*
========================
=
= RescanByte
=
========================
*/

static int RescanByte(int byte)
{
    int shift;
    int i;
    int resultbyte;

    resultbyte = 0;
    i = 0;
    shift = 1;

    if (byte <= 0)
    {
        return resultbyte;
    }
    do
    {
        if (DecodeScan() != 0)
        {
            resultbyte |= shift;
        }
        i++;
        shift = (shift << 1);
    } while (i != byte);

    return resultbyte;
}

/*
========================
=
= InitDecodeTable
=
========================
*/
static void InitDecodeTable(void)
{
    int evenVal, oddVal, incrVal;

    short *curArray;
    short *incrTbl;
    short *evenTbl;
    short *oddTbl;

    tableVar01[15] = 3;
    tableVar01[16] = 0;
    tableVar01[17] = 0;

    decoder.var0 = 0;
    decoder.var1 = 0;
    decoder.var2 = 0;
    decoder.var3 = 0;

    curArray = &array01[2];
    incrTbl = &DecodeTable[0x4F2];

    incrVal = 2;

    do
    {
        if (incrVal < 0)
        {
            *incrTbl = (short)((incrVal + 1) >> 1);
        }
        else
        {
            *incrTbl = (short)(incrVal >> 1);
        }

        *curArray++ = 1;
        incrTbl++;
    } while (++incrVal < 1258);

    oddTbl = &DecodeTable[0x279];
    evenTbl = &DecodeTable[1];

    evenVal = 2;
    oddVal = 3;

    do
    {
        *oddTbl++ = (short)oddVal;
        oddVal += 2;

        *evenTbl++ = (short)evenVal;
        evenVal += 2;

    } while (oddVal < 1259);

    tableVar01[0] = 0;

    incrVal = (1 << ShiftTable[0]);
    tableVar01[6] = (incrVal - 1);
    tableVar01[1] = incrVal;

    incrVal += (1 << ShiftTable[1]);
    tableVar01[7] = (incrVal - 1);
    tableVar01[2] = incrVal;

    incrVal += (1 << ShiftTable[2]);
    tableVar01[8] = (incrVal - 1);
    tableVar01[3] = incrVal;

    incrVal += (1 << ShiftTable[3]);
    tableVar01[9] = (incrVal - 1);
    tableVar01[4] = incrVal;

    incrVal += (1 << ShiftTable[4]);
    tableVar01[10] = (incrVal - 1);
    tableVar01[5] = incrVal;

    incrVal += (1 << ShiftTable[5]);
    tableVar01[11] = (incrVal - 1);
    tableVar01[12] = (incrVal - 1);

    tableVar01[13] = tableVar01[12] + 64;
}

/*
========================
=
= CheckTable
=
========================
*/
static void CheckTable(int a0, int a1, int a2)
{
    int i;
    int idByte1;
    int idByte2;
    short *curArray;
    short *evenTbl;
    short *oddTbl;
    short *incrTbl;

    i = 0;
    evenTbl = &DecodeTable[0];
    oddTbl = &DecodeTable[0x278];
    incrTbl = &DecodeTable[0x4F0];

    idByte1 = a0;

    do
    {
        idByte2 = incrTbl[idByte1];

        array01[idByte2] = (array01[a1] + array01[a0]);

        a0 = idByte2;

        if (idByte2 != 1)
        {
            idByte1 = incrTbl[idByte2];
            idByte2 = evenTbl[idByte1];

            a1 = idByte2;

            if (a0 == idByte2)
            {
                a1 = oddTbl[idByte1];
            }
        }

        idByte1 = a0;
    } while (a0 != 1);

    if (array01[1] != 0x7D0)
    {
        return;
    }

    array01[1] >>= 1;

    curArray = &array01[2];
    do
    {
        curArray[3] >>= 1;
        curArray[2] >>= 1;
        curArray[1] >>= 1;
        curArray[0] >>= 1;
        curArray += 4;
        i += 4;
    } while (i != 1256);
}

/*
========================
=
= DecodeByte
=
========================
*/
static void DecodeByte(int tblpos)
{
    int incrIdx;
    int evenVal;
    int idByte1;
    int idByte2;
    int idByte3;
    int idByte4;

    short *evenTbl;
    short *oddTbl;
    short *incrTbl;
    short *tmpIncrTbl;

    evenTbl = &DecodeTable[0];
    oddTbl = &DecodeTable[0x278];
    incrTbl = &DecodeTable[0x4F0];

    idByte1 = (tblpos + 0x275);
    array01[idByte1] += 1;

    if (incrTbl[idByte1] != 1)
    {
        tmpIncrTbl = &incrTbl[idByte1];
        idByte2 = *tmpIncrTbl;

        if (idByte1 == evenTbl[idByte2])
        {
            CheckTable(idByte1, oddTbl[idByte2], idByte1);
        }
        else
        {
            CheckTable(idByte1, evenTbl[idByte2], idByte1);
        }

        do
        {
            incrIdx = incrTbl[idByte2];
            evenVal = evenTbl[incrIdx];

            if (idByte2 == evenVal)
            {
                idByte3 = oddTbl[incrIdx];
            }
            else
            {
                idByte3 = evenVal;
            }

            if (array01[idByte3] < array01[idByte1])
            {
                if (idByte2 == evenVal)
                {
                    oddTbl[incrIdx] = (short)idByte1;
                }
                else
                {
                    evenTbl[incrIdx] = (short)idByte1;
                }

                evenVal = evenTbl[idByte2];

                if (idByte1 == evenVal)
                {
                    idByte4 = oddTbl[idByte2];
                    evenTbl[idByte2] = (short)idByte3;
                }
                else
                {
                    idByte4 = evenVal;
                    oddTbl[idByte2] = (short)idByte3;
                }

                incrTbl[idByte3] = (short)idByte2;

                *tmpIncrTbl = (short)incrIdx;
                CheckTable(idByte3, idByte4, idByte4);

                tmpIncrTbl = &incrTbl[idByte3];
            }

            idByte1 = *tmpIncrTbl;
            tmpIncrTbl = &incrTbl[idByte1];

            idByte2 = *tmpIncrTbl;
        } while (idByte2 != 1);
    }
}

/*
========================
=
= StartDecodeByte
=
========================
*/
static int StartDecodeByte(void)
{
    int lookup;
    short *evenTbl;
    short *oddTbl;

    lookup = 1;

    evenTbl = &DecodeTable[0];
    oddTbl = &DecodeTable[0x278];

    while (lookup < 0x275)
    {
        if (DecodeScan() == 0)
        {
            lookup = evenTbl[lookup];
        }
        else
        {
            lookup = oddTbl[lookup];
        }
    }

    lookup = (lookup + -0x275);
    DecodeByte(lookup);

    return lookup;
}

/*
========================
=
= DecodeD64
=
= Exclusive Doom 64
=
========================
*/
void DecodeD64(unsigned char *input, unsigned char *output)
{
    int copyPos, storePos;
    int dec_byte, resc_byte;
    int incrBit, copyCnt, shiftPos, j;

    InitDecodeTable();

    OVERFLOW_READ = MAXINT;
    OVERFLOW_WRITE = MAXINT;

    incrBit = 0;

    decoder.read = input;
    decoder.readPos = input;
    decoder.write = output;
    decoder.writePos = output;

    allocPtr = (byte *)Z_Alloc(tableVar01[13], PU_STATIC, NULL);

    dec_byte = StartDecodeByte();

    while (dec_byte != 256)
    {
        if (dec_byte < 256)
        {
            /* Decode the data directly using binary data code */

            WriteOutput((byte)(dec_byte & 0xff));
            allocPtr[incrBit] = (byte)dec_byte;

            /* Resets the count once the memory limit is exceeded in allocPtr,
                so to speak resets it at startup for reuse */
            incrBit += 1;
            if (incrBit == tableVar01[13])
            {
                incrBit = 0;
            }
        }
        else
        {
            /* Decode the data using binary data code,
                a count is obtained for the repeated data,
                positioning itself in the root that is being stored in allocPtr previously. */

            /*  A number is obtained from a range from 0 to 5,
                necessary to obtain a shift value in the ShiftTable*/
            shiftPos = (dec_byte + -257) / 62;

            /*  get a count number for data to copy */
            copyCnt = (dec_byte - (shiftPos * 62)) + -254;

            /*  To start copying data, you receive a position number
                that you must sum with the position of table tableVar01 */
            resc_byte = RescanByte(ShiftTable[shiftPos]);

            /*  with this formula the exact position is obtained
                to start copying previously stored data */
            copyPos = incrBit - ((tableVar01[shiftPos] + resc_byte) + copyCnt);

            if (copyPos < 0)
            {
                copyPos += tableVar01[13];
            }

            storePos = incrBit;

            for (j = 0; j < copyCnt; j++)
            {
                /* write the copied data */
                WriteOutput(allocPtr[copyPos]);

                /* save copied data at current position in memory allocPtr */
                allocPtr[storePos] = allocPtr[copyPos];

                storePos++; /* advance to next allocPtr memory block to store */
                copyPos++;  /* advance to next allocPtr memory block to copy */

                /* reset the position of storePos once the memory limit is exceeded */
                if (storePos == tableVar01[13])
                {
                    storePos = 0;
                }

                /* reset the position of copyPos once the memory limit is exceeded */
                if (copyPos == tableVar01[13])
                {
                    copyPos = 0;
                }
            }

            /* Resets the count once the memory limit is exceeded in allocPtr,
                so to speak resets it at startup for reuse */
            incrBit += copyCnt;
            if (incrBit >= tableVar01[13])
            {
                incrBit -= tableVar01[13];
            }
        }

        dec_byte = StartDecodeByte();
    }

    Z_Free(allocPtr);
}

/*
== == == == == == == == == ==
=
= DecodeJaguar (decode original name)
=
= Exclusive Psx Doom / Doom 64 from Jaguar Doom
=
== == == == == == == == == ==
*/

#define WINDOW_SIZE 4096
#define LOOKAHEAD_SIZE 16
#define LENSHIFT 4 /* this must be log2(LOOKAHEAD_SIZE) */

void DecodeJaguar(unsigned char *input, unsigned char *output)
{
    int getidbyte = 0;
    int len;
    int pos;
    int i;
    unsigned char *source;
    int idbyte = 0;

    while (1)
    {
        /* get a new idbyte if necessary */
        if (!getidbyte)
        {
            idbyte = *input++;
        }
        getidbyte = (getidbyte + 1) & 7;

        if (idbyte & 1)
        {
            /* decompress */
            pos = *input++ << LENSHIFT;
            pos = pos | (*input >> LENSHIFT);
            source = output - pos - 1;
            len = (*input++ & 0xf) + 1;
            if (len == 1)
            {
                break;
            }

            i = 0;
            if (len > 0)
            {
                if ((len & 3))
                {
                    while (i != (len & 3))
                    {
                        *output++ = *source++;
                        i++;
                    }
                }
                while (i != len)
                {
                    output[0] = source[0];
                    output[1] = source[1];
                    output[2] = source[2];
                    output[3] = source[3];
                    output += 4;
                    source += 4;
                    i += 4;
                }
            }
        }
        else
        {
            *output++ = *input++;
        }

        idbyte = idbyte >> 1;
    }
}
