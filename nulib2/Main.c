/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Main entry point and shell command argument processing.
 */
#include "Nulib2.h"
#include <ctype.h>


/*
 * Globals and constants.
 */
const char* gProgName = "Nulib2";


/*
 * Which modifiers are valid with which commands?
 */
typedef struct ValidCombo {
    Command     cmd;
    Boolean     okayForPipe;
    Boolean     filespecRequired;
    const char* modifiers;
} ValidCombo;

static const ValidCombo gValidCombos[] = {
    { kCommandAdd,              false,  true,   "ufrj0cke" },
    { kCommandDelete,           false,  true,   "r" },
    { kCommandExtract,          true,   false,  "ufrjclse" },
    { kCommandExtractToPipe,    true,   false,  "rl" },
    { kCommandListShort,        true,   false,  "" },
    { kCommandListVerbose,      true,   false,  "" },
    { kCommandListDebug,        true,   false,  "" },
    { kCommandTest,             true,   false,  "r" },
};


/*
 * Find an entry in the gValidCombos table matching the specified command.
 *
 * Returns nil if not found.
 */
static const ValidCombo*
FindValidComboEntry(Command cmd)
{
    int i;

    for (i = 0; i < NELEM(gValidCombos); i++) {
        if (gValidCombos[i].cmd == cmd)
            return &gValidCombos[i];
    }

    return nil;
}

/*
 * Determine whether the specified modifier is valid when used with the
 * current command.
 */
static Boolean
IsValidModifier(Command cmd, char modifier)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        if (strchr(pvc->modifiers, modifier) == nil)
            return false;
        else
            return true;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean
IsValidOnPipe(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        return pvc->okayForPipe;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean
IsFilespecRequired(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        return pvc->filespecRequired;
    } else {
        /* command not found?  warn about it here... */
        fprintf(stderr, "%s: Command %d not found in gValidCombos table\n",
            gProgName, cmd);
        return false;
    }
}


/*
 * Separate the program name out of argv[0].
 */
static const char*
GetProgName(const NulibState* pState, const char* argv0)
{
    const char* result;
    char sep;

    /* use the appropriate system pathname separator */
    sep = NState_GetSystemPathSeparator(pState);

    result = strrchr(argv0, sep);
    if (result == nil)
        result = argv0;
    else
        result++;   /* advance past the separator */
    
    return result;
}


/*
 * Print program usage.
 */
static void
Usage(const NulibState* pState)
{
    long majorVersion, minorVersion, bugVersion;
    const char* nufxLibDate;
    const char* nufxLibFlags;

    (void) NuGetVersion(&majorVersion, &minorVersion, &bugVersion,
            &nufxLibDate, &nufxLibFlags);

    printf("\nNulib2 v%s, linked with NufxLib v%ld.%ld.%ld [%s]\n",
        NState_GetProgramVersion(pState),
        majorVersion, minorVersion, bugVersion, nufxLibFlags);
    printf("This software is distributed under terms of the GNU General Public License.\n");
    printf("Written by Andy McFadden, http://www.nulib.com/.\n\n");
    printf("Usage: %s -command[modifiers] archive [filename-list]\n\n",
        gProgName);
    printf(
        "  -a  add files, create arc if needed   -x  extract files\n"
        "  -t  list files (short)                -v  list files (verbose)\n"
        "  -p  extract files to pipe, no msgs    -i  test archive integrity\n"
        "  -d  delete files from archive\n"
        "\n"
        " modifiers:\n"
        "  -u  update files (add + keep newest)  -f  freshen (update, no add)\n"
        "  -r  recurse into subdirs              -j  junk (don't record) directory names\n"
        "  -0  don't use compression             -c  add one-line comments\n"
        "  -l  auto-convert text files           -ll auto-convert ALL files\n"
        "  -s  stomp existing files w/o asking   -k  store files as disk images\n"
        "  -e  preserve ProDOS file types        -ee extend preserved names\n"
        );
}


/*
 * Process the command-line options.  The results are placed into "pState".
 */
