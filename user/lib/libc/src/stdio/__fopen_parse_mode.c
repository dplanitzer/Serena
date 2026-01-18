//
//  __fopen_parse_mode.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


// Parses the given mode string into a stream-mode value. Supported modes:
//
// Mode                         Action                          File exists         File does not exist
// -----------------------------------------------------------------------------------------------------
// "r"      read                open for reading                read from start     error
// "w"      write               create & open for writing       truncate file       create
// "a"      append              append to file                  write to end        create
// "r+" 	read extended       open for read/write             read from start     error
// "w+" 	write extended      create & open for read/write    truncate file       create
// "a+" 	append extended     open for read/write             write to end        create
//
// "x" may be used with "w" and "w+". It enables exclusive mode which means that open() will return with
// an error if the file already exists.
//
// Modifier                     Action
// -----------------------------------------------------------------------------
// "b"                          open in binary (untranslated) mode
// "t"                          open in translated mode
//
// Modifiers are optional and follow the mode. "b" is always implied on Serena OS.
int __fopen_parse_mode(const char* _Nonnull _Restrict mode, __FILE_Mode* _Nonnull _Restrict pOutMode)
{
    __FILE_Mode sm = 0;
    int ok = 1;

    // Mode
    switch (*mode++) {
        case 'r':   sm |= __kStreamMode_Read; break;
        case 'w':   sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Truncate); break;
        case 'a':   sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Append); break;
        default:    *pOutMode = 0; return EINVAL;
    }


    // Modifier
    while (*mode != '\0') {
        switch (*mode) {
            case 'x':   sm |= __kStreamMode_Exclusive; break;
            case 'b':   sm |= __kStreamMode_Binary; break;
            case 't':   sm |= __kStreamMode_Text; break;
            case '+':   sm |= (__kStreamMode_Read | __kStreamMode_Write); break;
            default:    break;
        }
        mode++;
    }


    if ((sm & (__kStreamMode_Read|__kStreamMode_Write)) == 0) {
        ok = 0;
    }
    if (((sm & __kStreamMode_Exclusive) == __kStreamMode_Exclusive) && ((sm & __kStreamMode_Write) == 0)) {
        ok = 0;
    }
    if ((sm & (__kStreamMode_Append|__kStreamMode_Truncate)) == (__kStreamMode_Append|__kStreamMode_Truncate)) {
        ok = 0;
    }
    if ((sm & (__kStreamMode_Binary | __kStreamMode_Text)) == (__kStreamMode_Binary | __kStreamMode_Text)) {
        ok = 0;
    }

    if (ok) {
        *pOutMode = sm;
        return 0;
    }
    else {
        errno = EINVAL;
        return EOF;
    }
}
