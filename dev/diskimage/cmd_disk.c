//
//  cmd_disk.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ext/errno.h>
#include <machine/amiga/adf.h>


//
// diskimage describe
//

errno_t cmd_describe_disk(const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskImage info;
    const char* formatName;

    err = di_describe_diskimage(dmgPath, &info);
    if (err != EOK) {
        return err;
    }

    switch (info.format) {
        case kDiskImage_Amiga_DD_Floppy:    formatName = "Amiga DD Floppy"; break;
        case kDiskImage_Amiga_HD_Floppy:    formatName = "Amiga HD Floppy"; break;
        case kDiskImage_Serena:             formatName = "Serena Disk Image"; break;
        default:                            abort(); break;
    }

    printf("Type: %s\n\n", formatName);

    if (info.format == kDiskImage_Serena) {
        printf("Logical Size: %zi Blocks\n", info.cylindersPerDisk);
        printf("Physical Size: %zi Blocks\n\n", info.physicalSize / info.bytesPerSector);
        printf("Sector Size: %ziB\n", info.bytesPerSector);
        printf("Disk Size:   %ziKB\n", info.physicalSize / 1024);
    }
    else {
        printf("Cylinders: %zi\n", info.cylindersPerDisk);
        printf("Heads:     %zi\n", info.headsPerCylinder);
        printf("Sectors:   %zi\n\n", info.sectorsPerTrack);
        printf("Sector Size: %ziB\n", info.bytesPerSector);
        printf("Disk Size:   %ziKB\n", info.physicalSize / 1024);
    }

    return EOK;
}


//
// diskimage diff <dmgPath1> <dmgPath2>
//

errno_t cmd_diff_disks(const char* _Nonnull dmgPath1, const char* _Nonnull dmgPath2)
{
    decl_try_err();
    DiskImage info1, info2;
    FILE* fp1 = NULL;
    FILE* fp2 = NULL;
    char* buf1 = NULL;
    char* buf2 = NULL;
    size_t sectorCount = 0;

    err = di_describe_diskimage(dmgPath1, &info1);
    if (err != EOK) {
        return err;
    }

    err = di_describe_diskimage(dmgPath2, &info2);
    if (err != EOK) {
        return err;
    }

    if (info1.format != info2.format) {
        printf("Disk image types differ\n");
        return EOK;
    }

    if (info1.bytesPerSector != info2.bytesPerSector || info1.physicalSize != info2.physicalSize) {
        printf("Disk image sizes differ\n");
        return EOK;
    }

    try_null(fp1, fopen(dmgPath1, "rb"), errno);
    try_null(fp2, fopen(dmgPath2, "rb"), errno);

    try_null(buf1, malloc(info1.bytesPerSector), ENOMEM);
    try_null(buf2, malloc(info2.bytesPerSector), ENOMEM);

    sectorCount = info1.cylindersPerDisk * info1.headsPerCylinder * info1.sectorsPerTrack;
    for (size_t lba = 0; lba < sectorCount; lba++) {
        if (fread(buf1, info1.bytesPerSector, 1, fp1) < 1) {
            throw(EIO);
        }
        if (fread(buf2, info2.bytesPerSector, 1, fp2) < 1) {
            throw(EIO);
        }

        if (memcmp(buf1, buf2, info1.bytesPerSector)) {
            size_t c, h, s;

            di_chs_from_lba(&c, &h, &s, &info1, lba);
            printf("%zi - %zi:%zi:%zi\n", lba, c, h, s);
        }
    }

catch:
    if (fp1) {
        fclose(fp1);
    }
    free(buf1);

    if (fp2) {
        fclose(fp2);
    }
    free(buf2);

    return err;
}


//
// diskimage get --sector=c:h:s
//

#define ADDR_FMT "%.8zx"

static void print_hex_line(size_t addr, const uint8_t* buf, size_t nbytes, size_t ncolumns)
{
    static const char* digits = "0123456789abcdef";
    char tmp[3] = "00 ";

    printf(ADDR_FMT"   ", addr);

    for (size_t i = 0; i < nbytes; i++) {
        tmp[0] = digits[buf[i] >> 4];
        tmp[1] = digits[buf[i] & 0x0f];
        fwrite(tmp, 1, 3, stdout);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fwrite("   ", 1, 3, stdout);
    }
    
    fwrite("  ", 1, 2, stdout);
    for (size_t i = 0; i < nbytes; i++) {
        fputc(isprint(buf[i]) ? buf[i] : '.', stdout);
    }
    for (size_t i = nbytes; i < ncolumns; i++) {
        fputc(' ', stdout);
    }
}

