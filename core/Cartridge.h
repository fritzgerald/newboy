#define GB_CARTRIDGE_SUCCESS    0
#define GB_CARTRIDGE_FILE_ERROR 1
#define GB_CARTRIDGE_NOSAVE_ERROR 2

#define GB_CARTRIDGE_NAME     0x0134
#define GB_CARTRIDGE_TYPE     0x0147
#define GB_CARTRIDGE_ROM_SIZE 0x0148
#define GB_CARTRIDGE_RAM_SIZE 0x0149

#define GBCART_NO_MBC     0x00

#define GBCART_MBC1       0x01
#define GBCART_MBC1_ram   0x02
#define GBCART_MBC1_ram_b 0x03

#define GBCART_MBC2       0x05
#define GBCART_MBC2_b     0x06

#define GBCART_MMM01       0x0B
#define GBCART_MMM01_ram   0x0C
#define GBCART_MMM01_ram_b 0x0D

#define GBCART_MBC3_t_b     0x0F
#define GBCART_MBC3_t_ram_b 0x10
#define GBCART_MBC3         0x11
#define GBCART_MBC3_ram     0x12
#define GBCART_MBC3_ram_b   0x13

#define GBCART_MBC5          0x19
#define GBCART_MBC5_RAM      0x1A
#define GBCART_MBC5_RAM_B    0x1B
#define GBCART_MBC5_RU       0x1C
#define GBCART_MBC5_RU_RAM   0x1D
#define GBCART_MBC5_RU_RAM_B 0x1E

#define GBCART_MBC6          0x20
#define GBCART_MBC7          0x22

#define GBCART_CAMERA        0xFC
#define GBCART_BANDAI_TAMA5  0xFD
#define GBCART_HuC3          0xFE
#define GBCART_HuC1          0xFF
