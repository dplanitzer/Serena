//
//  keymap.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


// Compile on Windows:
// - open a Visual Studio Command Line environment
// - cd to the Apollo folder
// - cl keymap.c
//


// USB key scan code
typedef uint16_t    USBKeyCode;

// Longest possible byte sequence that a key can produce and that KeyMap_Map()
// will return.
// The max length is chosen such that a single key stroke can be mapped to
// 4 UTF-32 characters.
#define kKeyMap_MaxByteSequenceLength   16

// A small string. This is the kind of string we are willing to store in a string
// based key trap. Max length including the trailing \0 is 17 bytes for now.
typedef char        SmallString[kKeyMap_MaxByteSequenceLength+1];


// Key Map Types:
// 0    -> big endian
typedef enum _KeyMapType {
    kKeyMapType_0 = 0
} KeyMapType;

// Key (Range/Trap) Types:
// 0    -> key(usb_key_code, char, char, char, char)    [unmodified, shift, alt, shift+alt]
// 1    -> key(usb_key_code, string)
typedef enum _KeyType {
    kKeyType_4Bytes = 0,
    kKeyType_String = 3
} KeyType;


typedef struct _Key {
    KeyType     type;
    USBKeyCode  keyCode;
    union {
        char    b[4];
        size_t  stringIdx;
    }       u;
} Key;

typedef struct _KeysTable {
    Key*    table;
    size_t  count;
    size_t  capacity;
} KeysTable;



typedef KeyType RangeType;

typedef struct _KeyRange {
    RangeType   keyType;        // kKeyType_XXX
    size_t      lowerKeyIndex;  // Index into KeysTable
    size_t      upperKeyIndex;  // Index into KeysTable
} KeyRange;

typedef struct _RangesTable {
    KeyRange*   table;
    size_t      count;
    size_t      capacity;
} RangesTable;


typedef struct _StringTable {
    SmallString*    table;
    size_t          count;
    size_t          capacity;
} StringTable;


typedef struct _KeyMap {
    KeysTable   keys;
    RangesTable ranges;
    StringTable strings;
} KeyMap;


////////////////////////////////////////////////////////////////////////////////
// Errors
////////////////////////////////////////////////////////////////////////////////

typedef struct _SourceLocation {
    int line;
    int column;
} SourceLocation;

static void failed(const char* msg)
{
    puts(msg);
    exit(EXIT_FAILURE);
}

