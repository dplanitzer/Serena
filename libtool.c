//
//  libtool.c
//  Apollo
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


// To compile on Windows:
// - open a Visual Studio Command Line environment
// - cd to the Apollo folder
// - cl libtool.c
//
// To compile on POSIX:
// - open a terminal window
// - cd to the Apollo folder
// - gcc libtool.c -o libtool
//


// An 'ar' without all the legacy stuff and things that aren't relevant to the
// use case of creating and maintaining static libraries.

#define AR_MAGIC                    "!<arch>\012"
#define AR_EOL                      "`\012"
#define AR_LONG_STRINGS_MEMBER_NAME "//"
#define AR_SYMBOLS_MEMBER_NAME_BSD  "__.SYMDEF"
#define AR_SYMBOLS_MEMBER_NAME_ELF  "/"
#define AR_MAX_MEMBER_NAME_LENGTH   16
#define AR_PADDED_SIZE(s)   (((s) + 1ul) & ~1ul)


typedef struct _ArFileHeader {
    char    magic[8];
} ArFileHeader;

typedef struct _ArMemberHeader {
    char    name[AR_MAX_MEMBER_NAME_LENGTH];
    char    mtime[12];
    char    uid[6];
    char    gid[6];
    char    mode[8];
    char    size[10];
    char    eol[2];
} ArMemberHeader;


typedef struct _ArchiveMember {
    char*   name;
    size_t  longStringOffset;   // Offset to the member name in the long_strings table if the name is a long string. Only valid if the archive has a long_strings table
    
    char*   data;               // Without padding \n
    size_t  size;
    size_t  paddedSize;
} ArchiveMember;

typedef struct _Archive {
    ArchiveMember** members;
    size_t          capacity;
    size_t          count;
    ArchiveMember*  longStrings;    // Holds the long strings separated by '/\n'
    ArchiveMember*  symbolTable;
} Archive;


////////////////////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////////////////////

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

static void* malloc_require(size_t size, bool doClear)
{
    void* p = malloc(size);

    if (p == NULL) {
        failed("Out of memory");
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
        failed("Out of memory");
        // NOT REACHED
    }

    memcpy(p, str, len);
    p[len] = '\0';

    return p;
}

static FILE* open_require(const char* filename, const char* mode)
{
    FILE* s = fopen(filename, mode);

    if (s == NULL) {
        failedf("Unable to open '%s'", filename);
        // NOT REACHED
    }
    return s;
}

static void fread_require(void* data, size_t size, FILE* s)
{
    if (fread(data, size, 1, s) < 1) {
        failed("I/O error");
        // NOT REACHED
    }
}

static void fwrite_require(const void* data, size_t size, FILE* s)
{
    if (fwrite(data, size, 1, s) < 1) {
        failed("I/O error");
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
    pMember->paddedSize = AR_PADDED_SIZE(pMember->size);
    pMember->data = malloc_require(pMember->size, false);
    fread_require(pMember->data, pMember->size, s);

    return pMember;
}

static void _ArchiveMember_ParseFilenameFromArchive(ArchiveMember* pMember, Archive* pArchive, const ArMemberHeader* hdr, FILE* s)
{
    size_t nameLen;
    const char* pName;

    if (pArchive->longStrings && hdr->name[0] == '/' && isdigit(hdr->name[1])) {
        // System V.4 long name
        pMember->longStringOffset = strtoul(&hdr->name[1], NULL, 10);
        if (pMember->longStringOffset >= pArchive->longStrings->size - 2) {
            failed("Corrupt library file");
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
        nameLen = strtoul(&hdr->name[4], NULL, 10);
        if (nameLen == 0 || nameLen >= pMember->size) {
            failed("Corrupt library file");
            // NOT REACHED
        }

        pMember->name = (char*) malloc_require(nameLen + 1, false);
        fread_require(pMember->name, nameLen, s);
        pMember->name[nameLen] = '\0';
        pMember->longStringOffset = 0;

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
            failed("Corrupt library file");
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
            failed("I/O error");
            // NOT REACHED
        }
    }
    if (strncmp(hdr.eol, AR_EOL, 2)) {
        failed("Corrupt library file");
        // NOT REACHED
    }

    ArchiveMember* pMember = (ArchiveMember*) malloc_require(sizeof(ArchiveMember), true);


    // Get the data size
    pMember->size = strtoul(hdr.size, NULL, 10);
    if (pMember->size == 0) {
        failed("Corrupt library file");
        // NOT REACHED
    }


    // Get the member name.
    _ArchiveMember_ParseFilenameFromArchive(pMember, pArchive, &hdr, s);


    // Get the data
    pMember->paddedSize = AR_PADDED_SIZE(pMember->size);
    pMember->data = malloc_require(pMember->paddedSize, false);
    fread_require(pMember->data, pMember->paddedSize, s);

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
        failed("Not a library file");
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
        pMember->paddedSize = AR_PADDED_SIZE(count);
        pArchive->longStrings = pMember;
    }
}