static int
ProcessOptions(NulibState* pState, int argc, char* const* argv)
{
    const char* cp;
    int idx;

    /*
     * Must have at least a command letter and an archive filename.
     */
    if (argc < 3) {
        Usage(pState);
        return -1;
    }

    /*
     * Argv[1] and any subsequent entries that have a leading hyphen
     * are options.  Anything after that is a filename.  Parse until we
     * think we've hit the filename.
     *
     * By UNIX convention, however, stdin is specified as a file called "-".
     */
    for (idx = 1; idx < argc; idx++) {
        cp = argv[idx];

        if (idx > 1 && *cp != '-')
            break;

        if (*cp == '-')
            cp++;
        if (*cp == '\0') {
            if (idx == 1) {
                fprintf(stderr,
                    "%s: You must specify a command after the '-'\n",
                    gProgName);
                goto fail;
            } else {
                /* they're using '-' for the filename */
                break;
            }
        }

        if (idx == 1) {
            switch (tolower(*cp)) {
            case 'a': NState_SetCommand(pState, kCommandAdd);           break;
            case 'x': NState_SetCommand(pState, kCommandExtract);       break;
            case 'p': NState_SetCommand(pState, kCommandExtractToPipe); break;
            case 't': NState_SetCommand(pState, kCommandListShort);     break;
            case 'v': NState_SetCommand(pState, kCommandListVerbose);   break;
            case 'z': NState_SetCommand(pState, kCommandListDebug);     break;
            case 'i': NState_SetCommand(pState, kCommandTest);          break;
            case 'd': NState_SetCommand(pState, kCommandDelete);        break;
            default:
                fprintf(stderr, "%s: Unknown command '%c'\n", gProgName, *cp);
                goto fail;
            }

            cp++;
        }

        while (*cp != '\0') {
            switch (tolower(*cp)) {
            case 'u': NState_SetModUpdate(pState, true);                break;
            case 'f': NState_SetModFreshen(pState, true);               break;
            case 'r': NState_SetModRecurse(pState, true);               break;
            case 'j': NState_SetModJunkPaths(pState, true);             break;
            case '0': NState_SetModNoCompression(pState, true);         break;
            case 'c': NState_SetModComments(pState, true);              break;
            case 's': NState_SetModOverwriteExisting(pState, true);     break;
            case 'k': NState_SetModAddAsDisk(pState, true);             break;
            case 'e':
                if (*(cp-1) == 'e')     /* should never point at invalid */
                    NState_SetModPreserveTypeExtended(pState, true);
                else
                    NState_SetModPreserveType(pState, true);
                break;
            case 'l':
                if (*(cp-1) == 'l')     /* should never point at invalid */
                    NState_SetModConvertAll(pState, true);
                else
                    NState_SetModConvertText(pState, true);
                break;
            default:
                fprintf(stderr, "%s: Unknown modifier '%c'\n", gProgName, *cp);
                goto fail;
            }

            if (!IsValidModifier(NState_GetCommand(pState), (char)tolower(*cp)))
            {
                fprintf(stderr,
                    "%s: The '%c' modifier doesn't make sense here\n",
                    gProgName, tolower(*cp));
                goto fail;
            }

            cp++;
        }
    }

    /*
     * See if we have an archive name.  If it's "-", see if we allow that.
     */
    assert(idx < argc);
    NState_SetArchiveFilename(pState, argv[idx]);
    if (IsFilenameStdin(argv[idx])) {
        if (!IsValidOnPipe(NState_GetCommand(pState))) {
            fprintf(stderr, "%s: You can't do that with a pipe\n",
                gProgName);
            goto fail;
        }
    }
    idx++;

    /*
     * See if we have a file specification.  Some of the commands require
     * a filespec; others just perform the requested operation on all of
     * the records in the archive if none is provided.
     */
    if (idx < argc) {
        /* got one or more */
        NState_SetFilespecPointer(pState, &argv[idx]);
        NState_SetFilespecCount(pState, argc - idx);
    } else {
        assert(idx == argc);
        if (IsFilespecRequired(NState_GetCommand(pState))) {
            fprintf(stderr, "%s: This command requires a list of files\n",
                gProgName);
            goto fail;
        }
        NState_SetFilespecPointer(pState, nil);
        NState_SetFilespecCount(pState, 0);
    }


#ifdef DEBUG_VERBOSE
    NState_DebugDump(pState);
#endif

    return 0;

fail:
    fprintf(stderr,
        "%s: (invoke without arguments to see usage information)\n",
        gProgName);
    return -1;
}


/*
 * We have all of the parsed command line options in "pState".  Now we just
 * have to do something useful with it.
 *
 * Returns 0 on success, 1 on error.
 */
int
DoWork(NulibState* pState)
{
    NuError err;

    switch (NState_GetCommand(pState)) {
    case kCommandAdd:
        err = DoAdd(pState);
        break;
    case kCommandExtract:
        err = DoExtract(pState);
        break;
    case kCommandExtractToPipe:
        err = DoExtractToPipe(pState);
        break;
    case kCommandTest:
        err = DoTest(pState);
        break;
    case kCommandListShort:
        err = DoListShort(pState);
        break;
    case kCommandListVerbose:
        err = DoListVerbose(pState);
        break;
    case kCommandListDebug:
        err = DoListDebug(pState);
        break;
    case kCommandDelete:
        err = DoDelete(pState);
        break;
    default:
        fprintf(stderr, "ERROR: unexpected command %d\n",
            NState_GetCommand(pState));
        err = kNuErrInternal;
        assert(0);
        break;
    }

    return (err != kNuErrNone);
}

/*
 * Entry point.
 */
int
main(int argc, char** argv)
{
    NulibState* pState = nil;
    int result = 0;

    #if 0
    extern NuResult ErrorMessageHandler(NuArchive* pArchive,
        void* vErrorMessage);
    NuSetGlobalErrorMessageHandler(ErrorMessageHandler);
    #endif

    if (NState_Init(&pState) != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to initialize globals\n");
        exit(1);
    }

    gProgName = GetProgName(pState, argv[0]);

    if (ProcessOptions(pState, argc, argv) < 0) {
        result = 2;
        goto bail;
    }

    if (NState_ExtraInit(pState) != kNuErrNone) {
        fprintf(stderr, "ERROR: additional initialization failed\n");
        exit(1);
    }

    result = DoWork(pState);
    if (result)
        printf("Failed.\n");

bail:
    NState_Free(pState);
    exit(result);
}

