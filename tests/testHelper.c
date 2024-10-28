#include "testHelper.h"
#include "core/Device.h"
#include "core/PPU.h"
#include "core/MMU.h"
#include "core/RomMBC.h"
#include "core/definitions.h"

#include <string.h>
#include <stdlib.h>

#define TEST_BLOCK_SIZE 50

GBTestSuite* GBNewTestSuite(char* name, GBTestCase* test, int testsLen) {
    GBTestSuite * suite = malloc(sizeof(GBTestSuite));

    suite->name = malloc(strlen(name)+1);
    strcpy(suite->name, name);

    suite->tests = malloc(sizeof(void*) * testsLen);
    memcpy(suite->tests, test, sizeof(void*) * testsLen);
    suite->testsLen = testsLen;

    return suite;
}

// void GBAddTestCase(GBTestSuite* suite, GBTestCase test) {
//     if ((suite->testsLen % TEST_BLOCK_SIZE) == 0) {
//         // Need to Expand array size
//         GBTestCase* newArray = malloc(sizeof(void*) * (suite->testsLen + TEST_BLOCK_SIZE));
//         memcpy(newArray, suite->tests, sizeof(void*) * suite->testsLen);
        
//         GBTestCase* oldArray = suite->tests;
//         suite->tests = newArray;
//         free(oldArray);
//     }
//     suite->tests[suite->testsLen++] = test;
// }

uint32_t checksum(uint8_t const *data, size_t nBytes, int start, int stride) {
    uint32_t remainder = 0;
    for (int byte = start; byte < nBytes; byte += stride) {
        remainder = remainder - data[byte] - 1;
    }
    return remainder;
}

int testRomWithCRC(char* romPath, uint64_t steps, uint32_t crcCheck) {

    GB_device* device = GB_newDevice();

    GBRomMBC* rom = GBNewRom(romPath);
    GBCartridgeDef *cartDef = malloc(sizeof(GBCartridgeDef));
    cartDef->sender = rom;
    cartDef->read = (GBCartrigeReadFunc)GBReadFromRom;
    cartDef->write = (GBCartridgeWriteFunc)GBWriteToRom;
    GB_emulationLoadCartdrige(device, cartDef);

    uint64_t testlen = steps;
    while (testlen != 0){
        testlen--;
        GB_emulationStep(device);
    }

    uint32_t crc = checksum((uint8_t *)device->ppu->frameBuffer[GBBackgroundFrameBuffer], sizeof(int32_t) * 160 * 144, 0, 1);
   
    GB_freeDevice(device);
    if (crc == crcCheck) {
        return GB_TEST_OK;
    }
    return GB_TEST_FAIL;
}