static void failedf(const char* fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void failedParsing(SourceLocation loc, const char* msg)
{
    printf("line %d:%d: %s\n", loc.line, loc.column, msg);
    exit(EXIT_FAILURE);
}


////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////

static FILE* km_open(const char* filename, const char* mode)
{
    FILE* s = fopen(filename, mode);

    if (s == NULL) {
        failedf("Unable to open '%s'", filename);
        // NOT REACHED
    }
    return s;
}

static char* createPathWithReplacedExtension(const char* path, const char* newExtension)
{
    const size_t oldPathOverallLength = strlen(path);
    const char* oldPathExtension = strrchr(path, '.');
    const size_t oldExtensionLength = (oldPathExtension) ? strlen(oldPathExtension + 1) : 0;
    const size_t newExtensionLength = (newExtension) ? strlen(newExtension) : 0;
    const size_t oldPathLengthWithDotSansExtension = oldPathOverallLength - oldExtensionLength;
    char* newPath = malloc(oldPathLengthWithDotSansExtension + newExtensionLength + 1);

    if (newPath) {
        strncpy(newPath, path, oldPathLengthWithDotSansExtension);
        newPath[oldPathLengthWithDotSansExtension] = '\0';
        strcat(newPath, newExtension);
    }
    return newPath;
}

static char* createFilenameFromPath(const char* path)
{
    char* pathFilename = strrchr(path, '/');
    if (pathFilename == NULL) pathFilename = strrchr(path, '\\');
    pathFilename = (pathFilename) ? pathFilename + 1 : path;
    const char* pathExtension = strrchr(pathFilename, '.');
    const size_t filenameLength = (pathExtension) ? pathExtension - pathFilename : strlen(pathFilename);
    char* filename = malloc(filenameLength + 1);

    if (filename) {
        strncpy(filename, pathFilename, filenameLength);
        filename[filenameLength] = '\0';
    }

    return filename;
}


////////////////////////////////////////////////////////////////////////////////
// Lexer
////////////////////////////////////////////////////////////////////////////////

// Token types
typedef enum _TokenType {
    kTokenType_Eof = 0,
    kTokenType_Key,
    kTokenType_OpeningPara,
    kTokenType_ClosingPara,
    kTokenType_Comma,
    kTokenType_NumberLiteral,        // number
    kTokenType_CharacterLiteral,     // character
    kTokenType_StringLiteral,        // SmallString
    kTokenType_Other                 // character
} TokenType;

typedef struct _Token {
    TokenType       type;
    SourceLocation  loc;
    union {
        int         number;
        char        character;
        SmallString string;
    }           u;
} Token;


static SourceLocation gCurrentLoc;
static int gPreviousLineLastColumn;

static int km_getc(FILE *s)
{
    const int r = getc(s);
    gCurrentLoc.column++;

    if (r == '\n') {
        gPreviousLineLastColumn = gCurrentLoc.column;
        gCurrentLoc.column = 1;
        gCurrentLoc.line++;
    }
    else if (r == EOF) {
        gCurrentLoc.column--;
    }

    return r;
}

static int km_ungetc(int ch, FILE *s)
{
    if (ch == '\n') {
        gCurrentLoc.line--;
        gCurrentLoc.column = gPreviousLineLastColumn;
    }
    else if (ch != EOF) {
        gCurrentLoc.column--;
    }
    return ungetc(ch, s);
}

static void skipLineComment(FILE *s)
{
    while (true) {
        const int ch = km_getc(s);

        if (ch == '\n' || ch == EOF) {
            km_ungetc(ch, s);
            break;
        }
    }
}

static void readIdentifier(FILE *s, int firstChar, SmallString id)
{
    int i = 0;

    id[i++] = (char) firstChar;
    while (i < kKeyMap_MaxByteSequenceLength - 1) {
        const int ch = km_getc(s);

        if (!isalpha(ch)) {
            km_ungetc(ch, s);
            break;
        }
        id[i++] = (char) ch;
    }
    id[i] = '\0';
}

static long readNumberLiteral(FILE *s, int firstDigit)
{
    char digits[16];
    int i = 0;
    bool hasCheckedRadix = false;
    int radix = 10;

    km_ungetc(firstDigit, s);
    while (i < sizeof(digits)-1) {
        int ch = km_getc(s);

        if (ch == '0' && !hasCheckedRadix) {
            int ch2 = km_getc(s);

            if (ch2 == 'x' || ch2 == 'X') {
                radix = 16;
                ch = km_getc(s);
            } else {
                radix = 8;
                km_ungetc(ch2, s);
            }
            hasCheckedRadix = true;
        }

        if ((radix != 16 && !isdigit(ch)) || (radix == 16 && !isxdigit(ch))) {
            km_ungetc(ch, s);
            break;
        }

        digits[i++] = (char) ch;
    }
    digits[i] = '\0';

    return strtol(digits, NULL, radix);
}

static char readEscapedCharacter(FILE *s)
{
    const int ch = km_getc(s);

    switch (ch) {
        case 'n':   return 0x0a;
        case 'r':   return 0x0d;
        case 'b':   return 0x08;
        case 't':   return 0x09;
        case 'e':   return 0x1b;
        case '\'':  return '\'';
        case '\"':  return '\"';
        case '\\':  return '\\';
        default:    failedParsing(gCurrentLoc, "expected a valid escaped character"); return 0;
    }
}

static char readCharacterLiteral(FILE *s)
{
    int ch = km_getc(s);

    if (ch == '\\') {
        ch = readEscapedCharacter(s);
    }
    if (km_getc(s) != '\'') {
        failedParsing(gCurrentLoc, "expected a ' character");
    }

    return ch;
}

static void readStringLiteral(FILE *s, SmallString str)
{
    int i = 0;

    while (i < kKeyMap_MaxByteSequenceLength) {
        int ch = km_getc(s);

        if (ch == '\\') {
            ch = readEscapedCharacter(s);
        }
        else if(ch == '\"') {
            break;
        }
        else if (ch == EOF) {
            km_ungetc(ch, s);
            break;
        }

        str[i++] = (char) ch;
    }
    str[i] = '\0';
}

static Token xGetNextToken(FILE *s)
{
    Token t;

    while (true) {
        const int ch = km_getc(s);

        switch (ch) {
            case EOF:
                t.type = kTokenType_Eof;
                t.loc = gCurrentLoc;
                return t;

            case '(':
                t.type = kTokenType_OpeningPara;
                t.loc = gCurrentLoc;
                return t;

            case ')':
                t.type = kTokenType_ClosingPara;
                t.loc = gCurrentLoc;
                return t;

            case ',':
                t.type = kTokenType_Comma;
                t.loc = gCurrentLoc;
                return t;

            case '\'':
                t.type = kTokenType_CharacterLiteral;
                t.loc = gCurrentLoc;
                t.u.character = readCharacterLiteral(s);
                return t;

            case '\"':
                t.type = kTokenType_StringLiteral;
                t.loc = gCurrentLoc;
                readStringLiteral(s, t.u.string);
                return t;

            case '/':
                if (km_getc(s) == '/') {
                    skipLineComment(s);
                } else {
                    km_ungetc(ch, s);
                    t.type = kTokenType_Other;
                    t.loc = gCurrentLoc;
                    t.u.character = (char) ch;
                    return t;
                }
                break;

            case '+':
            case '-':
                t.type = kTokenType_NumberLiteral;
                t.loc = gCurrentLoc;
                t.u.number = readNumberLiteral(s, ch);
                return t;

            default:
                if (!isspace(ch)) {
                    t.loc = gCurrentLoc;
                    if (isdigit(ch)) {
                        t.type = kTokenType_NumberLiteral;
                        t.u.number = readNumberLiteral(s, ch);
                    } else {
                        readIdentifier(s, ch, t.u.string);
                        if (!strcmp(t.u.string, "key")) {
                            t.type = kTokenType_Key;
                        } else {
                            t.type = kTokenType_Other;
                            t.u.character = (char) ch;
                        }
                    }
                    return t;
                }
                break;
        }
    }
}

typedef struct _TokenBuffer {
    Token   t;
    bool    isValid;
} TokenBuffer;

static TokenBuffer  gTokenBuffer = {0};


static Token getNextToken(FILE *s)
{
    if (gTokenBuffer.isValid) {
        gTokenBuffer.isValid = false;
        return gTokenBuffer.t;
    } else {
        return xGetNextToken(s);
    }
}

static Token peekNextToken(FILE *s)
{
    assert(!gTokenBuffer.isValid);

    gTokenBuffer.t = xGetNextToken(s);
    gTokenBuffer.isValid = true;

    return gTokenBuffer.t;
}


////////////////////////////////////////////////////////////////////////////////
// Parser
////////////////////////////////////////////////////////////////////////////////

#define expectToken(ttype, err_msg) \
    t = getNextToken(s); \
    if (t.type != ttype) { failedParsing(t.loc, err_msg); }

#define expectComma() \
    expectToken(kTokenType_Comma, "expected a comma")

static char parseCharacter(FILE *s)
{
    Token t = getNextToken(s);

    if (t.type == kTokenType_NumberLiteral) {
        // XXX warn on overflow
        return (char) t.u.number;
    }
    else if (t.type == kTokenType_CharacterLiteral) {
        return t.u.character;
    }
    else {
        failedParsing(t.loc, "expected a character literal");
        // NOT REACHED
        return 0;
    }
}

static size_t addAndUniqueString(KeyMap* kmap, SmallString str)
{
    // Check whether we already got this string and return the index to this
    // string, if so.
    for (size_t i = 0; i < kmap->strings.count; i++) {
        if (!strcmp(kmap->strings.table[i], str)) {
            return i;
        }
    }


    // A new string. Add it to our table
    if (kmap->strings.count == kmap->strings.capacity) {
        const size_t newCapacity = kmap->strings.capacity + 8;

        kmap->strings.table = (SmallString*) realloc(kmap->strings.table, newCapacity * sizeof(SmallString));
        kmap->strings.capacity = newCapacity;
    }

    strcpy(kmap->strings.table[kmap->strings.count++], str);
    return kmap->strings.count - 1;
}

// key(0x0004, 'a', 'A', 0, 0)
// key(0x003a, "\e[11~")
static void parseKeyStatement(FILE* s, KeyMap* kmap)
{
    Token t;

    if (kmap->keys.count == kmap->keys.capacity) {
        const size_t newCapacity = kmap->keys.capacity + 32;

        kmap->keys.table = (Key*) realloc(kmap->keys.table, newCapacity * sizeof(Key));
        kmap->keys.capacity = newCapacity;
    }

    Key* pKey = &kmap->keys.table[kmap->keys.count];


    expectToken(kTokenType_OpeningPara, "expected a ( character");
    expectToken(kTokenType_NumberLiteral, "expected a USB key scan code");
    pKey->keyCode = (USBKeyCode) t.u.number;

    expectComma();

    if (peekNextToken(s).type == kTokenType_StringLiteral) {
        // key(0x003a, "\e[11~")
        t = getNextToken(s);
        pKey->u.stringIdx = addAndUniqueString(kmap, t.u.string);
        pKey->type = kKeyType_String;
    } else {
        // key(0x0004, 'a', 'A', 0, 0)
        // unmodified
        pKey->u.b[0] = parseCharacter(s);
        expectComma();
        // shift
        pKey->u.b[1] = parseCharacter(s);
        expectComma();
        // alt
        pKey->u.b[2] = parseCharacter(s);
        expectComma();
        // shift+alt
        pKey->u.b[3] = parseCharacter(s);
        pKey->type = kKeyType_4Bytes;
    }

    expectToken(kTokenType_ClosingPara, "expected a ) character");
    kmap->keys.count++;
}

static void parseKeysFile(FILE* s, KeyMap* kmap)
{
    bool done = false;

    memset(kmap, 0, sizeof(KeyMap));
    gCurrentLoc.line = 1;
    gCurrentLoc.column = 1;

    while (!done) {
        const Token t = getNextToken(s);

        switch (t.type) {
            case kTokenType_Key:
                parseKeyStatement(s, kmap);
                break;

            case kTokenType_Eof:
                done = true;
                break;

            default:
                failedParsing(t.loc, "expected a 'key' statement");
                break;
        }
    }
}

static void freeKeyMap(KeyMap* kmap)
{
    if (kmap) {
        free(kmap->strings.table);
        free(kmap->keys.table);
        free(kmap->ranges.table);
        memset(kmap, 0, sizeof(KeyMap));
    }
}


////////////////////////////////////////////////////////////////////////////////
// Ranges finder
////////////////////////////////////////////////////////////////////////////////

static int keyCodeSorter(const Key* lhs, const Key* rhs)
{
    return ((int) lhs->keyCode) - ((int) rhs->keyCode);
}

static void calculateKeyRanges(KeyMap* kmap)
{
    Key* pKeys = kmap->keys.table;
    const size_t nKeys = kmap->keys.count;

    qsort(pKeys, nKeys, sizeof(Key), keyCodeSorter);

    for (size_t i = 0; i < nKeys; i++) {
        bool isEndOfRange = false;

        if (i == 0
            || (pKeys[i].keyCode - pKeys[i - 1].keyCode) > 1
            || pKeys[i].type != pKeys[i - 1].type) {
            isEndOfRange = true;
        }

        if (isEndOfRange) {
            // Update the upper bound of the previous range
            if (kmap->ranges.count > 0) {
                kmap->ranges.table[kmap->ranges.count - 1].upperKeyIndex = i - 1;
            }

            if (kmap->ranges.count == kmap->ranges.capacity) {
                const size_t newCapacity = kmap->ranges.capacity + 8;

                kmap->ranges.table = (KeyRange*) realloc(kmap->ranges.table, newCapacity * sizeof(KeyRange));
                kmap->ranges.capacity = newCapacity;
            }

            kmap->ranges.table[kmap->ranges.count].keyType = pKeys[i].type;
            kmap->ranges.table[kmap->ranges.count].lowerKeyIndex = i;
            kmap->ranges.table[kmap->ranges.count].upperKeyIndex = i;
            kmap->ranges.count++;
        }
    }

    // Update the upper bound of the last range
    if (kmap->ranges.count > 0) {
        kmap->ranges.table[kmap->ranges.count - 1].upperKeyIndex = kmap->keys.count - 1;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Compile keymaps
////////////////////////////////////////////////////////////////////////////////

typedef enum _Endianness {
    Endian_Big = 0,
    Endian_Little
} Endianness;

typedef struct _Data {
    uint8_t*    bytes;
    size_t      count;
    size_t      capacity;
} Data;

typedef struct _PatchLocation {
    size_t      offsetToPatchLocation;
    const void* label;
} PatchLocation;

typedef struct _PatchLocations {
    PatchLocation*  table;
    size_t          count;
    size_t          capacity;
} PatchLocations;

typedef struct _CompiledKeyMap {
    Data            data;
    PatchLocations  patchLocs;
} CompiledKeyMap;


static uint16_t getLocationCounter(const CompiledKeyMap* ckmap)
{
    return (uint16_t) ckmap->data.count;
}

static void addPatchLocation(CompiledKeyMap* ckmap, size_t offsetToPatchLocation, const void* label)
{
    if (ckmap->patchLocs.count == ckmap->patchLocs.capacity) {
        const size_t newCapacity = ckmap->patchLocs.capacity + 16;

        ckmap->patchLocs.table = (PatchLocation*) realloc(ckmap->patchLocs.table, newCapacity * sizeof(PatchLocation));
        ckmap->patchLocs.capacity = newCapacity;
    }

    ckmap->patchLocs.table[ckmap->patchLocs.count].offsetToPatchLocation = offsetToPatchLocation;
    ckmap->patchLocs.table[ckmap->patchLocs.count].label = label;
    ckmap->patchLocs.count++;
}

static void patch16(CompiledKeyMap *ckmap, size_t offsetToPatchLocation, uint16_t w)
{
    uint8_t* p = &ckmap->data.bytes[offsetToPatchLocation];
    p[0] = (w >> 8) & 0xff;
    p[1] = (w >> 0) & 0xff;
}

static void patchLabeled16(CompiledKeyMap* ckmap, const void* label, uint16_t w)
{
    for (size_t i = 0; i < ckmap->patchLocs.count; i++) {
        const PatchLocation* pCurPatch = &ckmap->patchLocs.table[i];

        if (pCurPatch->label == label) {
            patch16(ckmap, pCurPatch->offsetToPatchLocation, w);
        }
    }
}

static void ensureCompiledKeyMapSize(CompiledKeyMap* ckmap, size_t minCapacityIncrease)
{
    if (ckmap->data.count > 65535) {
        failed("Compiled key map is too big");
        // NOT REACHED
    }

    if (ckmap->data.count == ckmap->data.capacity) {
        size_t newCapacity = (ckmap->data.capacity > 0) ? ckmap->data.capacity * 2 : 4096;

        if (newCapacity < minCapacityIncrease + ckmap->data.capacity) {
            newCapacity = minCapacityIncrease + ckmap->data.capacity;
        }

        ckmap->data.bytes = (uint8_t*) realloc(ckmap->data.bytes, newCapacity);
        ckmap->data.capacity = newCapacity;
    }
}

// Returns the offset to the 16bit value that was just written out
static size_t write16(CompiledKeyMap* ckmap, uint16_t w)
{
    ensureCompiledKeyMapSize(ckmap, 2);
    patch16(ckmap, ckmap->data.count, w);
    ckmap->data.count += 2;
    return ckmap->data.count - 2;
}

static void write8(CompiledKeyMap* ckmap, uint8_t b)
{
    ensureCompiledKeyMapSize(ckmap, 1);
    ckmap->data.bytes[ckmap->data.count++] = b;
}

static void writeString(CompiledKeyMap* ckmap, const char* str)
{
    ensureCompiledKeyMapSize(ckmap, strlen(str) + 1);
    while (*str != '\0') {
        ckmap->data.bytes[ckmap->data.count++] = *str++;
    }
    ckmap->data.bytes[ckmap->data.count++] = '\0';
}

static void compileKeyMap_Type0(KeyMap* kmap, CompiledKeyMap* ckmap)
{
    // Write the key map
    write16(ckmap, kKeyMapType_0);
    const size_t keyMapSizeOffset = write16(ckmap, 0);  // overall map size
    write16(ckmap, kmap->ranges.count);

    if (kmap->ranges.count > 0) {
        // Write the key range offset slots in the key map header
        const size_t offsetToFirstRangeOffset = getLocationCounter(ckmap);

        for (size_t i = 0; i < kmap->ranges.count; i++) {
            write16(ckmap, 0);
        }


        // Write key ranges
        for (size_t i = 0; i < kmap->ranges.count; i++) {
            const KeyRange* pCurRange = &kmap->ranges.table[i];

            const size_t offsetToCurRange = write16(ckmap, pCurRange->keyType);
            write16(ckmap, kmap->keys.table[pCurRange->lowerKeyIndex].keyCode);
            write16(ckmap, kmap->keys.table[pCurRange->upperKeyIndex].keyCode);
            addPatchLocation(ckmap, write16(ckmap, 0), pCurRange);

            // Back-patch the range offset in the key map header range table
            patch16(ckmap, offsetToFirstRangeOffset + 2 * i, offsetToCurRange);
        }


        // Write one key trap table per range
        for (size_t r = 0; r < kmap->ranges.count; r++) {
            const KeyRange* pCurRange = &kmap->ranges.table[r];

            // Back-patch the offset to the start of the key trap table in the key range record
            patchLabeled16(ckmap, pCurRange, getLocationCounter(ckmap));


            // Write the key traps for the current range
            for (size_t k = pCurRange->lowerKeyIndex; k <= pCurRange->upperKeyIndex; k++) {
                const Key* pCurKey = &kmap->keys.table[k];

                switch (pCurKey->type) {
                    case kKeyType_4Bytes:
                        write8(ckmap, pCurKey->u.b[0]);
                        write8(ckmap, pCurKey->u.b[1]);
                        write8(ckmap, pCurKey->u.b[2]);
                        write8(ckmap, pCurKey->u.b[3]);
                        break;

                    case kKeyType_String:
                        addPatchLocation(ckmap, write16(ckmap, 0), &kmap->strings.table[pCurKey->u.stringIdx]);
                        break;

                    default:
                        abort();
                        break;
                }
            }
        }


        // Write the string table
        for (size_t i = 0; i < kmap->strings.count; i++) {
            const uint16_t offsetToString = getLocationCounter(ckmap);
            writeString(ckmap, kmap->strings.table[i]);
            patchLabeled16(ckmap, kmap->strings.table[i], offsetToString);
        }        
    }


    // Patch the key map size in
    patch16(ckmap, keyMapSizeOffset, ckmap->data.count);
}

static void freeCompiledKeyMap(CompiledKeyMap* ckmap)
{
    if (ckmap) {
        free(ckmap->patchLocs.table);
        free(ckmap->data.bytes);
        memset(ckmap, 0, sizeof(CompiledKeyMap));
    }
}

static void writeKeyMap_Binary(CompiledKeyMap* ckmap, const char *pathToKeysFile)
{
    char* pathToKeymapsFile = createPathWithReplacedExtension(pathToKeysFile, "keymap");
    FILE* s = km_open(pathToKeymapsFile, "wb");

    fwrite(ckmap->data.bytes, 1, ckmap->data.count, s);
    fflush(s);

    if (ferror(s)) {
        failed("Unable to write key map file");
        // NOT REACHED
    }

    fclose(s);
    free(pathToKeymapsFile);
}

static void writeKeyMap_C_Source(CompiledKeyMap* ckmap, const char *pathToKeysFile)
{
    char* pathToKeymapsFile = createPathWithReplacedExtension(pathToKeysFile, "c");
    char* filename = createFilenameFromPath(pathToKeymapsFile);
    FILE* s = km_open(pathToKeymapsFile, "w");
    const size_t nBytesToWrite = ckmap->data.count;
    const size_t nBytesPerRow = 16;
    size_t nBytesWritten = 0;

    fputs("// Auto-generated by the keymap tool.\n", s);
    fputs("// Do not edit.\n\n", s);

    fprintf(s, "const unsigned char gKeyMap_%s[%hu] = {\n", filename, (unsigned short) ckmap->data.count);
    while (nBytesWritten < nBytesToWrite) {
        const size_t nColumns = (nBytesWritten + nBytesPerRow > nBytesToWrite) ? nBytesToWrite - nBytesWritten : nBytesPerRow;

        fputs("   ", s);
        for (size_t i = 0; i < nColumns; i++) {
            fprintf(s, "0x%02hhx", ckmap->data.bytes[nBytesWritten + i]);
            if (i < (nColumns - 1)) {
                fputs(", ", s);
            }
        }
        fputs(",\n", s);
        nBytesWritten += nColumns;
    }
    fputs("};\n\n", s);

    fclose(s);
    free(filename);
    free(pathToKeymapsFile);
}

static void compileKeyMap(const char *pathToKeysFile)
{
    FILE* inFile = km_open(pathToKeysFile, "r");

    KeyMap kmap = {0};
    CompiledKeyMap ckmap = {0};

    parseKeysFile(inFile, &kmap);
    fclose(inFile);
    calculateKeyRanges(&kmap);

    compileKeyMap_Type0(&kmap, &ckmap);
    //writeKeyMap_Binary(&ckmap, pathToKeysFile);
    writeKeyMap_C_Source(&ckmap, pathToKeysFile);

    freeCompiledKeyMap(&ckmap);
    freeKeyMap(&kmap);
}


////////////////////////////////////////////////////////////////////////////////
// Decompile a keymap file
////////////////////////////////////////////////////////////////////////////////

static void readKeyMapFile(FILE* s, Data* data)
{
    uint8_t hdr8[4];
    uint16_t hdr16[2];

    if (fread(hdr8, 1, 4, s) != 4) {
        failed("Unexpected EOF");
        // NOT REACHED
    }

    const uint16_t type = (uint16_t)((((uint8_t)hdr8[0]) << 8) | ((uint8_t)hdr8[1]));
    const uint16_t size = (uint16_t)((((uint8_t)hdr8[2]) << 8) | ((uint8_t)hdr8[3]));

    if (type != kKeyMapType_0) {
        failedf("Unknown key map type: 0x%hx", hdr16[0]);
        // NOT REACHED
    }

    data->bytes = (uint8_t*) malloc(size);
    data->count = size;
    data->capacity = size;

    rewind(s);
    if (fread(data->bytes, 1, size, s) != size) {
        failed("Unexpected EOF");
        // NOT REACHED
    }
}

static uint8_t read8(const Data* data, uint16_t* offset)
{
    if (*offset >= data->count) {
        failedf("Out-of-range offset: %hu (%hu)", *offset, data->count);
        // NOT REACHED
    }
    return data->bytes[(*offset)++];
}

static uint16_t read16(const Data* data, uint16_t* offset)
{
    if (*offset >= data->count) {
        failedf("Out-of-range offset: %hu (%hu)", *offset, data->count);
        // NOT REACHED
    }

    const uint8_t hb = data->bytes[(*offset)++];
    const uint8_t lb = data->bytes[(*offset)++];

    return (uint16_t)((hb << 8) | lb);
}

static void writeCharacterWithEscapingIfNeeded(uint8_t ch, bool isForString, FILE* s)
{
    if (isprint(ch)) {
        fputc(ch, s);
    } else {
        switch (ch) {
            case 0:
                // Make the 0 stand out more compared to all the other hex numbers
                (isForString) ? fputs("\\0", s) : fputc('0', s);
                break;

            case '\\':
                fputs("\\\\", s);
                break;

            case '\n':
                fputs("\\n", s);
                break;

            case '\r':
                fputs("\\r", s);
                break;

            case '\t':
                fputs("\\t", s);
                break;

            case '\b':
                fputs("\\b", s);
                break;

            case '\'':
                fputs("\\'", s);
                break;

            case '\"':
                fputs("\\\"", s);
                break;

            case 0x1b:
                fputs("\\e", s);
                break;

            default:
                if (isForString) {
                    fprintf(s, "\\%hho", ch);
                } else {
                    fprintf(s, "0x%02hx", ch);
                }
                break;
        }
    }
}

static void writeFormattedCharacter(uint8_t ch, FILE* s)
{
    fputc('\'', s);
    writeCharacterWithEscapingIfNeeded(ch, false, s);
    fputc('\'', s);
}

static void writeFormattedString(SmallString str, FILE* s)
{
    const char* p = str;

    fputc('\"', s);
    while (*p != '\0') {
        writeCharacterWithEscapingIfNeeded(*p++, true, s);
    }
    fputc('\"', s);
}

static void decompileKeyTrap_Type0(uint16_t keyTrapOffset, uint16_t usbKeyCode, const Data* data, FILE* out)
{
    uint16_t offset = keyTrapOffset;
    const uint8_t unshifted = read8(data, &offset);
    const uint8_t shifted = read8(data, &offset);
    const uint8_t alted = read8(data, &offset);
    const uint8_t shiftedAlted = read8(data, &offset);

    fprintf(out, "key(0x%04hx, ", usbKeyCode);
    writeFormattedCharacter(unshifted, out);
    fputs(", ", out);
    writeFormattedCharacter(shifted, out);
    fputs(", ", out);
    writeFormattedCharacter(alted, out);
    fputs(", ", out);
    writeFormattedCharacter(shiftedAlted, out);
    fputs(")\n", out);
}

static void decompileKeyTrap_Type3(uint16_t keyTrapOffset, uint16_t usbKeyCode, const Data* data, FILE* out)
{
    uint16_t offset = keyTrapOffset;
    uint16_t stringOffset = read16(data, &offset);
    SmallString str;
    int i = 0;

    while (i < kKeyMap_MaxByteSequenceLength) {
        str[i] = (char) read8(data, &stringOffset);
        if (str[i] == '\0') {
            break;
        }
        i++;
    }
    str[i] = '\0';

    fprintf(out, "key(0x%04hx, ", usbKeyCode);
    writeFormattedString(str, out);
    fputs(")\n", out);
}

static void decompileKeyRange(uint16_t keyRangeOffset, const Data* data, FILE* out)
{
    uint16_t offset = keyRangeOffset;
    const uint16_t type = read16(data, &offset);
    const uint16_t lowerUsbKeyCode = read16(data, &offset);
    const uint16_t upperUsbKeyCode = read16(data, &offset);
    const uint16_t keyTrapsOffset = read16(data, &offset);
    const uint16_t nKeyTraps = upperUsbKeyCode - lowerUsbKeyCode + 1;

    for (uint16_t i = 0; i < nKeyTraps; i++) {
        switch (type) {
            case kKeyType_4Bytes:
                decompileKeyTrap_Type0(keyTrapsOffset + i * 4, lowerUsbKeyCode + i, data, out);
                break;

            case kKeyType_String:
                decompileKeyTrap_Type3(keyTrapsOffset + i * 2, lowerUsbKeyCode + i, data, out);
                break;

            default:
                failedf("Unknown key trap type: 0x%hx", type);
        }
    }
}

static void decompileKeyMap(const char* pathToKeymapsFile)
{
    FILE* in = km_open(pathToKeymapsFile, "rb");
    FILE* out = stdout;
    Data data;

    readKeyMapFile(in, &data);

    uint16_t offset = 0;
    fprintf(out, "// Key map type: 0x%04hu\n", read16(&data, &offset));
    fprintf(out, "// Key map size: %hu\n", read16(&data, &offset));
    const uint16_t nRanges = read16(&data, &offset);
    fprintf(out, "// Key map ranges: %hu\n", nRanges);
    fputc('\n', out);

    for (uint16_t i = 0; i < nRanges; i++) {
        decompileKeyRange(read16(&data, &offset), &data, out);
    }
}


////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    if (argc > 1) {
        if (!strcmp(argv[1], "compile")) {
            if (argc > 2) {
                char* inputPath = argv[2];

                compileKeyMap(inputPath);
                puts("OK");
                return EXIT_SUCCESS;
            }
        }
        else if (!strcmp(argv[1], "decompile")) {
            if (argc > 2) {
                char* inputPath = argv[2];

                decompileKeyMap(inputPath);
                return EXIT_SUCCESS;
            }
        }
    }

    printf("keymap <action> ...\n");
    printf("   compile <path>     Compiles a .keys file to a .keymap file with the same name.\n");
    printf("   decompile <path>   Decompiles a .keymap file and lists its contents.\n");

    return EXIT_FAILURE;
}
