//
//  assert.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "assert.h"
#include "Bytes.h"


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

extern const Byte font8x8_latin1[128][8];
#define GLYPH_WIDTH     8
#define GLYPH_HEIGHT    8


typedef struct _VideoConfig {
    Int16       width;
    Int16       height;
    Int8        fps;
    UInt8       diw_start_h;        // display window start
    UInt8       diw_start_v;
    UInt8       diw_stop_h;         // display window stop
    UInt8       diw_stop_v;
    UInt8       ddf_start;          // data fetch start
    UInt8       ddf_stop;           // data fetch stop
    UInt8       ddf_mod;            // number of padding bytes stored in memory between scan lines
    UInt16      bplcon0;            // BPLCON0 template value
    UInt8       spr_shift;          // Shift factors that should be applied to X & Y coordinates to convert them from screen coords to sprite coords [h:4,v:4]
} VideoConfig;

// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
// DDFSTART = low res: DDIWSTART / 2 - 8; high res: DDIWSTART / 2 - 4
// DDFSTOP = low res: DDFSTART + 8*(nwords - 2); high res: DDFSTART + 4*(nwords - 2)
static const VideoConfig kVidConfig_NTSC_640_200_60 = {
        640, 200, 60, 0x81, 0x2c, 0xc1, 0xf4, 0x3c, 0xd4, 0, 0x8200, 0x10
    };
static const VideoConfig kVidConfig_PAL_640_256_50 = {
        640, 256, 50, 0x81, 0x2c, 0xc1, 0x2c, 0x3c, 0xd4, 0, 0x8200, 0x10
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

static void micro_console_cls(MicroConsole* _Nonnull pCon);


static void micro_console_init(MicroConsole* _Nonnull pCon)
{
    pCon->config = chipset_is_ntsc() ? &kVidConfig_NTSC_640_200_60 : &kVidConfig_PAL_640_256_50;
    pCon->framebuffer = (Byte*)FRAMEBUFFER_BASE_ADDR;
    pCon->bytesPerRow = pCon->config->width >> 3;
    pCon->cols = pCon->config->width / GLYPH_WIDTH;
    pCon->rows = pCon->config->height / GLYPH_HEIGHT;
    pCon->x = 0;
    pCon->y = 0;


    // Build a Copper program so that we can display our framebuffer
    CopperInstruction* pCode = (CopperInstruction*)COPPER_PROG_ADDR;
    const VideoConfig* pConfig = pCon->config;
    const UInt32 bplpt = (UInt32)pCon->framebuffer;
    Int ip = 0;
    
    // BPLCONx
    pCode[ip++] = COP_MOVE(BPLCON0, pConfig->bplcon0 | ((UInt16)1) << 12);
    pCode[ip++] = COP_MOVE(BPLCON1, 0);
    pCode[ip++] = COP_MOVE(BPLCON2, 0x0024);
    
    // DIWSTART / DIWSTOP
    pCode[ip++] = COP_MOVE(DIWSTART, (pConfig->diw_start_v << 8) | pConfig->diw_start_h);
    pCode[ip++] = COP_MOVE(DIWSTOP, (pConfig->diw_stop_v << 8) | pConfig->diw_stop_h);
    
    // DDFSTART / DDFSTOP
    pCode[ip++] = COP_MOVE(DDFSTART, pConfig->ddf_start);
    pCode[ip++] = COP_MOVE(DDFSTOP, pConfig->ddf_stop);
    
    // BPLxMOD
    pCode[ip++] = COP_MOVE(BPL1MOD, pConfig->ddf_mod);
    pCode[ip++] = COP_MOVE(BPL2MOD, pConfig->ddf_mod);
    
    // BPLxPT        
    pCode[ip++] = COP_MOVE(BPL1PTH, (bplpt >> 16) & UINT16_MAX);
    pCode[ip++] = COP_MOVE(BPL1PTL, bplpt & UINT16_MAX);

    // DMACON
    pCode[ip++] = COP_MOVE(DMACON, 0x8300);

    // End
    pCode[ip++] = COP_END();


    // Clear the screen
    micro_console_cls(pCon);


    // Set the CLUT up
    denise_set_clut_entry(0, 0x036a);    // #306ab0
    denise_set_clut_entry(1, 0x0fff);    // #ffffff


    // Install the Copper program
    copper_force_run_program(pCode);
}

static void micro_console_cls(MicroConsole* _Nonnull pCon)
{
    Bytes_ClearRange(pCon->framebuffer, pCon->bytesPerRow * pCon->config->height);
}

static void micro_console_blit_glyph(MicroConsole* _Nonnull pCon, Character ch, Int x, Int y)
{
    const Int bytesPerRow = pCon->bytesPerRow;
    const Int maxX = pCon->config->width >> 3;
    const Int maxY = pCon->config->height >> 3;
    
    if (x < 0 || y < 0 || x >= maxX || y >= maxY) {
        return;
    }
    
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
static void micro_console_draw_character(MicroConsole* _Nonnull pCon, Character ch)
{
    switch (ch) {
        case '\0':
            break;
            
        case '\t':
            micro_console_draw_character(pCon, ' ');
            break;

        case '\n':
            pCon->x = 0;
            if (pCon->y < pCon->rows - 1) {
                pCon->y++;
            }
            break;
            
        case '\r':
            pCon->x = 0;
            break;
            
        case 12:    // FF Form feed (new page / clear screen)
            micro_console_cls(pCon);
            break;
            
        default:
            if (ch < 32) {
                // non-prinable (control code 0) characters do nothing
                break;
            }
            
            if (pCon->x >= pCon->cols && pCon->cols > 0) {
                // do wrap-by-character
                pCon->x = 0;
                if (pCon->y < pCon->rows - 1) {
                    pCon->y++;
                }
            }

            if ((pCon->x >= 0 && pCon->x < pCon->cols) && (pCon->y >= 0 && pCon->y < pCon->rows)) {
                micro_console_blit_glyph(pCon, ch, pCon->x, pCon->y);
            }
            pCon->x++;
            break;
    }
}

static void micro_console_draw_string(MicroConsole* _Nonnull pCon, const Character* _Nonnull pString)
{
    register char ch;
    
    while ((ch = *pString++) != '\0') {
        micro_console_draw_character(pCon, ch);
    }
}


////////////////////////////////////////////////////////////////////////////////

static void fprintv_micro_console_sink(MicroConsole* _Nonnull pCon, const Character* _Nonnull pString)
{
    micro_console_draw_string(pCon, pString);
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
    cpu_disable_irqs();
    chipset_stop_quantum_timer();
}

_Noreturn fatalError(const Character* _Nonnull format, ...)
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

_Noreturn fatalAbort(const Character* _Nonnull filename, int line)
{
    stop_machine();
    fatalError("Abort: %s:%d", filename, line);
}

_Noreturn fatalAssert(const Character* _Nonnull filename, int line)
{
    stop_machine();
    fatalError("Assert: %s:%d", filename, line);
}

_Noreturn _fatalException(const ExceptionStackFrame* _Nonnull pFrame)
{
    stop_machine();
    fatalError("Exception: %hhx, Format %hhx, PC %p, SR %hx", pFrame->fv.vector >> 2, pFrame->fv.format, pFrame->pc, pFrame->sr);
}
