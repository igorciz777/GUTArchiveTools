#include "gut_archive.h"

void usage(const char *progname)
{
    printf("\nUsage: %s [mode] [comp] [log]\n\n", progname);
    printf("  Modes:\n");
    printf("    -r  <BUILD.TOC> <BUILD.DAT> <IN_DIR>: \n\trebuild files in <IN_DIR> into <BUILD.DAT>\n\n");
    printf("    -d  <BUILD.TOC> <BUILD.DAT> <OUT_DIR>: \n\tdecompress and output the archive to <OUT_DIR>\n\n");
    printf("    -cd <FILE.DAT>  <OUT_DIR>: \n\textract files from a .dat container\n\n");
    printf("    -cb <IN_DIR> <OUT_FILE>: \n\tbuild files into a new .dat container\n\n");
    printf("  Compatibility switches (only use if stated):\n");
    printf("    -0: TXR:D2, KR2, KB3, Wangan Midnight Portable, Ninkyouden\n");
    printf("    -2: Import Tuner Challenge, Shutokou Battle X\n");
    printf("    -3: Kaidou Battle 1 Taikenban\n");
    printf("    -4: Kaidou Battle 2 PurePure 2 Volume 10 Demo\n\n");
    printf("  Logs:\n");
    printf("    -log: Creates a log file for the operation\n\n");
    printf("\n");
}

static void parse_optional_args(int argc, char *argv[], int start_idx)
{
    for (int i = start_idx; i < argc; i++)
    {
        if (strcmp(argv[i], "-log") == 0)
        {
            _LOGS = true;
        }
        else if (argv[i][0] == '-' && argv[i][1] >= '0' && argv[i][1] <= '9' && argv[i][2] == '\0')
        {
            set_game_id(argv[i]);
        }
    }
}

int main(int argc, char *argv[])
{
    int result = 1;

    if (argc < 2)
    {
        printf("Error: Insufficient arguments\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (ucl_init() != UCL_E_OK)
    {
        printf("Error: ucl_init() failed\n");
        return EXIT_FAILURE;
    }

    /**
     * Two argument modes:
     */
    if (strcmp(argv[1], "-cd") == 0)
    {
        if (argc < 4)
        {
            printf("Error: Insufficient arguments for -cd\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        parse_optional_args(argc, argv, 4);

        FILE *datafile = fopen(argv[2], "rb");
        if (datafile == NULL)
        {
            perror("Failed to open .dat file");
            return EXIT_FAILURE;
        }

        result = extract_datafile(datafile, argv[3]);
        fclose(datafile);
    }
    else if (strcmp(argv[1], "-cb") == 0)
    {
        if (argc < 4)
        {
            printf("Error: Insufficient arguments for -cb\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        parse_optional_args(argc, argv, 4);

        result = build_datafile(argv[2], argv[3]);
    }
    /**
     * Three argument modes:
     */
    else if (strcmp(argv[1], "-r") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments for -r\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        parse_optional_args(argc, argv, 5);

        result = rebuild_GUTArchive(argv[2], argv[3], argv[4]);
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        if (argc < 5)
        {
            printf("Error: Insufficient arguments for -d\n");
            usage(argv[0]);
            return EXIT_FAILURE;
        }

        parse_optional_args(argc, argv, 5);

        FILE *toc_file = fopen(argv[2], "rb");
        if (toc_file == NULL)
        {
            perror("Failed to open .toc file");
            return EXIT_FAILURE;
        }
        FILE *dat_file = fopen(argv[3], "rb");
        if (dat_file == NULL)
        {
            perror("Failed to open .dat file");
            fclose(toc_file);
            return EXIT_FAILURE;
        }

        result = extract_GUTArchive_all(toc_file, dat_file, argv[4]);
        fclose(toc_file);
        fclose(dat_file);
    }
    else
    {
        printf("Error: Unknown mode '%s'\n", argv[1]);
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (result != 0)
    {
        printf("Error: Operation failed\n");
        return EXIT_FAILURE;
    }

    close_log();

    return EXIT_SUCCESS;
}