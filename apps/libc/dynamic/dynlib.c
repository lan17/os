/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dynlib.c

Abstract:

    This module implements support for loading dynamic libraries at runtime.

Author:

    Evan Green 17-Oct-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <dlfcn.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the last status of a dynamic library open operation.
//

KSTATUS ClDynamicLibraryStatus = STATUS_SUCCESS;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void *
dlopen (
    const char *Library,
    int Flags
    )

/*++

Routine Description:

    This routine opens and loads a dynamic library object with the given name.
    Only one instance of a given binary will be loaded per process.

Arguments:

    Library - Supplies a pointer to the null terminated string of the library
        to open. Supply NULL to open a handle to a global symbol table.

    Flags - Supplies a bitfield of flags governing the behavior of the library.
        See RTLD_* definitions.

Return Value:

    Returns an opaque handle to the library that can be used in calls to dlsym.

    NULL on failure, and more information can be retrieved via the dlerror
    function.

--*/

{

    HANDLE Handle;
    KSTATUS Status;

    Status = OsLoadLibrary((PSTR)Library, 0, &Handle);
    if (!KSUCCESS(Status)) {
        ClDynamicLibraryStatus = Status;
        return NULL;
    }

    assert(Handle != INVALID_HANDLE);

    return Handle;
}

LIBC_API
int
dlclose (
    void *Handle
    )

/*++

Routine Description:

    This routine closes a previously opened dynamic library. This may or may
    not result in the library being unloaded, depending on what else has
    references out on it. Either way, callers should assume the handle is
    not valid for any future calls to the dlsym function.

Arguments:

    Handle - Supplies the opaque handle returned when the library was opened.

Return Value:

    0 on success.

    Non-zero on failure, and dlerror will be set to contain more information.

--*/

{

    if (Handle == NULL) {
        ClDynamicLibraryStatus = STATUS_INVALID_HANDLE;
        return -1;
    }

    OsFreeLibrary(Handle);
    return 0;
}

LIBC_API
char *
dlerror (
    void
    )

/*++

Routine Description:

    This routine returns a null terminated string (with no trailing newline)
    that describes the last error that occurred during dynamic linking
    processing. If no errors have occurred since the last invocation, NULL is
    returned. Invoking this routine a second time immediately following a prior
    invocation will return NULL. This routine is neither thread-safe nor
    reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to a string describing the last error on success. The
    caller is not responsible for freeing this memory.

    NULL if nothing went wrong since the last invocation.

--*/

{

    INT ErrorNumber;
    KSTATUS Status;

    Status = ClDynamicLibraryStatus;
    ClDynamicLibraryStatus = STATUS_SUCCESS;
    if (KSUCCESS(Status)) {
        return NULL;
    }

    ErrorNumber = ClConvertKstatusToErrorNumber(Status);
    return strerror(ErrorNumber);
}

LIBC_API
void *
dlsym (
    void *Handle,
    const char *SymbolName
    )

/*++

Routine Description:

    This routine returns the address of a symbol defined within an object made
    accessible through a call to dlopen. This routine searches both this object
    and any objects loaded as a result of this one.

Arguments:

    Handle - Supplies a pointer to the opaque handle returned by the dlopen
        routine.

    SymbolName - Supplies a pointer to a null-terminated string containing the
        name of the symbol whose address should be retrieved.

Return Value:

    Returns the address of the symbol on success.

    NULL if the handle was not valid or the symbol could not be found. More
    information can be retrieved via the dlerror function.

--*/

{

    PVOID Address;
    KSTATUS Status;

    //
    // The C Library handle definitions better line up with the OS base's.
    //

    ASSERT((RTLD_DEFAULT == OS_LIBRARY_DEFAULT) &&
           (RTLD_NEXT == OS_LIBRARY_NEXT));

    Status = OsGetLibrarySymbolAddress(Handle, (PSTR)SymbolName, &Address);
    if (!KSUCCESS(Status)) {
        ClDynamicLibraryStatus = Status;
        return NULL;
    }

    return Address;
}

LIBC_API
int
dladdr (
    void *Address,
    Dl_info *Information
    )

/*++

Routine Description:

    This routine resolves an address into the symbol and dynamic library
    information.

Arguments:

    Address - Supplies the address being resolved.

    Information - Supplies a pointer that recevies that dynamic library
        information for the given address.

Return Value:

    Non-zero on success.

    0 on failure, but dlerror will not be set to contain more information.

--*/

{

    KSTATUS Status;
    OS_LIBRARY_SYMBOL Symbol;

    Status = OsGetLibrarySymbolForAddress(OS_LIBRARY_DEFAULT, Address, &Symbol);
    if (!KSUCCESS(Status)) {
        return 0;
    }

    Information->dli_fname = Symbol.LibraryName;
    Information->dli_fbase = Symbol.LibraryBaseAddress;
    Information->dli_sname = Symbol.SymbolName;
    Information->dli_saddr = Symbol.SymbolAddress;
    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

