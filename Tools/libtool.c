//
//  libtool.c
//  libtool
//
//  Created by Dietmar Planitzer on 9/19/23.
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
#include <clap.h>


// To compile on Windows:
// - open a Visual Studio Command Line environment
// - cl /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS libtool.c
//
// To compile on POSIX:
// - open a terminal window
// - gcc libtool.c -o libtool
//


// An 'ar' without all the legacy stuff and things that aren't relevant to the
// use case of creating and maintaining static libraries.

#define AR_MAGIC                    "!<arch>\012"
#define AR_EOL                      "`\012"
#define AR_LONG_STRINGS_MEMBER_NAME "//"
#define AR_SYMBOLS_MEMBER_NAME_BSD  "__.SYMDEF"
#define AR_SYMBOLS_MEMBER_NAME_ELF  "/"
#define AR_PADDED_SIZE(s)   (((s) + 1ul) & ~1ul)

#define AR_MAX_MEMBER_NAME_LENGTH   16
#define AR_MTIME_LENGTH             12
#define AR_UID_LENGTH               6
#define AR_GID_LENGTH               6
#define AR_MODE_LENGTH              8
#define AR_SIZE_LENGTH              10
#define AR_EOL_LENGTH               2

// How to write long names
typedef enum LongNameFormat {
    kLongNameFormat_SystemV4 = 0,
    kLongNameFormat_BSD
} LongNameFormat;

typedef struct ArFileHeader {
    char    magic[8];
} ArFileHeader;

typedef struct ArMemberHeader {
    char    name[AR_MAX_MEMBER_NAME_LENGTH];
    char    mtime[AR_MTIME_LENGTH];
    char    uid[AR_UID_LENGTH];
    char    gid[AR_GID_LENGTH];
    char    mode[AR_MODE_LENGTH];
    char    size[AR_SIZE_LENGTH];
    char    eol[AR_EOL_LENGTH];
} ArMemberHeader;


typedef struct ArchiveMember {
    char*   name;
    size_t  longStringOffset;   // Offset to the member name in the long_strings table if the name is a long string. Only valid if the archive has a long_strings table
    
    char*   data;               // Without padding \n
    size_t  size;
} ArchiveMember;

typedef struct Archive {
    ArchiveMember** members;
    size_t          capacity;
    size_t          count;
    ArchiveMember*  longStrings;    // Holds the long strings separated by '/\n'
    ArchiveMember*  symbolTable;
} Archive;


////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////

static const char* gArgv_Zero = "";

static void vfatal(const char* fmt, va_list ap)
{
    clap_verror(gArgv_Zero, fmt, ap);
    exit(EXIT_FAILURE);
    // NOT REACHED
}

static void fatal(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfatal(fmt, ap);
    va_end(ap);
}


static void* malloc_require(size_t size, bool doClear)
{
    void* p = malloc(size);

    if (p == NULL) {
        fatal("Out of memory");
        // NOT REACHED
    }

    if (doClear) {
        memset(p, 0, size);
    }

    return p;
}

static char* stralloc_require(const char* str, size_t len)
{
    char* p = (char*) malloc(len + 1);

    if (p == NULL) {
        fatal("Out of memory");
        // NOT REACHED
    }

    memcpy(p, str, len);
    p[len] = '\0';

    return p;
}

static void itoa_unterminated(int val, char* buffer, int bufsiz)
{
    char digits[11];

    itoa(val, digits, 10);
    
    const int ndigits = (int)strlen(digits);
    if (ndigits > bufsiz) {
        fatal("Overflow");
        // NOT REACHED
    }

    for (int i = 0; i < ndigits; i++) {
        buffer[i] = digits[i];
    }
}

static FILE* open_require(const char* filename, const char* mode)
{
    FILE* s = fopen(filename, mode);

    if (s == NULL) {
        fatal("Unable to open '%s'", filename);
        // NOT REACHED
    }
    return s;
}

static void fread_require(void* data, size_t size, FILE* s)
{
    if (fread(data, size, 1, s) < 1) {
        fatal("I/O error");
        // NOT REACHED
    }
}

