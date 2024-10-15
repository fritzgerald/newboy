#include "testHelper.h"
#include "core/Device.h"
#include "core/PPU.h"
#include "core/MMU.h"

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

uint8_t _crc8(uint8_t const *data, size_t nBytes, int start, int stride) {
    if (data == NULL) {
        return 0;
    }
    uint8_t coefficient = 0xb2;

    uint8_t remainder = 0;
    for (int byte = start; byte < nBytes; byte += stride) {
        remainder ^= data[byte];
        // Perform modulo-2 division, a bit at a time.
        for (uint8_t i = 0; i < 8; i++) {
            // Try to divide the current data bit.
            remainder = ((remainder & 0x1) != 0) ? (remainder >> 1) ^ coefficient : (remainder >> 1);
        }
    }
    return remainder ^ 0xFF;
}

int testRomWithCRC(char* romPath, u_int64_t steps, u_int32_t crcCheck) {

    GB_device* device = GB_newDevice();
    GB_deviceloadRom(device, romPath);
    u_int64_t testlen = steps;
    while (testlen != 0){
        testlen--;
        GB_emulationStep(device);
    }

    uint8_t crc1 = _crc8((uint8_t *)device->ppu->frameBuffer[GBBackgroundFrameBuffer], sizeof(int32_t) * 160 * 144, 0, 1);
    uint8_t crc2 = _crc8((uint8_t *)device->ppu->frameBuffer[GBBackgroundFrameBuffer], sizeof(int32_t) * 160 * 144, 0, 2);
    uint8_t crc3 = _crc8((uint8_t *)device->ppu->frameBuffer[GBBackgroundFrameBuffer], sizeof(int32_t) * 160 * 144, 1, 2);
    uint8_t crc4 = _crc8((uint8_t *)device->ppu->frameBuffer[GBObjectFrameBuffer], sizeof(int32_t) * 160 * 144, 0, 1);
   
    GB_freeDevice(device);
    if (crc1 == (crcCheck & 0xFF) &&
        crc2 == ((crcCheck >> 8) & 0xFF) && 
        crc3 == ((crcCheck >> 16) & 0xFF) &&
        crc4 == ((crcCheck >> 24) & 0xFF)) {
        return GB_TEST_OK;
    }
    return GB_TEST_FAIL;
}