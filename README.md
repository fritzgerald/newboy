# New Boy
NewBoy is a Game Boy emulator written has an hobby and educational purposes in C. Although full Game Boy and Game Boy color support is the main goal there are a lot of  other stable emulator that you can choose to play without the fear crash or bug.

## Targeted Platforms

 - [x] Mac OS
 - [ ] Windows
 - [ ] Linux
 - [ ] Raspberry

## Feature list (current state)

 - [x] Game Boy (DMG) support 
 - [ ] Game Boy Color (CGB) support
 - [x] Audio generation (APU)
 - [x] Picture processing (PPU)
 - [x] Input handling
 - [ ] Load from cartridges
	 - [x] NO MBC
	 - [x] MBC1
	 - [x] MBC2
	 - [x] MBC3
	 - [ ] MBC5 
	 - [ ] MBC6
	 - [ ] MBC7
	 - [ ] MMM01
	 - [ ] M161
	 - [ ] HuC1
	 - [ ] HuC-3

## Build from sources
To compile make sure you are at the root of the project and `make <target>`.
List of targets:
 - `cocoaApp` generate the mac os app
 - `tests` run the tests
 - `clean`clean the build folder