static void fwrite_require(const void* data, size_t size, FILE* s)
{
    if (fwrite(data, size, 1, s) < 1) {
        fatal("I/O error");
        // NOT REACHED
    }
}

static char* createFilenameFromPath(const char* path)
{
    const char* pathFilename = strrchr(path, '/');
    if (pathFilename == NULL) pathFilename = strrchr(path, '\\');
    pathFilename = (pathFilename) ? pathFilename + 1 : path;
    const size_t filenameLength = strlen(pathFilename);
    char* filename = malloc_require(filenameLength + 1, false);
    memcpy(filename, pathFilename, filenameLength);
    filename[filenameLength] = '\0';

    return filename;
}


////////////////////////////////////////////////////////////////////////////////
// Archive
////////////////////////////////////////////////////////////////////////////////

static ArchiveMember* ArchiveMember_CreateFromPath(const char* objPath)
{
    ArchiveMember* pMember = (ArchiveMember*) malloc_require(sizeof(ArchiveMember), true);
    
    pMember->name = createFilenameFromPath(objPath);
    pMember->longStringOffset = 0;

    FILE* s = open_require(objPath, "rb");
    fseek(s, 0, SEEK_END);
    pMember->size = ftell(s);
    rewind(s);
    pMember->data = malloc_require(pMember->size, false);
    fread_require(pMember->data, pMember->size, s);

    return pMember;
}

static void _ArchiveMember_ParseHeader(ArchiveMember* pMember, Archive* pArchive, const ArMemberHeader* hdr, FILE* s)
{
    size_t nameLen;
    const char* pName;

    // Get the data size
    pMember->size = strtoul(hdr->size, NULL, 10);
    if (pMember->size == 0) {
        fatal("Corrupt library file");
        // NOT REACHED
    }


    // Get the member name. Note that the pMember->size may have to be reduced
    // by the member name length in some cases
    if (pArchive->longStrings && hdr->name[0] == '/' && isdigit(hdr->name[1])) {
        // System V.4 long name
        pMember->longStringOffset = strtoul(&hdr->name[1], NULL, 10);
        if (pMember->longStringOffset >= pArchive->longStrings->size - 2) {
            fatal("Corrupt library file");
            // NOT REACHED
        }

        // System V.4 is '/\n' and Windows (COFF) is '\0'
        char* p = &pArchive->longStrings->data[pMember->longStringOffset];
        char* ep = p;
        while ((*(ep + 0) != '/' && *(ep + 1) != '\012') || (*ep == '\0')) {
            ep++;
        }
        nameLen = ep - p;
        pName = p;
    }
    else if (hdr->name[0] == '#' && hdr->name[1] == '1' && hdr->name[2] == '/' && isdigit(hdr->name[3])) {
        // BSD long name
        nameLen = strtoul(&hdr->name[3], NULL, 10);
        if (nameLen == 0 || nameLen >= pMember->size) {
            fatal("Corrupt library file");
            // NOT REACHED
        }

        pMember->name = (char*) malloc_require(nameLen + 1, false);
        fread_require(pMember->name, nameLen, s);
        pMember->name[nameLen] = '\0';
        pMember->longStringOffset = 0;
        pMember->size -= nameLen;

        nameLen = 0;
        pName = NULL;
    }
    else {
        // Short name or one of the special names
        pMember->longStringOffset = 0;
        nameLen = AR_MAX_MEMBER_NAME_LENGTH;
        for (int i = 15; i >= 0; i--) {
            if (hdr->name[i] != ' ') {
                if (hdr->name[i] == '/') {
                    if (i == 0 || (i == 1 && hdr->name[0] == '/')) {
                        // Preserve the special names '/' and '//' because we need more needless complexity for the sake of complexity
                        ;
                    } else {
                        nameLen--;
                    }
                }
                break;
            }
            nameLen--;
        }
        pName = hdr->name;
    }

    if (pName) {
        if (nameLen == 0) {
            fatal("Corrupt library file");
            // NOT REACHED
        }

        pMember->name = stralloc_require(pName, nameLen);
    }
}

