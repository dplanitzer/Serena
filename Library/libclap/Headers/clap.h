//
//  clap.h
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _CLAP_H
#define _CLAP_H 1

#ifdef __SERENA__
#include <System/_cmndef.h>
#else
#ifndef __has_feature
#define __has_feature(x) 0
#endif


#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

#ifdef __cplusplus
#define __CPP_BEGIN extern "C" {
#else
#define __CPP_BEGIN
#endif

#ifdef __cplusplus
#define __CPP_END }
#else
#define __CPP_END
#endif

#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

__CPP_BEGIN

struct clap_param_t;


// The type of value that a parameter expects.
enum clap_type {
    // Value parameters
    clap_type_boolean = 0,      // bool
    clap_type_integer,          // int
    clap_type_string,           // const char*
    clap_type_string_array,     // clap_string_array_t
    clap_type_enum,             // int (index of selected enum value)
    clap_type_command,          // int (index value of the selected command)

    // Semantic parameters
    clap_type_help,             // a parameter which triggers the display of a help message
    clap_type_section,          // a help section (title)
    clap_type_end,              // marks the end of the parameter declarations
};


// A string array stores 'count' pointers to strings. Note that this is not a
// NULL terminated array. Each array entry corresponds to a string that the user
// provided on the command line.
typedef struct clap_string_array_t {
    const char* _Nonnull * _Nonnull strings;    // array of 'count' string pointers
    size_t                          count;      // number of entries in the array
} clap_string_array_t;


// A command enumeration. It consists of two tables: a NULL terminated array of
// command (enumeration) names plus a parallel array of per-command parameter
// lists. Note that the parameter table does not have to be NULL terminated.
// NULL termination is only required or the names array. Once a command has been
// detected, the active parameter list is switched to the one associated with the
// command and all the following command line arguments are parsed according to
// the rules defined by the new parameter list. 
typedef struct clap_command_enum_t {
    const char * _Nonnull * _Nullable           names;  // NULL terminated array of command names
    struct clap_param_t* _Nonnull * _Nonnull    params; // Array of command specific parameters with one entry per command
} clap_command_enum_t;


// The command line syntax help information. 'usages' is a NULL terminated array
// of usage strings. 'prolog' is a string that is displayed after the last usage
// string and before the parameter descriptions and 'epilog' is a string that is
// displayed after the last parameter description.
typedef struct clap_help_t {
    const char* _Nullable * _Nullable   usages;
    const char* _Nullable               prolog;
    const char* _Nullable               epilog;
} clap_help_t;


enum clap_flag {
    clap_flag_required = 1,                 // The user must provide this parameter in the command line
    clap_flag_appeared = 2,                 // This parameter appeared in the command line
    clap_flag_positionally_bindable = 4,    // This parameter is able to bind to a positional parameter
    clap_flag_positionally_bound = 8,       // The value of this parameter was bound via positional syntax
};


// A command line parameter. Use the macros defined below to define the parameter
// list.
typedef struct clap_param_t {
    uint8_t                 type;
    uint8_t                 flags;
    const char              short_label;
    const char* _Nullable   long_label;
    const char* _Nonnull    help;
    void* _Nonnull          value;
    union {
        const char* _Nonnull * _Nullable    enum_strings;  // NULL terminated array of enum values
        clap_command_enum_t* _Nonnull       cmds;       
        clap_help_t                         help;
        const char*                         title;
    }                       u;
} clap_param_t;


// Declares a parameter list. Use it like so:
// CLAP_DECL(my_params,
//    CLAP_BOOL(...),
//    CLAP_REQUIRED_STRING(...)
//    ...
//    CLAP_INT(...)
// );
// Note that the CLAP_DECL macros automatically adds the required CLAP_END()
// macro invocation.
#define CLAP_DECL(__params_name, ...) \
clap_param_t __params_name[] = { __VA_ARGS__, CLAP_END() }


// Defines an optional/required boolean (switch) parameter. '__bptr' is expected
// to point to a bool variable.
#define CLAP_BOOL(__short_label, __long_label, __bptr, __help) \
{clap_type_boolean, 0, __short_label, __long_label, __help, (void*)__bptr}

#define CLAP_REQUIRED_BOOL(__short_label, __long_label, __bptr, __help) \
{clap_type_boolean, clap_flag_required, __short_label, __long_label, __help, (void*)__bptr}


// Defines an optional/required integer (option) parameter. '__iptr' is expected
// to point to an int variable.
#define CLAP_INT(__short_label, __long_label, __iptr, __help) \
{clap_type_integer, 0, __short_label, __long_label, __help, (void*)__iptr}

#define CLAP_REQUIRED_INT(__short_label, __long_label, __iptr, __help) \
{clap_type_integer, clap_flag_required, __short_label, __long_label, __help, (void*)__iptr}


// Defines an optional/required string (option) parameter. '__sptr' is expected
// to point to a string pointer variable.
#define CLAP_STRING(__short_label, __long_label, __sptr, __help) \
{clap_type_string, 0, __short_label, __long_label, __help, (void*)__sptr}

