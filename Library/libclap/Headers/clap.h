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


// A command line argument parser library.
//
// The libclap parser supports named and positional parameters and parameters of
// various different types. A named parameter is a parameter that starts with a
// short or long label. A short label starts with a single dash while a long
// label starts with a double dash.
// A positional parameter has no label. It picks up whatever arguments appear on
// the command line that are not immediately following a named parameter label.
// A parameter may be declared as required or optional: all parameters are
// optional by default and a parameter that should be required must be declared
// as such.
// A named parameter takes a single or multiple values. A single valued parameter
// may be written in one of these ways:
// --foo 10
// --foo=10
// Mutiple named short label parameters may be clustered like this:
// -abc
// where 'a', 'b' and 'c' are separate parameters. Alternatively you could have
// written:
// -a -b -c
// instead.
// The '--' (double dash surrounded by whitespace) parameter signals that the
// rest of the command line should be exclusively treated as parameter values
// and not as (named) parameters. Note that the '--' itself is transparent. This
// means that it will never appear as a value in the parser output.
// A parameter may be of type string array. A string array is a list of parameter
// values that is terminated either by the end of the command line or by a short
// or long label of a named parameter.


// The type of value that a parameter expects.
enum clap_type {
    // Value parameters
    clap_type_boolean = 0,      // bool
    clap_type_integer,          // int
    clap_type_string,           // const char*
    clap_type_string_array,     // clap_string_array_t
    clap_type_enum,             // int (index of selected enum value)
    clap_type_command,          // int (index value of the selected command)
    clap_type_value,            // A value of a type defined by a callback function

    // Semantic parameters
    clap_type_vararg,           // all unnamed/positional parameters at the end of the command line
    clap_type_version,          // an option to print version information
    clap_type_help,             // an option to print the help page
    clap_type_usage,            // a usage line in the help page
    clap_type_prolog,           // a prolog paragraph in the help page
    clap_type_section,          // a help section (title)
    clap_type_epilog,           // an epilog paragraph in the help page
    clap_type_end,              // marks the end of the parameter declarations
};


// A string array stores 'count' pointers to strings. Note that this is not a
// NULL terminated array. Each array entry corresponds to a string that the user
// provided on the command line.
typedef struct clap_string_array_t {
    const char* _Nonnull * _Nonnull strings;    // array of 'count' string pointers
    size_t                          count;      // number of entries in the array
} clap_string_array_t;


// A command. Each command has a name, an optional usage and help line and command
// specific parameters. A tool may define multiple command however the user can
// only select one command per tool invocation. The parameters that are associated
// with a command are only activated and interpreted if the user selects the
// corresponding command.
// Commands are declared just like any other parameter in a CLAP_DECL() paramater
// list declaration. All non-command parameters up to the first command parameter
// are considered to be global tool parameters. Then all non-command parameters
// following the first command paramater declaration are considered to be
// associated with the first command. Then all non-command parameters following
// the second command parameter declaration are associated with the second
// command, etc.
// Here is an example of how a tool can declare a global help and two command
// parameters with command specific options:
//
// CLAP_DECL(params,
//    CLAP_HELP(...),
//    CLAP_REQUIRED_COMMAND("first", ...),
//       CLAP_INT("foo", ...),
//    CLAP_REQUIRED_COMMAND("second", ...),
//       CLAP_VARARG(...)
// )
//
// The user must provide either the first or the second command when they invoke
// the tool.
typedef struct clap_command {
    const char* _Nonnull    name;
    const char* _Nullable   usage;
} clap_command_t;


// A callback function that the command line parser invokes to parse the command
// line argument 'arg' into a value that the callback should store in the storage
// pointed to by clap_param_t->value. The callback should invoke one of the
// clap_error() functions if it detects a syntax or semantic error.
typedef (*clap_value_func_t)(const struct clap_param_t* _Nonnull, unsigned int eo, const char* _Nonnull arg);