static ArchiveMember* ArchiveMember_CreateFromArchive(Archive* pArchive, FILE* s)
{
    ArMemberHeader  hdr;

    const size_t nRead = fread(&hdr, sizeof(hdr), 1, s);
    if (nRead == 0) {
        if (feof(s)) {
            return NULL;
        } else {
            fatal("I/O error");
            // NOT REACHED
        }
    }
    if (strncmp(hdr.eol, AR_EOL, 2)) {
        fatal("Corrupt library file");
        // NOT REACHED
    }

    ArchiveMember* pMember = (ArchiveMember*) malloc_require(sizeof(ArchiveMember), true);


    // Parse the meta information from the archive member header.
    _ArchiveMember_ParseHeader(pMember, pArchive, &hdr, s);


    // Get the data
    pMember->data = malloc_require(pMember->size, false);
    fread_require(pMember->data, pMember->size, s);

    return pMember;
}

static bool ArchiveMember_IsLongStrings(ArchiveMember* pMember)
{
    return !strcmp(pMember->name, AR_LONG_STRINGS_MEMBER_NAME);
}

static bool ArchiveMember_IsSymbolTable(ArchiveMember* pMember)
{
    return !strcmp(pMember->name, AR_SYMBOLS_MEMBER_NAME_BSD) || !strcmp(pMember->name, AR_SYMBOLS_MEMBER_NAME_ELF);
}

static void ArchiveMember_Destroy(ArchiveMember* pMember)
{
    if (pMember) {
        free(pMember->name);
        pMember->name = NULL;
        free(pMember->data);
        pMember->data = NULL;
        free(pMember);
    }
}


static void Archive_InsertMemberAt(Archive* pArchive, ArchiveMember* pMember, size_t idx)
{
    if (pArchive->count == pArchive->capacity) {
        size_t newCapacity = pArchive->capacity * 2;
        ArchiveMember** newMembers = malloc_require(sizeof(void*) * newCapacity, false);

        memcpy(newMembers, pArchive->members, pArchive->count * sizeof(void*));
        free(pArchive->members);
        pArchive->members = newMembers;
        pArchive->capacity = newCapacity;
    }

    if (idx < pArchive->count) {
        memmove(&pArchive->members[idx + 1], &pArchive->members[idx], (pArchive->count - idx) * sizeof(void*));
    }
    pArchive->members[idx] = pMember;
    pArchive->count++;
}

static void Archive_AddMember(Archive* pArchive, ArchiveMember* pMember)
{
    Archive_InsertMemberAt(pArchive, pMember, pArchive->count);
}

static Archive* Archive_Create(void)
{
    Archive* pArchive = malloc_require(sizeof(Archive), true);

    pArchive->count = 0;
    pArchive->capacity = 8;
    pArchive->members = malloc_require(sizeof(void*) * pArchive->capacity, false);

    return pArchive;
}

static Archive* Archive_CreateFromPath(const char* path)
{
    FILE* s = open_require(path, "rb");
    ArFileHeader hdr;

    // Read the header
    fread_require(&hdr, sizeof(hdr), s);
    if (strncmp(hdr.magic, AR_MAGIC, 8)) {
        fatal("Not a library file");
        // NOT REACHED
    }

    Archive* pArchive = Archive_Create();


    // Read the archive members
    for (;;) {
        ArchiveMember* pMember = ArchiveMember_CreateFromArchive(pArchive, s);

        if (pMember == NULL) {
            break;
        }
        else if (ArchiveMember_IsLongStrings(pMember)) {
            pArchive->longStrings = pMember;
        }
        else if (ArchiveMember_IsSymbolTable(pMember)) {
            pArchive->symbolTable = pMember;
        }
        else {
            Archive_AddMember(pArchive, pMember);
        }
    }

    fclose(s);

    return pArchive;
}

