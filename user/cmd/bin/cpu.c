//
//  cpu.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <stdio.h>
#include <serena/host.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("cpu")
);


static void print_68k(cpu_subtype_t subtype)
{
    #define k68k_family_count 6
    static const char* g_68k_cpu_name[k68k_family_count] = {
        "68000", "68010", "68020", "68030", "68040", "68060"
    };
    const int family = cpu_68k_family(subtype) - 1;
    const char* ipu_str = (family < k68k_family_count) ? g_68k_cpu_name[family] : "??";


    #define k68k_fpu_count 5
    static const char* g_68k_fpu_name[k68k_fpu_count] = {
        "", "68881", "68882", "", ""
    };
    const int fpu = cpu_68k_fpu(subtype);
    const char* fpu_str = (fpu < k68k_fpu_count) ? g_68k_fpu_name[fpu] : "??";

    fputs(ipu_str, stdout);
    if (*fpu_str != '\0') {
        fputs(", ", stdout);
        fputs(fpu_str, stdout);
    }
    fputc('\n', stdout);
}

int main(int argc, char* argv[])
{
    host_basic_info_t info;

    clap_parse(0, params, argc, argv);

    if (host_info(HOST_INFO_BASIC, &info) != 0) {
        return EXIT_FAILURE;
    }

    switch (info.cpu_type) {
        case CPU_TYPE_68K:
            print_68k(info.cpu_subtype);
            break;

        default:
            puts("??");
            break;
    }

    return EXIT_SUCCESS;
}
