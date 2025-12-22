;
;  machine/hw/m68k-amiga/chipset.i
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

        ifnd CHIPSET_I
CHIPSET_I   set 1


; ROMs
DIAG_ROM_BASE       equ     $f00000
DIAG_ROM_SIZE       equ     $80000
LOW_ROM_BASE        equ     $f80000
LOW_ROM_SIZE        equ     $40000
HIGH_ROM_BASE       equ     $fc0000
HIGH_ROM_SIZE       equ     $40000


; CIA A registers
CIAAPRA     equ     $bfe001
CIAAPRB     equ     $bfe101
CIAADDRA    equ     $bfe201
CIAADDRB    equ     $bfe301
CIAATALO    equ     $bfe401
CIAATAHI    equ     $bfe501
CIAATBLO    equ     $bfe601
CIAATBHI    equ     $bfe701
CIAATODLO   equ     $bfe801
CIAATODMID  equ     $bfe901
CIAATODHI   equ     $bfea01
CIAASDR     equ     $bfec01
CIAAICR     equ     $bfed01
CIAACRA     equ     $bfee01
CIAACRB     equ     $bfef01


; CIA B registers
CIABPRA     equ     $bfd000
CIABPRB     equ     $bfd100
CIABDDRA    equ     $bfd200
CIABDDRB    equ     $bfd300
CIABTALO    equ     $bfd400
CIABTAHI    equ     $bfd500
CIABTBLO    equ     $bfd600
CIABTBHI    equ     $bfd700
CIABTODLO   equ     $bfd800
CIABTODMID  equ     $bfd900
CIABTODHI   equ     $bfda00
CIABSDR     equ     $bfdc00
CIABICR     equ     $bfdd00
CIABCRA     equ     $bfde00
CIABCRB     equ     $bfdf00


; CIA ICR flags
ICRF_IR         equ $0080
ICRF_FLG        equ $0010
ICRF_SP         equ $0008
ICRF_ALRM       equ $0004
ICRF_TB         equ $0002
ICRF_TA         equ $0001

ICRB_IR         equ 7
ICRB_FLG        equ 4
ICRB_SP         equ 3
ICRB_ALRM       equ 2
ICRB_TB         equ 1
ICRB_TA         equ 0


;-------------------------------------------------------------------------------

; RTC chip base address
RTC_BASE    equ     $dc0000


; Base adress of the custom chips.
; The custom chip register numbers are all relative to this base address.
CUSTOM_BASE equ     $dff000


; Misc control registers
JOY0DAT     equ     $00a
JOY1DAT     equ     $00c
JOYTEST     equ     $036
CLXDAT      equ     $00e
ADKCONR     equ     $010
POT0DAT     equ     $012
POT1DAT     equ     $014
POTGOR      equ     $016
POTGO       equ     $034
REFPTR      equ     $028
VPOSR       equ     $004
VHPOSR      equ     $006
VPOSW       equ     $02a
VHPOSW      equ     $02c
SERDATR     equ     $018
SERDAT      equ     $030
SERPER      equ     $032
STREQU      equ     $038
STRVBL      equ     $03a
STRHOR      equ     $03c
STRLONG     equ     $03e
DIWSTRT     equ     $08e
DIWSTOP     equ     $090
DDFSTRT     equ     $092
DDFSTOP     equ     $094
DMACONR     equ     $002
DMACON      equ     $096
INTENA      equ     $09a
INTREQ      equ     $09c
INTENAR     equ     $01c
INTREQR     equ     $01e


; Copper registers
COPCON      equ     $02e
COPINS      equ     $08c
COPJMP1     equ     $088
COPJMP2     equ     $08a
COP1LC      equ     $080
COP1LCH     equ     $080
COP1LCL     equ     $082
COP2LC      equ     $084
COP2LCH     equ     $084
COP2LCL     equ     $086


; Bitplane registers
BPL1PT      equ     $0e0
BPL1PTH     equ     $0e0
BPL1PTL     equ     $0e2
BPL2PT      equ     $0e4
BPL2PTH     equ     $0e4
BPL2PTL     equ     $0e6
BPL3PT      equ     $0e8
BPL3PTH     equ     $0e8
BPL3PTL     equ     $0ea
BPL4PT      equ     $0ec
BPL4PTH     equ     $0ec
BPL4PTL     equ     $0ee
BPL5PT      equ     $0f0
BPL5PTH     equ     $0f0
BPL5PTL     equ     $0f2
BPL6PT      equ     $0f4
BPL6PTH     equ     $0f4
BPL6PTL     equ     $0f6

BPLCON0     equ     $100
BPLCON1     equ     $102
BPLCON2     equ     $104
BPLCON3     equ     $106
BPL1MOD     equ     $108
BPL2MOD     equ     $10a

DPL1DATA    equ     $110
DPL2DATA    equ     $112
DPL3DATA    equ     $114
DPL4DATA    equ     $116
DPL5DATA    equ     $118
DPL6DATA    equ     $11a


; Blitter registers
BLTCON0     equ     $040
BLTCON1     equ     $042
BLTAFWM     equ     $044
BLTALWM     equ     $046
BLTCPT      equ     $048
BLTCPTH     equ     $048
BLTCPTL     equ     $04a
BLTBPT      equ     $04c
BLTBPTH     equ     $04c
BLTBPTL     equ     $04e
BLTAPT      equ     $050
BLTAPTH     equ     $050
BLTAPTL     equ     $052
BLTDPT      equ     $054
BLTDPTH     equ     $054
BLTDPTL     equ     $056
BLTSIZE     equ     $058
BLTCMOD     equ     $060
BLTBMOD     equ     $062
BLTAMOD     equ     $064
BLTDMOD     equ     $066
BLTCDAT     equ     $070
BLTBDAT     equ     $072
BLTADAT     equ     $074
BLTDDAT     equ     $000


