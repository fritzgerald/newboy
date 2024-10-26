#include "core/definitions.h"
#include "core/Device.h"
#include "core/PPU.h"
#include "core/MMU.h"
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include "testHelper.h"

int test_rom_files(const char* basePath, char ** romFiles, u_int32_t* crcs, u_int64_t* steps, int numberOfTests) {
    int fails = 0;
    for (int i = 0; i < numberOfTests; i++) {
        char* romName = romFiles[i];
        u_int64_t romCrcs = crcs[i];
        u_int64_t romSteps = steps[i];
        char romPath[150];
        strcpy(romPath, basePath);
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
        0xfffe93f8, 
        0xfffe9389,
        0xfffe9335,
        0xfffe949d,
        0xfffe9308,
        0xfffe821c,
        0xfffe91af,
        0xfffe8fea,
        0xfffe6d97,
        0xfffe68f6,
        0xfffe92db,
        0xfffe692f,
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

    return test_rom_files("testroms/dmg_sound/rom_singles/", testRoms, crcs, steps, 12);
}

int test_mem_timing() {
    char* testRoms[] = {
        "01-read_timing.gb",
        "02-write_timing.gb",
        "03-modify_timing.gb"
    };

    u_int32_t crcs[] = {
        0xfffe9377,
        0xfffe9356,
        0xfffe92e1

    };
    u_int64_t steps[] = {
        0x271f70, 
        0x26c075,
        0x272895
    };
    return test_rom_files("testroms/mem_timing/rom_singles/", testRoms, crcs, steps, 3);
}

int main(int argc, const char * argv[]) {
    int (*testFunc[]) (void) = {
        test_dmg_sound,
        test_mem_timing
    };
    char* tesNames[] = {
        "DMG sound",
        "Mem timing"
    };

    int fails = 0;
    int testLen = 2;
    for (int i = 0; i < testLen; i++) {
        printf("----------------------------\n");
        printf("Testing %s roms\n", tesNames[i]);
        printf("----------------------------\n");
        int failTests = testFunc[i]();

        printf("----------------------------\n");
        if (failTests == 0) {
            printf("✅ tests %s succeed\n", tesNames[i]);
        } else {
            printf("⛔️ test %s failed\n", tesNames[i]);
        }
        fails += failTests;
    }
    printf("----------------------------\n");
    return fails;
}