#define CLAP_REQUIRED_STRING(__short_label, __long_label, __sptr, __help) \
{clap_type_string, clap_flag_required, __short_label, __long_label, __help, (void*)__sptr}

// Defines an optional/required positional string parameter. 
#define CLAP_POSITIONAL_STRING(__sptr, __help) \
{clap_type_string, clap_flag_positionally_bindable, '\0', "", __help, (void*)__sptr}

#define CLAP_REQUIRED_POSITIONAL_STRING(__sptr, __help) \
{clap_type_string, clap_flag_required | clap_flag_positionally_bindable, '\0', "", __help, (void*)__sptr}


// Defines an optional/required string array (option) parameter. '__saptr' is
// expected to point to a clap_string_array_t variable. Note that the array
// backing store is owned by libclap and is guaranteed to exist through the
// lifetime of the process. You should not attempt to modify the array. Copy it
// if you want to add/remove/replace array entries.
#define CLAP_STRING_ARRAY(__short_label, __long_label, __saptr, __help) \
{clap_type_string_array, 0, __short_label, __long_label, __help, (void*)__saptr}

#define CLAP_REQUIRED_STRING_ARRAY(__short_label, __long_label, __saptr, __help) \
{clap_type_string_array, clap_flag_required, __short_label, __long_label, __help, (void*)__saptr}

// Defines an optional/required positional string array parameter.
#define CLAP_POSITIONAL_STRING_ARRAY(__saptr, __help) \
{clap_type_string_array, clap_flag_positionally_bindable, '\0', "", __help, (void*)__saptr}

#define CLAP_REQUIRED_POSITIONAL_STRING_ARRAY(__saptr, __help) \
{clap_type_string_array, clap_flag_required | clap_flag_positionally_bindable, '\0', "", __help, (void*)__saptr}


// Defines an optional/required (string) enumeration) (option) parameter. '__iptr'
// is expected to point to an int variable and it will be set to the index of the
// enumeration names array entry that matches what the user provided on the
// command line. 
#define CLAP_ENUM(__short_label, __long_label,  __iptr, __strs, __help) \
{clap_type_enum, 0, __short_label, __long_label, __help, (void*)__iptr, {.enum_strings = __strs}}

#define CLAP_REQUIRED_ENUM(__short_label, __long_label, __iptr, __strs, __help) \
{clap_type_enum, clap_flag_required, __short_label, __long_label, __help, (void*)__iptr, {.enum_strings = __strs}}

// Defines an optional/required positional string enumeration parameter.
#define CLAP_POSITIONAL_ENUM(__iptr, __strs, __help) \
{clap_type_enum, clap_flag_positionally_bindable, '\0', "", __help, (void*)__iptr, {.enum_strings = __strs}}

#define CLAP_REQUIRED_POSITIONAL_ENUM(__iptr, __strs, __help) \
{clap_type_enum, clap_flag_required | clap_flag_positionally_bindable, '\0', "", __help, (void*)__iptr, {.enum_strings = __strs}}


// Defines an optional/required command (option) parameter. '__iptr' is expected
// to point to an int variable and it will be set to the index of the command
// names array entry that matches the command name that the user provided on the
// command line.
#define CLAP_COMMAND(__short_label, __long_label,  __iptr, __cmds, __help) \
{clap_type_command, 0, __short_label, __long_label, __help, (void*)__iptr, {.cmds = __cmds}}

#define CLAP_REQUIRED_COMMAND(__short_label, __long_label, __iptr, __cmds, __help) \
{clap_type_command, clap_flag_required, __short_label, __long_label, __help, (void*)__iptr, {.cmds = __cmds}}

// Defines an optional/required positional command parameter.
#define CLAP_POSITIONAL_COMMAND(__iptr, __cmds, __help) \
{clap_type_command, clap_flag_positionally_bindable, '\0', "", __help, (void*)__iptr, {.cmds = __cmds}}

#define CLAP_REQUIRED_POSITIONAL_COMMAND(__iptr, __cmds, __help) \
{clap_type_command, clap_flag_required | clap_flag_positionally_bindable, '\0', "", __help, (void*)__iptr, {.cmds = __cmds}}


// __usages: a NULL terminates string array with one entry per possible usage
// __prolog: displayed after the usages lines and before the parameter descriptions
// __epilog: displayed at the very end of the help page
#define CLAP_HELP(__usages, __prolog, __epilog) \
{clap_type_help, 0, 'h', "help", "Print help", "", {.help = {__usages, __prolog, __epilog}}}

// __title: the help section title
#define CLAP_SECTION(__title) \
{clap_type_section, 0, '\0', "", "", "", {.title = __title}}


// Marks the end of a parameter list. This entry is automatically added by the
// CLAP_DECL() macro.
#define CLAP_END() \
{clap_type_end, clap_flag_required, '\0', "", "", ""}



// Parses the provided command line arguments based on the syntax rules defined
// by the 'params' parameter list. Prints an appropriate error and terminates the
// process with a failure exit code if a syntax or semantic error is detected.
extern void clap_parse(clap_param_t* _Nonnull params, int argc, const char** argv);

__CPP_END

#endif /* _CLAP_H */