static void print_hex_buffer(const char* _Nonnull buf, size_t bufSize)
{
    const size_t ncolumns = 16;
    size_t addr = 0;
    
    while (addr < bufSize) {
        const size_t nleft = bufSize - addr;
        const size_t nbytes = (nleft >= ncolumns) ? ncolumns : nleft;

        print_hex_line(addr, buf, nbytes, ncolumns);
        addr += ncolumns;
        buf += ncolumns;
        fputc('\n', stdout);
    }
}

static void print_binary_buffer(const char* _Nonnull buf, size_t bufSize)
{
    fwrite(buf, 1, bufSize, stdout);
}

static errno_t print_disk_slice(const char* _Nonnull dmgPath, const DiskImage* _Nonnull info, di_addr_t* _Nonnull addr, size_t sectorCount, bool isHex)
{
    decl_try_err();
    FILE* fp = NULL;
    char* buf = NULL;
    size_t lba;

    try(di_lba_from_disk_addr(&lba, info, addr));
    try_null(fp, fopen(dmgPath, "rb"), errno);
    try_null(buf, malloc(info->bytesPerSector * sectorCount), ENOMEM);
    fseek(fp, info->physicalOffset + info->bytesPerSector * lba, SEEK_SET);

    if (fread(buf, info->bytesPerSector, sectorCount, fp) != sectorCount) {
        throw(EIO);
    }

    if (isHex) {
        print_hex_buffer(buf, info->bytesPerSector * sectorCount);
    }
    else {
        print_binary_buffer(buf, info->bytesPerSector * sectorCount);
    }

catch:
    if (fp) {
        fclose(fp);
    }
    free(buf);

    return err;
}

errno_t cmd_get_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice, bool isHex)
{
    decl_try_err();
    DiskImage info;
    size_t sectorCount;
    
    err = di_describe_diskimage(dmgPath, &info);
    if (err == EOK) { 
        switch (slice->type) {
            case di_slice_type_empty:
                break;

            case di_slice_type_sector:
                sectorCount = 1;
                break;

            case di_slice_type_track:
                sectorCount = info.sectorsPerTrack;
                break;

            default:
                abort();
                break;
        }

        err = print_disk_slice(dmgPath, &info, &slice->start, sectorCount, isHex);
    }
    return err;
}


//
// diskimage put --sector=c:h:s
//

static errno_t replace_disk_slice(const char* _Nonnull dmgPath, const DiskImage* _Nonnull info, di_addr_t* _Nonnull addr, size_t sectorCount)
{
    decl_try_err();
    FILE* fp = NULL;
    char* buf = NULL;
    size_t lba;

    try(di_lba_from_disk_addr(&lba, info, addr));
    try_null(fp, fopen(dmgPath, "rb"), errno);
    try_null(buf, malloc(info->bytesPerSector * sectorCount), ENOMEM);
    fseek(fp, info->physicalOffset + info->bytesPerSector * lba, SEEK_SET);

    if (fread(buf, info->bytesPerSector, sectorCount, stdin) != sectorCount) {
        throw(EIO);
    }

    if (fwrite(buf, info->bytesPerSector, sectorCount, fp) != sectorCount) {
        throw(EIO);
    }
    if (fflush(fp) != 0) {
        throw(EIO);
    }

catch:
    if (fp) {
        fclose(fp);
    }
    free(buf);

    return err;
}

errno_t cmd_put_disk_slice(const char* _Nonnull dmgPath, di_slice_t* _Nonnull slice)
{
    decl_try_err();
    DiskImage info;
    size_t sectorCount;
    
    err = di_describe_diskimage(dmgPath, &info);
    if (err == EOK) {
        switch (slice->type) {
            case di_slice_type_empty:
                break;

            case di_slice_type_sector:
                sectorCount = 1;
                break;

            case di_slice_type_track:
                sectorCount = info.sectorsPerTrack;
                break;

            default:
                abort();
                break;
        }

        err = replace_disk_slice(dmgPath, &info, &slice->start, sectorCount);
    }
    return err;
}
