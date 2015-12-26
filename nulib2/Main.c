/*
 * NuLib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 *
 * Main entry point and shell command argument processing.
 */
#include "NuLib2.h"
#include <ctype.h>


/*
 * Globals and constants.
 */
const char* gProgName = "NuLib2";


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
    { kCommandAdd,              false,  true,   "ekcz0jrfu" },
    { kCommandDelete,           false,  true,   "r" },
    { kCommandExtract,          true,   false,  "beslcjrfu" },
    { kCommandExtractToPipe,    true,   false,  "blr" },
    { kCommandListShort,        true,   false,  "br" },
    { kCommandListVerbose,      true,   false,  "br" },
    { kCommandListDebug,        true,   false,  "b" },
    { kCommandTest,             true,   false,  "br" },
    { kCommandHelp,             false,  false,  "" },
};


/*
 * Find an entry in the gValidCombos table matching the specified command.
 *
 * Returns NULL if not found.
 */
static const ValidCombo* FindValidComboEntry(Command cmd)
{
    int i;

    for (i = 0; i < NELEM(gValidCombos); i++) {
        if (gValidCombos[i].cmd == cmd)
            return &gValidCombos[i];
    }

    return NULL;
}

/*
 * Determine whether the specified modifier is valid when used with the
 * current command.
 */
