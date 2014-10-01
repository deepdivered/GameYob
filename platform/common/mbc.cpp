#include "gameboy.h"
#include "console.h"
#include "inputhelper.h"
#include "menu.h"
#include "romfile.h"


/* MBC read handlers */

/* MBC3 */
u8 Gameboy::m3r (u16 addr) {
    if (!ramEnabled)
        return 0xff;

    switch (currentRamBank) { // Check for RTC register
        case 0x8:
            return gbClock.mbc3.s;
        case 0x9:
            return gbClock.mbc3.m;
        case 0xA:
            return gbClock.mbc3.h;
        case 0xB:
            return gbClock.mbc3.d&0xff;
        case 0xC:
            return gbClock.mbc3.ctrl;
        default: // Not an RTC register
            return memory[addr>>12][addr&0xfff];
    }
}

/* MBC7 */
u8 Gameboy::m7r (u16 addr) {
    return memory[addr>>12][addr&0xfff];
}

/* HUC3 */
u8 Gameboy::h3r (u16 addr) {
    switch (HuC3Mode) {
        case 0xc:
            return HuC3Value;
        case 0xb:
        case 0xd:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
    }
    return (ramEnabled) ? memory[addr>>12][addr&0xfff] : 0xff;
}


/* MBC Write handlers */

/* MBC0 (ROM) */
void Gameboy::m0w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* MBC1 */
void Gameboy::m1w (u16 addr, u8 val) {
    int newBank;

    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x1f;
            if (rockmanMapper)
                newBank = ((val > 0xf) ? val - 8 : val);
            else
                newBank = (romBank & 0xe0) | val;
            refreshRomBank((newBank) ? newBank : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) {
                newBank = (romBank & 0x1F) | (val<<5);
                refreshRomBank((newBank) ? newBank : 1);
            }
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* MBC2 */
void Gameboy::m2w(u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val&0xf);
            break;
    }
}

/* MBC3 */
void Gameboy::m3w(u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x7f;
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            /* The RTC register is selected by writing values 0x8-0xc, ram banks
             * are selected by values 0x0-0x3 */
            if (val <= 0x3)
                refreshRamBank(val);
            else if (val >= 8 && val <= 0xc)
                currentRamBank = val;
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            if (val)
                latchClock();
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (!ramEnabled)
                break;

            switch (currentRamBank) { // Check for RTC register
                case 0x8:
                    if (gbClock.mbc3.s != val) {
                        gbClock.mbc3.s = val;
                        writeClockStruct();
                    }
                    return;
                case 0x9:
                    if (gbClock.mbc3.m != val) {
                        gbClock.mbc3.m = val;
                        writeClockStruct();
                    }
                    return;
                case 0xA:
                    if (gbClock.mbc3.h != val) {
                        gbClock.mbc3.h = val;
                        writeClockStruct();
                    }
                    return;
                case 0xB:
                    if ((gbClock.mbc3.d&0xff) != val) {
                        gbClock.mbc3.d &= 0x100;
                        gbClock.mbc3.d |= val;
                        writeClockStruct();
                    }
                    return;
                case 0xC:
                    if (gbClock.mbc3.ctrl != val) {
                        gbClock.mbc3.d &= 0xFF;
                        gbClock.mbc3.d |= (val&1)<<8;
                        gbClock.mbc3.ctrl = val;
                        writeClockStruct();
                    }
                    return;
                default: // Not an RTC register
                    if (numRamBanks)
                        writeSram(addr&0x1fff, val);
            }
            break;
    }
}

void Gameboy::writeClockStruct() {
    if (autoSavingEnabled) {
        file_seek(saveFile, numRamBanks*0x2000, SEEK_SET);
        file_write(&gbClock, 1, sizeof(gbClock), saveFile);
        saveModified = true;
    }
}


/* MBC5 */
void Gameboy::m5w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
            refreshRomBank((romBank & 0x100) |  val);
            break;
        case 0x3:
            refreshRomBank((romBank & 0xff ) | (val&1) << 8);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 0xf;
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if (romFile->hasRumble()) {
                if (rumbleStrength) {
                    if (rumbleInserted) {
                        rumbleValue = (val & 0x8) ? 1 : 0;
                        if (rumbleValue != lastRumbleValue)
                        {
                            doRumble(rumbleValue);
                            lastRumbleValue = rumbleValue;
                        }
                    }
                }

                val &= 0x07;
            }

            refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* MBC7 */
void Gameboy::m7w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
            refreshRomBank((romBank & 0x100) |  val);
            break;
        case 0x3:
            refreshRomBank((romBank & 0xff ) | (val&1) << 8);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 0xf;
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if (romFile->hasRumble()) {
                if (rumbleStrength) {
                    if (rumbleInserted) {
                        rumbleValue = (val & 0x8) ? 1 : 0;
                        if (rumbleValue != lastRumbleValue)
                        {
                            doRumble(rumbleValue);
                            lastRumbleValue = rumbleValue;
                        }
                    }
                }

                val &= 0x07;
            }

            refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* HUC1 */
void Gameboy::h1w(u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank(val & 0x3f);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) 
                refreshRomBank(val);
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* HUC3 */

void Gameboy::h3w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            HuC3Mode = val;
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            refreshRamBank(val & 0xf);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            switch (HuC3Mode) {
                case 0xb:
                    handleHuC3Command(val);
                    break;
                case 0xc:
                case 0xd:
                case 0xe:
                    break;
                default:
                    if (ramEnabled && numRamBanks)
                        writeSram(addr&0x1fff, val);
            }
            break;
    }
}

void Gameboy::handleHuC3Command (u8 cmd) 
{
    switch (cmd&0xf0) {
        case 0x10: /* Read clock */
            if (HuC3Shift > 24)
                break;

            switch (HuC3Shift) {
                case 0: case 4: case 8:     /* Minutes */
                    HuC3Value = (gbClock.huc3.m >> HuC3Shift) & 0xf;
                    break;
                case 12: case 16: case 20:  /* Days */
                    HuC3Value = (gbClock.huc3.d >> (HuC3Shift - 12)) & 0xf;
                    break;
                case 24:                    /* Year */
                    HuC3Value = gbClock.huc3.y & 0xf;
                    break;
            }
            HuC3Shift += 4;
            break;
        case 0x40:
            switch (cmd&0xf) {
                case 0: case 4: case 7:
                    HuC3Shift = 0;
                    break;
            }

            latchClock();
            break;
        case 0x50:
            break;
        case 0x60: 
            HuC3Value = 1;
            break;
        default:
            printLog("unhandled HuC3 cmd %02x\n", cmd);
    }
}