//
//  kpi/ioctl.h
//  libc
//
//  Created by Dietmar Planitzer on 4/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_IOCTL_H
#define _KPI_IOCTL_H 1

#include <stdint.h>


#define IOResourceCommand(__cmd) (__cmd)

#define _IOCAT(major, minor) ((iocat_t)((major << 8) | (minor)))

// Major driver categories. Major categories are derived from the I/O categories
// defined below.
#define IOCAT_NONE          0
#define IOCAT_END           IOCAT_NONE
#define IOCAT_HID           1
#define IOCAT_DISK          2
#define IOCAT_TAPE          3
#define IOCAT_MEMORY        4
#define IOCAT_VIDEO         5
#define IOCAT_AUDIO         6
#define IOCAT_BUS           7
#define IOCAT_COM           8
#define IOCAT_UNSPECIFIED   9

// HID Category
#define IOHID_KEYBOARD      _IOCAT(IOCAT_HID, 1)
#define IOHID_KEYPAD        _IOCAT(IOCAT_HID, 2)
#define IOHID_MOUSE         _IOCAT(IOCAT_HID, 3)
#define IOHID_TRACKBALL     _IOCAT(IOCAT_HID, 4)
#define IOHID_STYLUS        _IOCAT(IOCAT_HID, 5)
#define IOHID_LIGHTPEN      _IOCAT(IOCAT_HID, 6)
#define IOHID_ANALOG_JOYSTICK   _IOCAT(IOCAT_HID, 7)
#define IOHID_DIGITAL_JOYSTICK  _IOCAT(IOCAT_HID, 8)

// Disk Category
#define IODISK_FLOPPY       _IOCAT(IOCAT_DISK, 1)
#define IODISK_ZIP          _IOCAT(IOCAT_DISK, 2)
#define IODISK_HARDDISK     _IOCAT(IOCAT_DISK, 3)
#define IODISK_SSD          _IOCAT(IOCAT_DISK, 4)
#define IODISK_CDROM        _IOCAT(IOCAT_DISK, 5)
#define IODISK_DVDROM       _IOCAT(IOCAT_DISK, 6)
#define IODISK_BLUERAY      _IOCAT(IOCAT_DISK, 7)
#define IODISK_VIDEODISC    _IOCAT(IOCAT_DISK, 8)
#define IODISK_RAMDISK      _IOCAT(IOCAT_DISK, 9)
#define IODISK_ROMDISK      _IOCAT(IOCAT_DISK, 10)

// Tape Category
#define IOTAPE_LTO          _IOCAT(IOCAT_TAPE, 1)
#define IOTAPE_DLT          _IOCAT(IOCAT_TAPE, 2)
#define IOTAPE_DAT          _IOCAT(IOCAT_TAPE, 3)

// Memory Category
#define IOMEM_RAM           _IOCAT(IOCAT_MEMORY, 1)

// Video Category
#define IOVID_FB            _IOCAT(IOCAT_VIDEO, 1)
#define IOVID_BLITTER       _IOCAT(IOCAT_VIDEO, 2)
#define IOVID_GPU_2D        _IOCAT(IOCAT_VIDEO, 3)
#define IOVID_GPU_3D        _IOCAT(IOCAT_VIDEO, 4)

// Audio Category
#define IOAUD_SYNTH         _IOCAT(IOCAT_AUDIO, 1)
#define IOAUD_DA            _IOCAT(IOCAT_AUDIO, 2)
#define IOAUD_AD            _IOCAT(IOCAT_AUDIO, 3)
#define IOAUD_MIDI          _IOCAT(IOCAT_AUDIO, 4)

// Bus Category
#define IOBUS_SCSI          _IOCAT(IOCAT_BUS, 1)
#define IOBUS_ATA           _IOCAT(IOCAT_BUS, 2)
#define IOBUS_ZORRO         _IOCAT(IOCAT_BUS, 3)
#define IOBUS_PCI           _IOCAT(IOCAT_BUS, 4)
#define IOBUS_NUBUS         _IOCAT(IOCAT_BUS, 5)
#define IOBUS_ISA           _IOCAT(IOCAT_BUS, 6)
#define IOBUS_EISA          _IOCAT(IOCAT_BUS, 7)
#define IOBUS_MICROCHANNEL  _IOCAT(IOCAT_BUS, 8)
#define IOBUS_ADB           _IOCAT(IOCAT_BUS, 9)
#define IOBUS_PS2           _IOCAT(IOCAT_BUS, 10)
#define IOBUS_GP            _IOCAT(IOCAT_BUS, 11)
#define IOBUS_USB           _IOCAT(IOCAT_BUS, 12)
#define IOBUS_FIREWIRE      _IOCAT(IOCAT_BUS, 13)
#define IOBUS_VIRTUAL       _IOCAT(IOCAT_BUS, 14)
#define IOBUS_PROPRIETARY   _IOCAT(IOCAT_BUS, 15)

// Communication (Network) Category
#define IOCOM_SERIAL        _IOCAT(IOCAT_COM, 1)
#define IOCOM_CENTRONICS    _IOCAT(IOCAT_COM, 2)
#define IOCOM_ETHERNET      _IOCAT(IOCAT_COM, 3)
#define IOCOM_TOKENRING     _IOCAT(IOCAT_COM, 4)
#define IOCOM_FDDI          _IOCAT(IOCAT_COM, 5)
#define IOCOM_WIFI          _IOCAT(IOCAT_COM, 6)

// Unspecified Category
#define IOUNS_UNKNOWN       _IOCAT(IOCAT_UNSPECIFIED, 0)
#define IOUNS_PROPRIETARY   _IOCAT(IOCAT_UNSPECIFIED, 1)


typedef uint16_t iocat_t;


// The maximum number of categories that a driver may conform to. This includes
// the terminating IOCAT_END.
#define IOCAT_MAX   16


#define IOCATS_DEF(__name, ...) \
static const iocat_t __name[] = { __VA_ARGS__, IOCAT_END }


//
// Driver Commands
//

// Returns the unique driver id. Note that this id is not suitable for
// persistence. The id for a driver may change from one boot cycle to the next.
// Use the driver hardware path ('/dev/hw/xxx') instead if you need to persist
// the name of a particular driver instance. The driver id is used to easily and
// efficiently refer to a particular driver instance with respect to the current
// boot cycle.
// get_id(did_t* _Nonnull id)
#define kDriverCommand_GetId    IOResourceCommand(1)

// Returns a copy of the I/O categories to which the driver conforms. The
// categories are returned in the buffer 'cats' with byte size sizeof(iocat_t) *
// 'bufsiz'. 'bufsiz' should be IOCAT_MAX to ensure that the buffer is big
// enough to hold all categories. Returns EINVAL if 'bufsiz' is 0 and ERANGE if
// 'bufsiz' is less than the number of I/O categories stored in the driver plus
// one. The returned I/O categories string is terminated by IOCAT_END.
// get_categories(iocat_t* _Nonnull cats, size_t bufsiz)
#define kDriverCommand_GetCategories    IOResourceCommand(2)


//
// Driver Subclasses
//

// Driver subclasses define their respective commands based on this number.
#define kDriverCommand_SubclassBase  IOResourceCommand(256)

#endif /* _KPI_IOCTL_H */