enum clap_flag {
    clap_flag_required = 1,                 // The user must provide this parameter in the command line
    clap_flag_appeared = 2,                 // This parameter appeared in the command line
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
        clap_value_func_t _Nonnull          value_func;
        clap_command_t                      cmd;
        const char*                         text;
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


// Defines an optional/required string array (option) parameter. '__saptr' is
// expected to point to a clap_string_array_t variable. Note that the array
// backing store is owned by libclap and is guaranteed to exist through the
// lifetime of the process. You should not attempt to modify the array. Copy it
// if you want to add/remove/replace array entries.
#define CLAP_STRING_ARRAY(__short_label, __long_label, __saptr, __help) \
{clap_type_string_array, 0, __short_label, __long_label, __help, (void*)__saptr}

#define CLAP_REQUIRED_STRING_ARRAY(__short_label, __long_label, __saptr, __help) \
{clap_type_string_array, clap_flag_required, __short_label, __long_label, __help, (void*)__saptr}


// Defines an optional/required (string) enumeration) (option) parameter. '__iptr'
// is expected to point to an int variable and it will be set to the index of the
// enumeration names array entry that matches what the user provided on the
// command line. 
#define CLAP_ENUM(__short_label, __long_label,  __iptr, __strs, __help) \
{clap_type_enum, 0, __short_label, __long_label, __help, (void*)__iptr, {.enum_strings = __strs}}

#define CLAP_REQUIRED_ENUM(__short_label, __long_label, __iptr, __strs, __help) \
{clap_type_enum, clap_flag_required, __short_label, __long_label, __help, (void*)__iptr, {.enum_strings = __strs}}


// Defines an optional/required command (option) parameter. '__name_ptr' is
// expected to point to a variable that stores a pointer to a string. This 
// variable will be set to point to the name of the command that appeared on the
// command line.
// All parameter declarations until the next command or end parameter declaration
// are considered to be associated with this command and will only be interpreted
// if the user selects this command. 
// Note that a command is always a positional parameter.
#define CLAP_COMMAND(__name, __name_ptr, __usage, __help) \
{clap_type_command, 0, '\0', "", __help, (void*)__name_ptr, {.cmd = {__name, __usage}}}

#define CLAP_REQUIRED_COMMAND(__name, __name_ptr, __usage, __help) \
{clap_type_command, clap_flag_required, '\0', "", __help, (void*)__name_ptr, {.cmd = {__name, __usage}}}


// Defines an optional/required value (option) parameter. '__vptr' is expected
// to point to a variable that will hold the value. '__func' is the function
// that will be used to parse an argument string and to update the value variable.
#define CLAP_VALUE(__short_label, __long_label, __vptr, __func, __help) \
{clap_type_value, 0, __short_label, __long_label, __help, (void*)__vptr, {.value_func = __func }}

#define CLAP_REQUIRED_VALUE(__short_label, __long_label, __vptr, __func, __help) \
{clap_type_value, clap_flag_required, __short_label, __long_label, __help, (void*)__vptr, {.value_func = __func }}


// Defines a variable argument list. This is the list of positional parameters
// at the end of the command line. It starts either with the first parameter that
// appears in a position where the parser would expect a short or long label and
// that is not a label (doesn't start with '-' or '--') or it is the first
// parameter following a '--' that is surrounded by whitespace. This list always
// extends to the very end of the command line. Parameters are appear in the
// string array in the same order in which they appear on the command line.
#define CLAP_VARARG(__saptr) \
{clap_type_vararg, 0, '\0', "", "", (void*)__saptr}

#define CLAP_REQUIRED_VARARG(__saptr, __help) \
{clap_type_vararg, clap_flag_required, '\0', "", __help, (void*)__saptr}


// Enables the user to print the version information for the tool by passing
// -v or --version.
#define CLAP_VERSION(__text) \
{clap_type_version, 0, 'v', "version", "Print version", "", {.text = __text}}


// Enables the user to print a help page by passing -h or --help
#define CLAP_HELP() \
{clap_type_help, 0, 'h', "help", "Print help", ""}

// A usage string in the help page. Usage strings are printed in the order in
// which they appear in the parameter list. All usage strings up to the first
// command declaration apply to the help page.
#define CLAP_USAGE(__text) \
{clap_type_usage, 0, '\0', "", "", "", {.text = __text}}

// A prolog paragraph in the help page. Prolog strings are printed in the order
// in which they appear in the parameter list. All prolog strings up to the first
// command declaration apply to the help page.
#define CLAP_PROLOG(__text) \
{clap_type_prolog, 0, '\0', "", "", "", {.text = __text}}

// __title: the help section title
#define CLAP_SECTION(__title) \
{clap_type_section, 0, '\0', "", "", "", {.text = __title}}

// An epilog paragraph in the help page. Epilog strings are printed in the order
// in which they appear in the parameter list. All epilog strings up to the first
// command declaration apply to the help page.
#define CLAP_EPILOG(__text) \
{clap_type_epilog, 0, '\0', "", "", "", {.text = __text}}


// Marks the end of a parameter list. This entry is automatically added by the
// CLAP_DECL() macro.
#define CLAP_END() \
{clap_type_end, 0, '\0', "", "", ""}



// Parses the provided command line arguments based on the syntax rules defined
// by the 'params' parameter list. Prints an appropriate error and terminates the
// process with a failure exit code if a syntax or semantic error is detected.
extern void clap_parse(clap_param_t* _Nonnull params, int argc, const char** argv);


// Call this function to print an error and to terminate the process. The error
// message is formatted like this:
// proc_name: format
extern void clap_error(const char* format, ...);


enum {
    clap_eo_long_label = 1
};

// Similar to clap_error but prints an error message of the form:
// proc_name: param_name: format
extern void clap_param_error(const struct clap_param_t* _Nonnull param, unsigned int eo, const char* format, ...);

__CPP_END

#endif /* _CLAP_H */
