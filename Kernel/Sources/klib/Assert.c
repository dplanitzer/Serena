//
//  Assert.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Assert.h"
#include "Bytes.h"
#include "Log.h"


// This implements a micro console that directly controls the graphics hardware.
// The reason for the direct hardware control is that we want to ensure that we
// are always able to show a useful fatal error screen no matter how broken the
// state of the kernel is. Eg we can not assume that we are able to acquire
// locks because (a) IRQs are turned off and (b) even if IRQs would still be
// turned on the VP that is holding a lock that we want may never drop that
// lock.
#define COPPER_PROG_ADDR        0x10000
#define MICRO_CONSOLE_ADDR      0x10100
#define PRINT_BUFFER_ADDR       0x10200
#define FRAMEBUFFER_BASE_ADDR   0x11000
#define PRINT_BUFFER_CAPACITY   80

#define TAB_WIDTH 8

extern const Byte font8x8_latin1[128][8];
#define GLYPH_WIDTH     8
#define GLYPH_HEIGHT    8


typedef struct _VideoConfig {
    Int16       width;
    Int16       height;
    UInt8       diw_start_h;        // display window start
    UInt8       diw_start_v;
    UInt8       diw_stop_h;         // display window stop
    UInt8       diw_stop_v;
    UInt8       ddf_start;          // data fetch start
    UInt8       ddf_stop;           // data fetch stop
    UInt8       ddf_mod;            // number of padding bytes stored in memory between scan lines
    UInt16      bplcon0;            // BPLCON0 template value
} VideoConfig;

// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
// DDFSTART = low res: DDIWSTART / 2 - 8; high res: DDIWSTART / 2 - 4
// DDFSTOP = low res: DDFSTART + 8*(nwords - 2); high res: DDFSTART + 4*(nwords - 2)
static const VideoConfig kVidConfig_NTSC_640_200_60 = {
        640, 200, 0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 0, 0x8200
    };
static const VideoConfig kVidConfig_PAL_640_256_50 = {
        640, 256, 0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 0, 0x8200
    };


typedef struct _MicroConsole {
    const VideoConfig* _Nonnull config;
    Byte* _Nonnull              framebuffer;
    Int                         bytesPerRow;
    Int                         cols;
    Int                         rows;
    Int                         x;
    Int                         y;
} MicroConsole;


// Initializes the graphics device enough to run our micro console on it
static void micro_console_init_gfx(const VideoConfig* _Nonnull pConfig, Byte* _Nonnull pFramebuffer)
{
    CopperInstruction* pCode = (CopperInstruction*)COPPER_PROG_ADDR;
    CopperInstruction* ip = pCode;
    const UInt32 bplpt = (UInt32)pFramebuffer;
    
    // BPLCONx
    *ip++ = COP_MOVE(BPLCON0, pConfig->bplcon0 | ((UInt16)1) << 12);
    *ip++ = COP_MOVE(BPLCON1, 0);
    *ip++ = COP_MOVE(BPLCON2, 0x0024);
    
    // DIWSTART / DIWSTOP
    *ip++ = COP_MOVE(DIWSTART, (pConfig->diw_start_v << 8) | pConfig->diw_start_h);
    *ip++ = COP_MOVE(DIWSTOP, (pConfig->diw_stop_v << 8) | pConfig->diw_stop_h);
    
    // DDFSTART / DDFSTOP
    *ip++ = COP_MOVE(DDFSTART, pConfig->ddf_start);
    *ip++ = COP_MOVE(DDFSTOP, pConfig->ddf_stop);
    
    // BPLxMOD
    *ip++ = COP_MOVE(BPL1MOD, pConfig->ddf_mod);
    *ip++ = COP_MOVE(BPL2MOD, pConfig->ddf_mod);
    
    // BPLxPT        
    *ip++ = COP_MOVE(BPL1PTH, (bplpt >> 16) & UINT16_MAX);
    *ip++ = COP_MOVE(BPL1PTL, bplpt & UINT16_MAX);

    // COLOR
    *ip++ = COP_MOVE(COLOR00, 0x036a);  // #306ab0
    *ip++ = COP_MOVE(COLOR01, 0x0fff);  // #ffffff
    for (Int i = 2, r = COLOR_BASE + i*2; i < COLOR_COUNT; i++, r += 2) {
        *ip++ = COP_MOVE(r, 0);
    }

    // DMACON
    *ip++ = COP_MOVE(DMACON, DMACONF_SETCLR | DMACONF_BPLEN);

    // End
    *ip++ = COP_END();


    // Install the Copper program
    CHIPSET_BASE_DECL(cp);

    *CHIPSET_REG_16(cp, DMACON) = DMACONF_COPEN;
    *CHIPSET_REG_32(cp, COP1LC) = (UInt32) pCode;
    *CHIPSET_REG_16(cp, COPJMP1) = 0;
    *CHIPSET_REG_16(cp, DMACON) = (DMACONF_SETCLR | DMACONF_COPEN | DMACONF_DMAEN);
}

static void micro_console_cls(MicroConsole* _Nonnull pCon)
{
    Bytes_ClearRange(pCon->framebuffer, pCon->bytesPerRow * pCon->config->height);
}

