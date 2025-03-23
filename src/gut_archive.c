#include "gut_archive.h"


void usage(const char *progname)
{
    printf("\nUsage: %s [mode] [comp] [log]\n\n", progname);
    printf("  Modes:\n");
    printf("    -r  <BUILD.TOC> <BUILD.DAT> <IN_DIR>: \n\trebuild files in <IN_DIR> into <BUILD.DAT>\n\n");
    printf("    -d  <BUILD.TOC> <BUILD.DAT> <OUT_DIR>: \n\tdecompress and output the archive to <OUT_DIR>\n\n");
    printf("    -dr <BUILD.TOC> <BUILD.DAT> <OUT_DIR>: \n\tdecompress and output the archive recursively (any .dat files inside) to <OUT_DIR>\n\n");
    printf("    -rr <BUILD.TOC> <BUILD.DAT> <IN_DIR>: \n\trebuild files in <IN_DIR> recursively (repacks .dat files inside) into <BUILD.DAT>\n\n");
    printf("    -a  <BUILD.TOC> <BUILD.DAT> <FILE>: \n\tadd a new <FILE> to the bottom of the <BUILD.DAT> archive [experimental]\n\n");
    printf("    -cd <FILE.DAT>  <OUT_DIR>: \n\textract files from a .dat container\n\n");
    printf("    -cr <FILE.DAT>  <IN_DIR>: \n\trebuild files into a .dat container\n\n");
    printf("  Compatibility switches (only use if stated):\n");
    printf("    -0: TXR:D2, KR2, KB3, Wangan Midnight Portable, Ninkyouden\n");
    printf("    -2: Import Tuner Challenge, Shutokou Battle X\n");
    printf("    -3: Kaidou Battle 1 Taikenban\n");
    printf("    -4: Kaidou Battle 2 PurePure 2 Volume 10 Demo\n\n");
    printf("  Logs:\n");
    printf("    -log: Creates a log file for the operation\n\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    char toc_file[256], dat_file[256], output_dir[256], directory[256];
    int result;

    if (argc < 2)
    {
        printf("Error: Insufficient arguments\n");
        usage(argv[0]);
        return 1;
    }
    if (ucl_init() != UCL_E_OK)
    {
        printf("Error: ucl_init() failed\n");
        return 1;
    }

    if (strcmp(argv[1], "-r") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }

        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(directory, argv[4], 255);
        if (argc > 5)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        if (REBUILDING_ALLOWED == 0)
        {
            printf("Rebuilding is disabled in this release, still in development\n");
            return 1;
        }
        result = rebuild_GUT_Archive(toc_file, dat_file, directory, false);
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(output_dir, argv[4], 255);
        if (argc > 5)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        result = decompress_GUT_Archive(toc_file, dat_file, output_dir, false);
    }
    else if (strcmp(argv[1], "-cd") == 0)
    {
        if (argc < 4)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        if (argc > 4)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 5)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        strncpy(dat_file, argv[2], 255);
        strncpy(output_dir, argv[3], 255);
        result = extract_dat_container(dat_file, output_dir, true);
    }
    else if (strcmp(argv[1], "-cr") == 0)
    {
        if (REBUILDING_DAT == 0)
        {
            printf("Rebuilding .dat containers is disabled in this release, still in development\n");
            return 1;
        }
        if (argc < 4)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        if (argc > 4)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 5)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        strncpy(dat_file, argv[2], 255);
        strncpy(directory, argv[3], 255);
        result = build_dat_container(dat_file, directory);
    }
    else if (strcmp(argv[1], "-dr") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        if (argc > 5)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(output_dir, argv[4], 255);
        if (RECURSIVE_ALLOWED == 0)
        {
            printf("Recursive extraction is disabled in this release, still in development\n");
            return 1;
        }
        result = decompress_GUT_Archive(toc_file, dat_file, output_dir, true);
    }
    else if (strcmp(argv[1], "-rr") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        if (argc > 5)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(directory, argv[4], 255);
        if (RECURSIVE_ALLOWED == 0 || REBUILDING_DAT == 0)
        {
            printf("Recursive rebuilding is disabled in this release, still in development\n");
            return 1;
        }
        result = rebuild_GUT_Archive(toc_file, dat_file, directory, true);
    }
    else if (strcmp(argv[1], "-a") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments\n");
            usage(argv[0]);
            return 1;
        }
        if (argc > 5)
        {
            char arg[5];
            strncpy(arg, argv[5], 4);
            if (arg[1] != 'l')
                gameid = atoi(arg);
            else if (strcmp(arg, "-log") == 0)
                logs = true;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = true;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(directory, argv[4], 255);
        result = add_file_to_archive(toc_file, dat_file, directory);
    }
    else
    {
        printf("Error: Invalid arguments\n");
        usage(argv[0]);
        return 1;
    }

    if (result != 0)
    {
        printf("Error: Operation failed\n");
        return 1;
    }

    return 0;
}