static void ArchiveMember_Write(ArchiveMember* pMember, FILE* s)
{
    ArMemberHeader hdr;

    memset(&hdr, ' ', sizeof(hdr));
    
    const size_t nameLen = strlen(pMember->name);
    if (nameLen > AR_MAX_MEMBER_NAME_LENGTH) {
        sprintf(hdr.name, "/%zu", pMember->longStringOffset);
    } else {
        memcpy(&hdr.name, pMember->name, nameLen);
        if (nameLen < AR_MAX_MEMBER_NAME_LENGTH) {
            hdr.name[nameLen] = '/';
        }
    }
    sprintf(hdr.mtime, "%d", 0);
    sprintf(hdr.uid, "%d", 0);
    sprintf(hdr.gid, "%d", 0);
    sprintf(hdr.mode, "%d", 0600);
    sprintf(hdr.size, "%zu", pMember->size);
    memcpy(hdr.eol, AR_EOL, 2);

    fwrite_require(&hdr, sizeof(hdr), s);
    fwrite_require(pMember->data, pMember->size, s);
    if (pMember->paddedSize > pMember->size) {
        fwrite_require("\012", 1, s);
    }
}

// Write a System V.4 style archive 
static void Archive_Write(Archive* pArchive, const char* libPath)
{
    FILE* s = open_require(libPath, "wb");
    ArFileHeader hdr;

    memcpy(hdr.magic, AR_MAGIC, 8);
    fwrite_require(&hdr, sizeof(hdr), s);

    // XXX add support for symbol tables one day

    if (pArchive->longStrings == NULL) {
        Archive_GenerateLongStrings(pArchive);
    }

    if (pArchive->longStrings) {
        ArchiveMember_Write(pArchive->longStrings, s);
    }

    for (size_t i = 0; i < pArchive->count; i++) {
        ArchiveMember_Write(pArchive->members[i], s);
    }

    fclose(s);
}


////////////////////////////////////////////////////////////////////////////////
// Create Library
////////////////////////////////////////////////////////////////////////////////

static void createLibrary(const char* libPath, char* objPaths[], int nObjPaths)
{
    Archive* pArchive = Archive_Create();

    for (size_t i = 0; i < nObjPaths; i++) {
        Archive_AddMember(pArchive, ArchiveMember_CreateFromPath(objPaths[i]));
    }

    Archive_Write(pArchive, libPath);
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

int main(int argc, char* argv[])
{
    if (argc > 1) {
        if (!strcmp(argv[1], "create")) {
            if (argc > 2) {
                char* libPath = argv[2];

                createLibrary(libPath, &argv[3], argc - 3);
                return EXIT_SUCCESS;
            }
        }
        else if (!strcmp(argv[1], "list")) {
            if (argc > 1) {
                char* libPath = argv[2];

                listLibrary(libPath);
                return EXIT_SUCCESS;
            }
        }
    }

    printf("libtool <action> ...\n");
    printf("   create <lib_path> <a.out_path> ...   Creates a static library from a list of a.out files. Replaces 'lib_path' if it already exists.\n");
    printf("   list <lib_path>                      Lists the a.out files stored inside the library file.\n");

    return EXIT_FAILURE;
}
