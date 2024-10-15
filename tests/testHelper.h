#include <stdint.h>
#include <stddef.h>

#define GB_TEST_OK 0
#define GB_TEST_FAIL 1

typedef int (*GBTestFunc)();

struct GBTestCase_s {
    char testName[100];
    GBTestFunc testFunction;
};

typedef struct GBTestCase_s GBTestCase;

struct GBTestSuite_s {
    char* name;
    GBTestCase* tests;
    int testsLen;
};

typedef struct GBTestSuite_s GBTestSuite;

GBTestSuite* GBNewTestSuite(char* name, GBTestCase* test, int testsLen);
// void GBAddTestCase(GBTestSuite* suite, GBTestCase test);

int testRomWithCRC(char* romPath, u_int64_t steps, u_int32_t crcCheck);
uint8_t _crc8(uint8_t const *data, size_t nBytes, int start, int stride);