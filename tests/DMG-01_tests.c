#include "core/definitions.h"
#include "core/Device.h"
#include "core/PPU.h"
#include "core/MMU.h"
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include "testHelper.h"

int test_dmg_sound() {
    char* testRoms[] = {
        "01-registers.gb", 
        "02-len ctr.gb",
        "03-trigger.gb",
        "04-sweep.gb",
        "05-sweep details.gb",
        "06-overflow on trigger.gb",
        "07-len sweep period sync.gb",
        "08-len ctr during power.gb",
        "09-wave read while on.gb",
        "10-wave trigger while on.gb",
        "11-regs after power.gb",
        "12-wave write while on.gb",
    };
    u_int32_t crcs[] = {
        0xffffc95c, 
        0xffff5938,
        0xffffce8c,
        0xffffe73f,
        0xffff0cee,
        0xffff9a86,
        0xffffdc70,
        0xffffc360,
        0xffffa1c9,
        0xffff1d11,
        0xffff9b5a,
        0xffff8b79,
    };
    u_int64_t steps[] = {
        0x29cccc, 
        0x6d87fb,
        0x8ea30e,
        0x2bb6a6,
        0x2b79c9,
        0x29bf05,
        0x27a541,
        0x2c95fc,
        0x28112f,
        0x421609,
        0x28818c,
        0x41f7d4,
    };

    int fails = 0;
    for (int i = 0; i < 12; i++) {
        char* romName = testRoms[i];
        u_int64_t romCrcs = crcs[i];
        u_int64_t romSteps = steps[i];
        char romPath[150];
        strcpy(romPath, "testroms/dmg_sound/rom_singles/");
        strcat(romPath, romName);

        int result = testRomWithCRC(romPath, romSteps, romCrcs);
        if (result == GB_TEST_OK) {
            printf("✅ %s succeed\n", romName);
        } else {
            printf("⛔️ %s failed\n", romName);
            fails++;
        }
    }
    return fails;
}

GBTestSuite* generateDMG_sound_suite() {
    GBTestCase tet = { "test 01-registers", test_dmg_sound};
    GBTestCase tests[] = { tet };
}


int main(int argc, const char * argv[]) {
    int failTests = test_dmg_sound();
    if (failTests == 0) {
        printf("✅ test_dmg_sound succeed\n");
    } else {
        printf("⛔️ test_dmg_sound failed\n");
    }
    return 0;
}