static void Archive_Destroy(Archive* pArchive)
{
    if (pArchive) {
        for (size_t i = 0; i < pArchive->count; i++) {
            ArchiveMember_Destroy(pArchive->members[i]);
            pArchive->members[i] = NULL;
        }
        free(pArchive->longStrings);
        pArchive->longStrings = NULL;
        free(pArchive->symbolTable);
        pArchive->symbolTable = NULL;
        free(pArchive);
    }
}

static void Archive_GenerateLongStrings(Archive* pArchive)
{
    char* pLongStrings = NULL;
    size_t count = 0;
    size_t capacity = 0;

    for (size_t i = 0; i < pArchive->count; i++) {
        ArchiveMember* pMember = pArchive->members[i];
        const size_t nameLen = strlen(pMember->name);

        if (nameLen > AR_MAX_MEMBER_NAME_LENGTH) {
            if (count == capacity) {
                const size_t newCapacity = (capacity + nameLen + 2 < capacity * 2) ? capacity * 2 : capacity + nameLen + 2;
                char* pNewStrings = (char*) malloc_require(newCapacity, false);

                if (count > 0) {
                    memcpy(pNewStrings, pLongStrings, count);
                }
                free(pLongStrings);
                pLongStrings = pNewStrings;
                capacity = newCapacity;
            }

            memcpy(&pLongStrings[count], pMember->name, nameLen);
            memcpy(&pLongStrings[count + nameLen], "/\012", 2);
            pMember->longStringOffset = count;
            count += nameLen;
            count += 2;
        }
    }

    if (count > 0) {
        ArchiveMember* pMember = (ArchiveMember*) malloc_require(sizeof(ArchiveMember), true);
        pMember->name = stralloc_require(AR_LONG_STRINGS_MEMBER_NAME, strlen(AR_LONG_STRINGS_MEMBER_NAME));
        pMember->longStringOffset = 0;
        pMember->data = pLongStrings;
        pMember->size = count;
        pArchive->longStrings = pMember;
    }
}

static void ArchiveMember_Write(ArchiveMember* pMember, FILE* s, LongNameFormat longNameFormat)
{
    ArMemberHeader hdr;
    const size_t nameLen = strlen(pMember->name);
    size_t memberSize = pMember->size;
    bool isExtendedName = false; 

    memset(&hdr, ' ', sizeof(hdr));
    
    switch (longNameFormat) {
        case kLongNameFormat_SystemV4:
            if (nameLen > AR_MAX_MEMBER_NAME_LENGTH) {
                sprintf(hdr.name, "/%zu", pMember->longStringOffset);
            } else {
                memcpy(&hdr.name, pMember->name, nameLen);
                if (nameLen < AR_MAX_MEMBER_NAME_LENGTH) {
                    hdr.name[nameLen] = '/';
                }
            }
            break;

        case kLongNameFormat_BSD: {
            bool hasSpace = false;
            for (int i = 0; i < nameLen; i++) {
                if (isspace(pMember->name[i])) {
                    hasSpace = true;
                    break;
                }
            }
            if (hasSpace || nameLen > AR_MAX_MEMBER_NAME_LENGTH) {
                memcpy(hdr.name, "#1/", 3);
                itoa_unterminated((int)nameLen, &hdr.name[3], AR_MAX_MEMBER_NAME_LENGTH - 3);
                memberSize += nameLen;
                isExtendedName = true;
            } else {
                memcpy(&hdr.name, pMember->name, nameLen);
            }
            break;
        }

        default:
            abort();
            break;
    }

    itoa_unterminated(0, hdr.mtime, AR_MTIME_LENGTH);
    itoa_unterminated(0, hdr.uid, AR_UID_LENGTH);
    itoa_unterminated(0, hdr.gid, AR_GID_LENGTH);
    itoa_unterminated(0600, hdr.mode, AR_MODE_LENGTH);
    itoa_unterminated((int)memberSize, hdr.size, AR_SIZE_LENGTH);
    memcpy(hdr.eol, AR_EOL, AR_EOL_LENGTH);

    fwrite_require(&hdr, sizeof(hdr), s);

    if (longNameFormat == kLongNameFormat_BSD && isExtendedName) {
        fwrite_require(pMember->name, nameLen, s);
    }

    fwrite_require(pMember->data, pMember->size, s);
    if (AR_PADDED_SIZE(memberSize) > memberSize) {
        fwrite_require("\012", 1, s);
    }
}