static void micro_console_init(MicroConsole* _Nonnull pCon)
{
    pCon->config = chipset_is_ntsc() ? &kVidConfig_NTSC_640_200_60 : &kVidConfig_PAL_640_256_50;
    pCon->framebuffer = (Byte*)FRAMEBUFFER_BASE_ADDR;
    pCon->bytesPerRow = pCon->config->width >> 3;
    pCon->cols = pCon->config->width / GLYPH_WIDTH;
    pCon->rows = pCon->config->height / GLYPH_HEIGHT;
    pCon->x = 0;
    pCon->y = 0;

    // Clear the screen
    micro_console_cls(pCon);

    // Initialize the graphics hardware
    micro_console_init_gfx(pCon->config, pCon->framebuffer);
}

static void micro_console_move_cursor(MicroConsole* _Nonnull pCon, Int dX, Int dY)
{
    const Int eX = pCon->cols - 1;
    const Int eY = pCon->rows - 1;
    Int x = pCon->x + dX;
    Int y = pCon->y + dY;

    if (x < 0) {
        x = eX;
        y--;
    }
    else if (x >= eX) {
        x = 0;
        y++;
    }

    if (y < 0) {
        y = 0;
    }
    else if (y >= eY) {
        y = eY;
    }

    pCon->x = x;
    pCon->y = y;
}

static void micro_console_blit_glyph(MicroConsole* _Nonnull pCon, Character ch, Int x, Int y)
{
    const Int bytesPerRow = pCon->bytesPerRow;
    const Int maxX = pCon->config->width >> 3;
    const Int maxY = pCon->config->height >> 3;

    Byte* dst = pCon->framebuffer + (y << 3) * bytesPerRow + x;
    const Byte* src = &font8x8_latin1[ch][0];
    
    *dst = *src++; dst += bytesPerRow;      // 0
    *dst = *src++; dst += bytesPerRow;      // 1
    *dst = *src++; dst += bytesPerRow;      // 2
    *dst = *src++; dst += bytesPerRow;      // 3
    *dst = *src++; dst += bytesPerRow;      // 4
    *dst = *src++; dst += bytesPerRow;      // 5
    *dst = *src++; dst += bytesPerRow;      // 6
    *dst = *src;                            // 7
}

// Prints the given character to the console.
// \param pConsole the console
// \param ch the character
static void micro_console_draw_char(MicroConsole* _Nonnull pCon, Character ch)
{
    switch (ch) {
        case '\t':
            micro_console_move_cursor(pCon, (pCon->x / TAB_WIDTH + 1) * TAB_WIDTH - pCon->x, 0);
            break;

        case '\n':
            micro_console_move_cursor(pCon, -pCon->x, 1);
            break;
            
        case '\r':
            micro_console_move_cursor(pCon, -pCon->x, 0);
            break;
            
        case 0x0c:  // FF Form feed (new page / clear screen)
            micro_console_cls(pCon);
            break;
            
        default:
            if (ch >= 32) {
                micro_console_blit_glyph(pCon, ch, pCon->x, pCon->y);
                micro_console_move_cursor(pCon, 1, 0);
            }
            break;
    }
}

static void micro_console_write(MicroConsole* _Nonnull pCon, const Character* _Nonnull pBuffer, ByteCount nBytes)
{
    const Character* pCurByte = pBuffer;
    const Character* pEndByte = pCurByte + nBytes;
    
    while (pCurByte < pEndByte) {
        micro_console_draw_char(pCon, *pCurByte++);
    }
}


////////////////////////////////////////////////////////////////////////////////

static void fprintv_micro_console_sink(MicroConsole* _Nonnull pCon, const Character* _Nonnull pBuffer, ByteCount nBytes)
{
    micro_console_write(pCon, pBuffer, nBytes);
}

void fprintv(MicroConsole* _Nonnull pCon, const Character* _Nonnull format, va_list ap)
{
    _printv((PrintSink_Func)fprintv_micro_console_sink, pCon, (Character*)PRINT_BUFFER_ADDR, PRINT_BUFFER_CAPACITY, format, ap);
}

static void fprint(MicroConsole* _Nonnull pCon, const Character* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    fprintv(pCon, format, ap);
    va_end(ap);
}


////////////////////////////////////////////////////////////////////////////////


static void stop_machine()
{
    chipset_reset();
}

_Noreturn fatal(const Character* _Nonnull format, ...)
{
    va_list ap;
    va_start(ap, format);

    stop_machine();

    MicroConsole* pCon = (MicroConsole*)MICRO_CONSOLE_ADDR;

    micro_console_init(pCon);
    fprintv(pCon, format, ap);
    va_end(ap);

    while (true);
}

_Noreturn fatalError(const Character* _Nonnull filename, Int line, Int err)
{
    stop_machine();
    fatal("Fatal Error: %d at %s:%d", err, filename, line);
}

_Noreturn fatalAbort(const Character* _Nonnull filename, Int line)
{
    stop_machine();
    fatal("Abort: %s:%d", filename, line);
}

_Noreturn fatalAssert(const Character* _Nonnull filename, Int line)
{
    stop_machine();
    fatal("Assert: %s:%d", filename, line);
}

_Noreturn _fatalException(const ExceptionStackFrame* _Nonnull pFrame)
{
    stop_machine();
    fatal("Exception: %hhx, Format %hhx, PC %p, SR %hx", pFrame->fv.vector >> 2, pFrame->fv.format, pFrame->pc, pFrame->sr);
}