; Color registers
COLOR_REGS_COUNT equ 32
COLOR_BASE  equ     $180
COLOR00     equ     COLOR_BASE+$00
COLOR01     equ     COLOR_BASE+$02
COLOR02     equ     COLOR_BASE+$04
COLOR03     equ     COLOR_BASE+$06
COLOR04     equ     COLOR_BASE+$08
COLOR05     equ     COLOR_BASE+$0A
COLOR06     equ     COLOR_BASE+$0C
COLOR07     equ     COLOR_BASE+$0E
COLOR08     equ     COLOR_BASE+$10
COLOR09     equ     COLOR_BASE+$12
COLOR10     equ     COLOR_BASE+$14
COLOR11     equ     COLOR_BASE+$16
COLOR12     equ     COLOR_BASE+$18
COLOR13     equ     COLOR_BASE+$1A
COLOR14     equ     COLOR_BASE+$1C
COLOR15     equ     COLOR_BASE+$1E
COLOR16     equ     COLOR_BASE+$20
COLOR17     equ     COLOR_BASE+$22
COLOR18     equ     COLOR_BASE+$24
COLOR19     equ     COLOR_BASE+$26
COLOR20     equ     COLOR_BASE+$28
COLOR21     equ     COLOR_BASE+$2A
COLOR22     equ     COLOR_BASE+$2C
COLOR23     equ     COLOR_BASE+$2E
COLOR24     equ     COLOR_BASE+$30
COLOR25     equ     COLOR_BASE+$32
COLOR26     equ     COLOR_BASE+$34
COLOR27     equ     COLOR_BASE+$36
COLOR28     equ     COLOR_BASE+$38
COLOR29     equ     COLOR_BASE+$3A
COLOR30     equ     COLOR_BASE+$3C
COLOR31     equ     COLOR_BASE+$3E


; Audio control registers
ADKCON      equ     $09e

AUD0LC      equ     $0a0
AUD0LCH     equ     $0a0
AUD0LCL     equ     $0a2
AUD0LEN     equ     $0a4
AUD0PER     equ     $0a6
AUD0VOL     equ     $0a8
AUD0DAT     equ     $0aa

AUD1LC      equ     $0b0
AUD1LCH     equ     $0b0
AUD1LCL     equ     $0b2
AUD1LEN     equ     $0b4
AUD1PER     equ     $0b6
AUD1VOL     equ     $0b8
AUD1DAT     equ     $0ba

AUD2LC      equ     $0c0
AUD2LCH     equ     $0c0
AUD2LCL     equ     $0c2
AUD2LEN     equ     $0c4
AUD2PER     equ     $0c6
AUD2VOL     equ     $0c8
AUD2DAT     equ     $0ca

AUD3LC      equ     $0d0
AUD3LCH     equ     $0d0
AUD3LCL     equ     $0d2
AUD3LEN     equ     $0d4
AUD3PER     equ     $0d6
AUD3VOL     equ     $0d8
AUD3DAT     equ     $0da


; Disk controller registers
DSKBYTR     equ     $01a
DSKPT       equ     $020
DSKPTH      equ     $020
DSKPTL      equ     $022
DSKLEN      equ     $024
DSKDATR     equ     $008
DSKDAT      equ     $026
DSKSYNC     equ     $07e


;-------------------------------------------------------------------------------


; INTENA flags
INTF_SETCLR     equ $8000
INTF_INTEN      equ $4000
INTF_EXTER      equ $2000
INTF_DSKSYN     equ $1000
INTF_RBF        equ $0800
INTF_AUD3       equ $0400
INTF_AUD2       equ $0200
INTF_AUD1       equ $0100
INTF_AUD0       equ $0080
INTF_BLIT       equ $0040
INTF_VERTB      equ $0020
INTF_COPER      equ $0010
INTF_PORTS      equ $0008
INTF_SOFT       equ $0004
INTF_DSKBLK     equ $0002
INTF_TBE        equ $0001
INTF_MASK       equ $7fff       ; mask

INTB_SETCLR     equ 15
INTB_INTEN      equ 14
INTB_EXTER      equ 13
INTB_DSKSYN     equ 12
INTB_RBF        equ 11
INTB_AUD3       equ 10
INTB_AUD2       equ 9
INTB_AUD1       equ 8
INTB_AUD0       equ 7
INTB_BLIT       equ 6
INTB_VERTB      equ 5
INTB_COPER      equ 4
INTB_PORTS      equ 3
INTB_SOFT       equ 2
INTB_DSKBLK     equ 1
INTB_TBE        equ 0


; DMACON flags
DMACONF_SETCLR     equ $8000
DMACONF_AUDIO      equ $000f       ; mask
DMACONF_AUD0       equ $0001
DMACONF_AUD1       equ $0002
DMACONF_AUD2       equ $0004
DMACONF_AUD3       equ $0008
DMACONF_DISK       equ $0010
DMACONF_SPRITE     equ $0020
DMACONF_BLITTER    equ $0040
DMACONF_COPPER     equ $0080
DMACONF_RASTER     equ $0100
DMACONF_MASTER     equ $0200
DMACONF_BLITHOG    equ $0400
DMACONF_ALL        equ $01ff       ; mask

        endif