// Write a System V.4 style archive 
static void Archive_Write(Archive* pArchive, const char* libPath, LongNameFormat longNameFormat)
{
    FILE* s = open_require(libPath, "wb");
    ArFileHeader hdr;

    memcpy(hdr.magic, AR_MAGIC, 8);
    fwrite_require(&hdr, sizeof(hdr), s);

    // XXX add support for symbol tables one day

    if (longNameFormat == kLongNameFormat_SystemV4) {
        if (pArchive->longStrings == NULL) {
            Archive_GenerateLongStrings(pArchive);
        }

        if (pArchive->longStrings) {
            ArchiveMember_Write(pArchive->longStrings, s, longNameFormat);
        }
    }

    for (size_t i = 0; i < pArchive->count; i++) {
        ArchiveMember_Write(pArchive->members[i], s, longNameFormat);
    }

    fclose(s);
}


////////////////////////////////////////////////////////////////////////////////
// Create Library
////////////////////////////////////////////////////////////////////////////////

static void createLibrary(const char* libPath, const char** objPaths, size_t nObjPaths, LongNameFormat longNameFormat)
{
    Archive* pArchive = Archive_Create();

    for (size_t i = 0; i < nObjPaths; i++) {
        Archive_AddMember(pArchive, ArchiveMember_CreateFromPath(objPaths[i]));
    }

    Archive_Write(pArchive, libPath, longNameFormat);
    Archive_Destroy(pArchive);
}


////////////////////////////////////////////////////////////////////////////////
// List Library
////////////////////////////////////////////////////////////////////////////////

static void listLibrary(const char* libPath)
{
    Archive* pArchive = Archive_CreateFromPath(libPath);
    size_t nameLenMax = 0;

    for (size_t i = 0; i < pArchive->count; i++) {
        ArchiveMember* pMember = pArchive->members[i];
        const size_t nameLen = strlen(pMember->name);

        if (nameLen > nameLenMax) {
            nameLenMax = nameLen;
        }
    }

    for (size_t i = 0; i < pArchive->count; i++) {
        ArchiveMember* pMember = pArchive->members[i];
        const size_t nameLen = strlen(pMember->name);
        const size_t nSpaces = nameLenMax - nameLen + 3;

        fputs(pMember->name, stdout);
        for (size_t j = 0; j < nSpaces; j++) {
            fputc(' ', stdout);
        }
        printf("(%zu bytes)\n", pMember->size);
    }

    Archive_Destroy(pArchive);
}


////////////////////////////////////////////////////////////////////////////////
// main
////////////////////////////////////////////////////////////////////////////////

clap_string_array_t paths = {NULL, 0};
const char* cmd_id = "";

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("libtool <command> ..."),

    CLAP_REQUIRED_COMMAND("create", &cmd_id, "<lib_path> <obj_path ...>", "Builds a new static library from a list of object files. Replaces the library file at 'lib_path' if it already exists."),
        CLAP_VARARG(&paths),
    CLAP_REQUIRED_COMMAND("list", &cmd_id, "<lib_path>", "Lists all object files stored in the library file."),
        CLAP_VARARG(&paths)
);


int main(int argc, char* argv[])
{
    gArgv_Zero = argv[0];
    clap_parse(0, params, argc, argv);

    if (!strcmp("create", cmd_id)) {
        if (paths.count < 2) {
            fatal("expected a library name and at least one object file");
            // not reached
        }
        createLibrary(paths.strings[0], &paths.strings[1], paths.count - 1, kLongNameFormat_BSD);
    }
    else if (!strcmp("list", cmd_id)) {
        for (size_t i = 0; i < paths.count; i++) {
            if (paths.count > 1) {
                if (i > 0) {
                    putchar('\n');
                }
                printf("%s:\n", paths.strings[i]);
            }
            listLibrary(paths.strings[i]);
        }
    }

    return EXIT_SUCCESS;
}