static Boolean IsValidModifier(Command cmd, char modifier)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != NULL) {
        if (strchr(pvc->modifiers, modifier) == NULL)
            return false;
        else
            return true;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean IsValidOnPipe(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != NULL) {
        return pvc->okayForPipe;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean IsFilespecRequired(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != NULL) {
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
static const char* GetProgName(const NulibState* pState, const char* argv0)
{
    const char* result;
    char sep;

    /* use the appropriate system pathname separator */
    sep = NState_GetSystemPathSeparator(pState);

    result = strrchr(argv0, sep);
    if (result == NULL)
        result = argv0;
    else
        result++;   /* advance past the separator */
    
    return result;
}


/*
 * Print program usage.
 */
static void Usage(const NulibState* pState)
{
    int32_t majorVersion, minorVersion, bugVersion;
    const char* nufxLibDate;
    const char* nufxLibFlags;

    (void) NuGetVersion(&majorVersion, &minorVersion, &bugVersion,
            &nufxLibDate, &nufxLibFlags);

    printf("\nNuLib2 v%s, linked with NufxLib v%d.%d.%d [%s]\n",
        NState_GetProgramVersion(pState),
        majorVersion, minorVersion, bugVersion, nufxLibFlags);
    printf("Copyright (C) 2000-2015, Andy McFadden.  All Rights Reserved.\n");
    printf("This software is distributed under terms of the BSD License.\n");
    printf("Visit http://www.nulib.com/ for source code and documentation.\n\n");
    printf("Usage: %s -command[modifiers] archive [filename-list]\n\n",
        gProgName);
    printf(
        "  -a  add files, create arc if needed   -x  extract files\n"
        "  -t  list files (short)                -v  list files (verbose)\n"
        "  -p  extract files to pipe, no msgs    -i  test archive integrity\n"
        "  -d  delete files from archive         -h  extended help message\n"
        "\n"
        " modifiers:\n"
        "  -u  update files (add + keep newest)  -f  freshen (update, no add)\n"
        "  -r  recurse into subdirs              -j  junk (don't record) directory names\n"
        "  -0  don't use compression             -c  add one-line comments\n");
    if (NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone)
        printf("  -z  use zlib 'deflate' compression    ");
    else
        printf("  -z  use zlib [not included]           ");
    if (NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone)
        printf("-zz use bzip2 BWT compression\n");
    else
        printf("-zz use bzip2 [not included]\n");
    printf(
        "  -l  auto-convert text files           -ll convert CR/LF on ALL files\n"
        "  -s  stomp existing files w/o asking   -k  store files as disk images\n"
        "  -e  preserve ProDOS file types        -ee preserve types and extend names\n"
        "  -b  force Binary II mode\n"
        );
}


/*
 * Handle the "-h" command.
 */
NuError DoHelp(const NulibState* pState)
{
    static const struct {
        Command cmd;
        char letter;
        const char* shortDescr;
        const char* longDescr;
    } help[] = {
        { kCommandListVerbose, 'v', "verbose listing of archive contents",
"  List archive contents in a multi-column format with summary totals.\n"
"  The output is similar to what ShrinkIt produces.  You can specify the\n"
"  names of the files to list.\n"
"\n"
"  The '-b' modifier will force NuLib2 to open the file as Binary II.\n"
"  This can be useful when working with encapsulated archives (.BXY and\n"
"  .BSE) and when the archive is presented on standard input (i.e.\n"
"  \"nulib2 -vb - < archive.bny\").\n",
        },
        { kCommandListShort, 't', "quick dump of table of contents",
"  List the filenames, one per line, without any truncation or embellishment.\n"
"  Useful for feeding into another program.\n",
        },
        { kCommandAdd, 'a', "add files to an archive",
"  Add files to an archive, possibly creating it first.  Files in\n"
"  subdirectories will not be added unless the '-r' flag is provided.\n"
"\n"
"  You can specify the '-z' or '-zz' flag to use \"deflate\" or \"bzip2\"\n"
"  compression, respectively.  These work much better than ShrinkIt's LZW,\n"
"  but the files can't be unpacked on an Apple II.  The flags will only be\n"
"  enabled if NuLib2 was built with the necessary libraries.\n",
        },
        { kCommandExtract, 'x', "extract files from an archive",
"  Extract the specified items from the archive.  If nothing is specified,\n"
"  everything is extracted.  The '-r' modifier tells NuLib2 to do a prefix\n"
"  match, so \"nulib2 -xr archive.shk doc\" will extract everything in the\n"
"  \"doc\" directory.  It will also extract a file called \"document\".  If\n"
"  you just want what's in a directory, tack on the filename separator\n"
"  character, e.g. \"nulib2 -xr archive.shk doc:\".\n"
"\n"
"  When working with Binary II archives, the following suboptions aren't\n"
"  allowed: -u -f -c -l -ll.\n",
        },
        { kCommandExtractToPipe, 'p', "extract files to pipe",
"  Works just like '-x', but all files are written to stdout.  Useful for\n"
"  viewing a file, e.g. \"nulib2 -pl archive.shk document.txt | less\".\n"
"  The progress indicators and interactive prompts are disabled.\n",
        },
        { kCommandTest, 'i', "test archive integrity",
"  Verify the contents of an archive by extracting all files to memory and\n"
"  verifying all CRCs.  Note that uncompressed files in archives created by\n"
"  P8 ShrinkIt and un-SQueezed files in Binary II archives do not have any\n"
"  sort of checksum.\n",
        },
        { kCommandDelete, 'd', "delete files from archive",
"  Delete the named files from the archive.  If you delete all of the files,\n"
"  the archive itself will be removed.\n",
        },
        { kCommandHelp, 'h', "show extended help",
"  This is the extended help text.  Go to www.nulib.com for a full manual.\n",
        },
    };

    int i;


    printf("%s",
"\n"
"Copyright (C) 2000-2007 by Andy McFadden.  All Rights Reserved.\n\n"
"NuLib2 uses NufxLib, a complete library of functions for accessing NuFX\n"
"(ShrinkIt) archives.  Both NufxLib and NuLib2 are free software, distributed\n"
"under terms of the BSD License.  Source code is available from\n"
"http://www.nulib.com/, and copies of the licenses are included.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"README file for more details.\n\n"
    );

    printf("Usage: %s -command[modifiers] archive [filename-list]\n\n",
        gProgName);
    printf("If \"archive\" is \"-\", the archive will be read from stdin.\n");

    for (i = 0; i < NELEM(help); i++) {
        const ValidCombo* pvc;
        int j;

        pvc = FindValidComboEntry(help[i].cmd);
        if (pvc == NULL) {
            fprintf(stderr, "%s: internal error: couldn't find vc for %d\n",
                gProgName, help[i].cmd);
            continue;
        }

        printf("\nCommand \"-%c\": %s\n", help[i].letter, help[i].shortDescr);
        printf("  Valid modifiers:");
        for (j = strlen(pvc->modifiers) -1; j >= 0; j--) {
            char ch = pvc->modifiers[j];
            /* print flags, special-casing options that can be doubled */
            if (ch == 'l' || ch == 'e' || ch == 'z')
                printf(" -%c -%c%c", ch, ch, ch);
            else
                printf(" -%c", ch);
        }
        putchar('\n');

        printf("\n%s", help[i].longDescr);
    }
    putchar('\n');

    printf("Compression algorithms supported:\n");
    printf("  SQueeze (Huffman+RLE) ...... %s\n",
        NuTestFeature(kNuFeatureCompressSQ) == kNuErrNone? "yes" : "no");
    printf("  LZW/1 and LZW/2 ............ %s\n",
        NuTestFeature(kNuFeatureCompressLZW) == kNuErrNone ? "yes" : "no");
    printf("  12- and 16-bit LZC ......... %s\n",
        NuTestFeature(kNuFeatureCompressLZC) == kNuErrNone ? "yes" : "no");
    printf("  Deflate .................... %s\n",
        NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone ? "yes" : "no");
    printf("  Bzip2 ...................... %s\n",
        NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone ? "yes" : "no");


    return kNuErrNone;
}


/*
 * Process the command-line options.  The results are placed into "pState".
 */
static int ProcessOptions(NulibState* pState, int argc, char* const* argv)
{
    const char* cp;
    int idx;

    /*
     * Must have at least a command letter and an archive filename, unless
     * the command letter is 'h'.  Special-case a solitary "-h" here.
     */
    if (argc == 2 && (tolower(argv[1][0]) == 'h' ||
                     (argv[1][0] == '-' && tolower(argv[1][1] == 'h')) ) )
    {
        DoHelp(NULL);
        return -1;
    }

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
            case 'g': NState_SetCommand(pState, kCommandListDebug);     break;
            case 'i': NState_SetCommand(pState, kCommandTest);          break;
            case 'd': NState_SetCommand(pState, kCommandDelete);        break;
            case 'h': NState_SetCommand(pState, kCommandHelp);          break;
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
            case 's': NState_SetModOverwriteExisting(pState, true);     break;
            case 'k': NState_SetModAddAsDisk(pState, true);             break;
            case 'c': NState_SetModComments(pState, true);              break;
            case 'b': NState_SetModBinaryII(pState, true);              break;
            case 'z':
                if (*(cp+1) == 'z') {
                    if (NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone)
                        NState_SetModCompressBzip2(pState, true);
                    else {
                        fprintf(stderr,
                            "%s: ERROR: libbz2 support not compiled in\n",
                            gProgName);
                        goto fail;
                    }
                    cp++;
                } else {
                    if (NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone)
                        NState_SetModCompressDeflate(pState, true);
                    else {
                        fprintf(stderr,
                            "%s: ERROR: zlib support not compiled in\n",
                            gProgName);
                        goto fail;
                    }
                }
                break;
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
     * Can't have tea and no tea at the same time.
     */
    if (NState_GetModNoCompression(pState) &&
        NState_GetModCompressDeflate(pState))
    {
        fprintf(stderr, "%s: Can't specify both -0 and -z\n",
            gProgName);
        goto fail;
    }

    /*
     * See if we have an archive name.  If it's "-", see if we allow that.
     */
    Assert(idx < argc);
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
        Assert(idx == argc);
        if (IsFilespecRequired(NState_GetCommand(pState))) {
            fprintf(stderr, "%s: This command requires a list of files\n",
                gProgName);
            goto fail;
        }
        NState_SetFilespecPointer(pState, NULL);
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
int DoWork(NulibState* pState)
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
    case kCommandHelp:
        err = DoHelp(pState);
        break;
    default:
        fprintf(stderr, "ERROR: unexpected command %d\n",
            NState_GetCommand(pState));
        err = kNuErrInternal;
        Assert(0);
        break;
    }

    return (err != kNuErrNone);
}

/*
 * Entry point.
 */
int main(int argc, char** argv)
{
    NulibState* pState = NULL;
    int32_t majorVersion, minorVersion, bugVersion;
    int result = 0;

    (void) NuGetVersion(&majorVersion, &minorVersion, &bugVersion, NULL, NULL);
    if (majorVersion != kNuVersionMajor || minorVersion < kNuVersionMinor) {
        fprintf(stderr, "ERROR: wrong version of NufxLib --"
                        " wanted %d.%d.x, got %d.%d.%d.\n",
            kNuVersionMajor, kNuVersionMinor,
            majorVersion, minorVersion, bugVersion);
        goto bail;
    }

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

