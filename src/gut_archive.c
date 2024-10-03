#include <ucl/ucl.h>
#include <dirent.h>
#include <stdio.h>
/* portability layer */
#define WANT_UCL_MALLOC 1
#define WANT_UCL_FREAD 1
#define WANT_UCL_WILDARGV 1

#include "include/portab.h"
#include "include/gnk_headers.h"
#include "include/ucl/ucl_func.h"
#include "include/ucl/ucl_endian.h"

/*debug switches*/
#define REBUILDING_ALLOWED 1
#define REBUILDING_DAT 1
#define REBUILDING_DEBUG 0
#define CONTAINER_DEBUG 0
#define RECURSIVE_ALLOWED 1

static char progname[] = "gut_archive";

static int gameid = 1;
static BOOL logs = FALSE;
static ucl_uint opt_fast = 1;

typedef struct
{
    char infilename[262];
    BOOL compressed;
    BOOL importing;
    ucl_uint start_offset;
    ucl_uint end_offset;
    ucl_uint compressed_size;
    ucl_uint decompressed_size;
    ucl_uint zero_field;
    ucl_uint32 block_size;
} build_file;

typedef struct
{
    ucl_uint start_offset;
    ucl_uint end_offset;
} dat_file_in;

void progress_bar(int percentage)
{
    int bar_width = 50;
    int pos = (percentage * bar_width) / 100;

    printf("["); // Start of the bar

    for (int i = 0; i < bar_width; ++i)
    {
        if (i < pos)
        {
            printf("=");
        }
        else
        {
            printf(" ");
        }
    }

    printf("] %d%%\r", percentage);
    fflush(stdout);
}

void read_toc(FILE* toc_file, ucl_uint* start_offset, ucl_uint* compressed_size, ucl_uint* end_offset, ucl_uint* zero_field, ucl_uint* decompressed_size){
    switch (gameid)
        {
        case 0: /*TXR:D2*/
            xread(toc_file, start_offset, 4, 0);
            xread(toc_file, compressed_size, 4, 0);
            xread(toc_file, decompressed_size, 4, 0);
            xread(toc_file, zero_field, 4, 0);
            *end_offset = 1 + *compressed_size / 0x800;
            break;
        case -2: /*ITC Big endian*/
            xread(toc_file, start_offset, 4, 0);
            xread(toc_file, compressed_size, 4, 0);
            *compressed_size = swap_uint32(*compressed_size);
            xread(toc_file, decompressed_size, 4, 0);
            *decompressed_size = swap_uint32(*decompressed_size);
            xread(toc_file, zero_field, 4, 0);
            if (zero_field != 0)
                end_offset = 0;
            else
                *end_offset = swap_uint32(1 + *compressed_size / 0x800);
            break;
        case -3: /*KB1T*/
            xread(toc_file, start_offset, 4, 0);
            xread(toc_file, compressed_size, 4, 0);
            xread(toc_file, end_offset, 4, 0);
            xread(toc_file, zero_field, 4, 0);
            *decompressed_size = 0;
            if(*compressed_size == 0) *zero_field = 1;
            break;
        default:
            xread(toc_file, start_offset, 4, 0);
            xread(toc_file, compressed_size, 4, 0);
            xread(toc_file, end_offset, 4, 0);
            xread(toc_file, zero_field, 4, 0);
            xread(toc_file, decompressed_size, 4, 0);
            break;
        }
}

static BOOL check_for_dat_container(char *file_data, ucl_uint file_size)
{
    BOOL flag = TRUE;
    ucl_uint file_count;
    ucl_uint file_offset;

    memcpy(&file_count, file_data, 4);
#if CONTAINER_DEBUG
    printf("File count: %d\n", file_count);
#endif
    if (file_count > 0x1000 || file_count <= 0x00)
    {
        flag = FALSE;
    }
    for (int i = 0; i < file_count && flag; i++)
    {
        memcpy(&file_offset, file_data + 4 + (i * 4), 4);
#if CONTAINER_DEBUG
        printf("File %d offset: %08x\n", i, file_offset);
#endif
        if (file_offset == 0x00)
        {
            flag = FALSE;
        }
        if (file_offset >= file_size)
        {
            flag = FALSE;
        }
        /*Kaido Racer edge case*/
        if (file_offset == 0xFFFFFFFF)
        {
            flag = FALSE;
        }
        /*Bakumatsuden edge case*/
        if (file_count == 0x0F && file_offset == 0x29 && i < 1)
        {
            flag = FALSE;
        }
    }
    if (flag)
    {
        memcpy(&file_offset, file_data + 4 + (file_count * 4), 4);
#if CONTAINER_DEBUG
        printf("Last offset (Zero check): %08x\n", file_offset);
#endif
        if (file_offset != 0x00)
        {
            flag = FALSE;
        }
    }
#if CONTAINER_DEBUG
    printf("Container check: %s\n", flag ? "PASSED" : "FAILED");
#endif

    return flag;
}

int build_dat_container(const char *dat_filename, const char *input_dir)
{
    FILE *dat_file;
    DIR *dir;
    struct dirent *entry;
    dat_file_in *files;
    ucl_uint file_count = 0;
    ucl_uint dat_size = 0;
    ucl_uint file_start_offset = 0;
    ucl_uint file_end_offset = 0;
    ucl_uint file_length = 0;
    int file_index = 0;

    dat_file = fopen(dat_filename, "r+b");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        return EXIT_FAILURE;
    }

    dir = opendir(input_dir);
    if (dir == NULL)
    {
        perror("Failed to open input directory");
        fclose(dat_file);
        return EXIT_FAILURE;
    }

    xread(dat_file, &file_count, 4, 0);
    files = (dat_file_in *)malloc(file_count * sizeof(dat_file_in));
    if (files == NULL)
    {
        perror("Failed to allocate memory");
        fclose(dat_file);
        closedir(dir);
        return EXIT_FAILURE;
    }

    printf("Rebuilding DAT Container...\n");

    while (TRUE)
    {
        fseek(dat_file, 4, SEEK_SET);
        fseek(dat_file, file_index * 4, SEEK_CUR);
        if (file_index >= file_count)
        {
            break;
        }
        xread(dat_file, &file_start_offset, 4, 0);
        if (file_start_offset == 0)
        {
            break;
        }
        xread(dat_file, &file_end_offset, 4, 0);
        if (file_end_offset == 0)
        {
            file_end_offset = dat_size;
        }

        files[file_index].start_offset = file_start_offset;
        files[file_index].end_offset = file_end_offset;
        file_index++;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry == NULL || entry->d_name[0] == '.')
        {
            continue;
        }

        char full_path[320];
        sprintf(full_path, "%s/%s", input_dir, entry->d_name);

        char no_ext[320];
        strncpy(no_ext, entry->d_name, 320);
        strtok(no_ext, ".");

        file_index = atoi(no_ext);

        FILE *input_file = fopen(full_path, "rb");
        if (input_file == NULL)
        {
            perror("Failed to open input file");
            fclose(dat_file);
            closedir(dir);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(input_file, 0, SEEK_END);
        file_length = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        char *file_data = (char *)malloc(file_length);
        if (file_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(input_file);
            fclose(dat_file);
            closedir(dir);
            free(files);
            return EXIT_FAILURE;
        }
        xread(input_file, file_data, file_length, 0);

        fseek(dat_file, 0, SEEK_SET);
        fseek(dat_file, files[file_index].start_offset, SEEK_CUR);

        xwrite(dat_file, file_data, file_length);

        free(file_data);
        fclose(input_file);
    }

    free(files);
    closedir(dir);
    fclose(dat_file);
    printf("DAT Container built successfully\n");
    return EXIT_SUCCESS;
}

int extract_dat_container(const char *dat_filename, const char *output_dir, BOOL check_correctness)
{
    BOOL recursive_dat = FALSE;
    FILE *dat_file;
    FILE *log;
    ucl_uint file_count;
    ucl_uint file_length;
    ucl_uint file_start_offset;
    ucl_uint file_end_offset;
    ucl_uint dat_size;
    char *dat_data;
    char log_line[256];
    int file_index = 0;

    dat_file = fopen(dat_filename, "rb");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        return EXIT_FAILURE;
    }

    log = fopen("dat_extract.log", "a");

    char full_path[256];
    sprintf(full_path, "%s/%s", output_dir, dat_filename);

#if defined(_WIN32)
    _mkdir(output_dir);
#else
    mkdir(output_dir, 0700);
#endif

    fseek(dat_file, 0, SEEK_END);
    dat_size = ftell(dat_file);
    fseek(dat_file, 0, SEEK_SET);

    if (check_correctness)
    {
        dat_data = (char *)malloc(dat_size);
        if (dat_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(dat_file);
            return EXIT_FAILURE;
        }

        xread(dat_file, dat_data, dat_size, 0);
        fseek(dat_file, 0, SEEK_SET);

        if (!check_for_dat_container(dat_data, dat_size))
        {
            printf("Invalid DAT Container\n");
            fclose(dat_file);
            free(dat_data);
            return EXIT_FAILURE;
        }
    }
#if CONTAINER_DEBUG
    printf("Extracting DAT Container...\n");
#endif
    sprintf(log_line, "Extracting DAT file: %s\n", dat_filename);
    fwrite(log_line, 1, strlen(log_line), log);

    xread(dat_file, &file_count, 4, 0);

    while (TRUE)
    {
        recursive_dat = FALSE;
        fseek(dat_file, 4, SEEK_SET);
        fseek(dat_file, file_index * 4, SEEK_CUR);
        if (file_index >= file_count)
        {
            break;
        }
        xread(dat_file, &file_start_offset, 4, 0);
        if (file_start_offset == 0)
        {
            break;
        }
        xread(dat_file, &file_end_offset, 4, 0);
        if (file_end_offset == 0)
        {
            file_end_offset = dat_size;
        }

#if CONTAINER_DEBUG
        printf("File %d: Start Offset: %08x, End Offset: %08x\n", file_index, file_start_offset, file_end_offset);
#endif
        log_line[0] = 0;
        sprintf(log_line, "File %d: Start Offset: %08x, End Offset: %08x\n", file_index, file_start_offset, file_end_offset);
        fwrite(log_line, 1, strlen(log_line), log);

        char filename[256];
        char file_extension[6];
        char file_header[32];

        file_length = file_end_offset - file_start_offset;

        if (file_length > dat_size - file_start_offset)
        {
            break;
        }

        char *file_data = (char *)malloc(file_length);
        if (file_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(dat_file);
            return EXIT_FAILURE;
        }

#if CONTAINER_DEBUG
        printf("File %d: Length: %08x\n", file_index, file_length);
#endif
        fseek(dat_file, file_start_offset, SEEK_SET);
        xread(dat_file, file_data, file_length, 0);
#if CONTAINER_DEBUG
        printf("File %d: Data read\n", file_index);
#endif
        file_header[0] = 0;
        file_extension[0] = 0;
        memcpy(file_header, file_data, 32);
        strncpy(file_extension, find_file_extension(file_header), 5);
        if (strcmp(file_extension, "bin") == 0)
        {
            if (check_for_dat_container(file_data, file_length))
            {
                strncpy(file_extension, "dat", 5);
                recursive_dat = TRUE;
            }
        }

        sprintf(filename, "%s/%08d.%s", output_dir, file_index, file_extension);

        FILE *output_file = fopen(filename, "wb");
        if (output_file == NULL)
        {
            perror("Failed to create output file");
            fclose(dat_file);
            free(file_data);
            return EXIT_FAILURE;
        }

        xwrite(output_file, file_data, file_length);
        fclose(output_file);
        /*recursive dat unpacking for dats inside dats (thanks genki)*/
        if (recursive_dat)
        {
            char dat_output_dir[256];
            sprintf(dat_output_dir, "%s/%08d", output_dir, file_index);
            extract_dat_container(filename, dat_output_dir, FALSE);
        }

        free(file_data);
        file_index++;
    }
#if CONTAINER_DEBUG
    printf("Files extracted successfully\n");
#endif
    log_line[0] = 0;
    sprintf(log_line, "Files from: %s extracted successfully\n", dat_filename);
    fwrite(log_line, 1, strlen(log_line), log);
    fclose(dat_file);
    fclose(log);
    if (!logs)
        remove("dat_extract.log");
    return EXIT_SUCCESS;
}

/*TODO: taken straight from uclpack, needs a lighter alternative rewrite*/
int do_decompress(FILE *fi, FILE *fo, unsigned long benchmark_loops, const char *filename, BOOL *dat_container)
{
    int r;
    int method;
    int level;
    ucl_uint buf_len;
    ucl_uint block_size;
    ucl_uint overhead = 0;
    ucl_uint32 checksum;
    ucl_uint32 flags;
    ucl_bytep buf = NULL;
    char header[32];
    char file_extension[6];
    char output_filename[256];
    unsigned char m[sizeof(UCL_MAGIC)];
    BOOL dat_check = 1;

    r = 0;
    header[0] = 0;

    /*
     * Step 1: check UCL_MAGIC header, read flags & block size, init checksum
     */
    if (xread(fi, m, sizeof(UCL_MAGIC), 1) != sizeof(UCL_MAGIC) ||
        memcmp(m, UCL_MAGIC, sizeof(UCL_MAGIC)) != 0)
    {
        printf("%s: header error - this file is not compressed by uclpack\n", progname);
        printf("check compatibility mode\n");
        r = 1;
        goto err;
    }
    flags = xread32(fi);
    method = xgetc(fi);
    level = xgetc(fi);
    block_size = xread32(fi);
    overhead = get_overhead(method, block_size);
    if (overhead == 0 || !set_method_name(method, level))
    {
        printf("%s: header error - invalid method %d (level %d)\n",
               progname, method, level);
        r = 2;
        goto err;
    }
    if (block_size < 32 || block_size > 32 * 1024 * 1024L)
    {
        printf("%s: header error - invalid block size %ld\n",
               progname, (long)block_size);
        r = 3;
        goto err;
    }

    checksum = ucl_adler32(0, NULL, 0);

    /*
     * Step 2: allocate buffer for in-place decompression
     */
    buf_len = block_size + overhead;
    if (benchmark_loops > 0)
    {
        /* cannot use in-place decompression when doing benchmarks */
        buf_len += block_size;
    }
    buf = (ucl_bytep)ucl_malloc(buf_len);
    if (buf == NULL)
    {
        printf("%s: out of memory\n", progname);
        r = 4;
        goto err;
    }

    /*
     * Step 3: process blocks
     */
    for (;;)
    {
        ucl_bytep in;
        ucl_bytep out;
        ucl_uint in_len;
        ucl_uint out_len;

        /* read uncompressed size */
        out_len = xread32(fi);

        /* exit if last block (EOF marker) */
        if (out_len == 0)
            break;

        /* read compressed size */
        in_len = xread32(fi);

        /* sanity check of the size values */
        if (in_len > block_size || out_len > block_size ||
            in_len == 0 || in_len > out_len)
        {
            printf("%s: block size error - data corrupted\n", progname);
            r = 5;
            goto err;
        }

        /* place compressed block at the top of the buffer */
        in = buf + buf_len - in_len;
        out = buf;

        /* read compressed block data */
        xread(fi, in, in_len, 0);

        if (in_len < out_len)
        {
            /* decompress - use safe decompressor as data might be corrupted */
            ucl_uint new_len = out_len;

            if (method == 0x2b)
            {
                r = ucl_nrv2b_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            else if (method == 0x2d)
            {
                r = ucl_nrv2d_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            else if (method == 0x2e)
            {
                r = ucl_nrv2e_decompress_le32(in, in_len, out, &new_len, NULL);
            }
            if (r != UCL_E_OK || new_len != out_len)
            {
                printf("%s: compressed data violation: error %d (0x%x: %ld/%ld/%ld)\n", progname, r, method, (long)in_len, (long)out_len, (long)new_len);
                r = 6;
                goto err;
            }
            if (header[0] == 0 && dat_check == 1)
            {
                dat_check = 0;
                fclose(fo);
                memcpy(header, out, 32);
                strncpy(file_extension, find_file_extension(header), 5);
                if (strcmp(file_extension, "bin") == 0)
                {
                    if (check_for_dat_container((char *)out, swap_uint16(out_len)))
                    {
                        strncpy(file_extension, "dat", 5);
                        *dat_container = TRUE;
                    }
                }
                sprintf(output_filename, "%s.%s", filename, file_extension);
                fo = fopen(output_filename, "wb");
                if (fo == NULL)
                {
                    perror("Failed to create output file");
                    r = 1;
                    goto err;
                }
            }
            /* write decompressed block */
            xwrite(fo, out, out_len);
            /* update checksum */
            if ((flags & 1) && !opt_fast)
                checksum = ucl_adler32(checksum, out, out_len);
        }
        else
        {
            /* write original (incompressible) block */
            xwrite(fo, in, in_len);
            /* update checksum */
            if ((flags & 1) && !opt_fast)
                checksum = ucl_adler32(checksum, in, in_len);
        }
    }

    /* read and verify checksum */
    if (flags & 1)
    {
        ucl_uint32 c = xread32(fi);
        if (!opt_fast && c != checksum)
        {
            printf("%s: checksum error - data corrupted\n", progname);
            r = 7;
            goto err;
        }
    }

    r = 0;
err:
    ucl_free(buf);
    return r;
}
/*TODO: taken straight from uclpack, needs a lighter alternative rewrite*/
int do_compress(FILE *fi, FILE *fo, int method, int level, ucl_uint block_size)
{
    int r = 0;
    ucl_bytep in = NULL;
    ucl_bytep out = NULL;
    ucl_uint in_len;
    ucl_uint out_len;
    ucl_uint32 flags = opt_fast ? 0 : 1;
    ucl_uint32 checksum;
    ucl_uint overhead = 0;

    struct ucl_compress_config_t cfg;

    /* little endian 32 config*/

    memset(&cfg, 0xff, sizeof(cfg));

    cfg.bb_endian = 0;
    cfg.bb_size = 32;
    cfg.c_flags = 0;

    /*
     * Step 1: write UCL_MAGIC header, flags & block size, init checksum
     */
    xwrite(fo, UCL_MAGIC, sizeof(UCL_MAGIC));
    xwrite32(fo, flags);
    xputc(fo, method); /* compression method */
    xputc(fo, level);  /* compression level */
    xwrite32(fo, block_size);
    checksum = ucl_adler32(0, NULL, 0);

    /*
     * Step 2: allocate compression buffers and work-memory
     */
    overhead = get_overhead(method, block_size);
    in = (ucl_bytep)ucl_malloc(block_size);
    out = (ucl_bytep)ucl_malloc(block_size + overhead);
    if (in == NULL || out == NULL)
    {
        printf("%s: out of memory\n", progname);
        r = 1;
        goto err;
    }

    /*
     * Step 3: process blocks
     */
    for (;;)
    {
        /* read block */
        in_len = xread(fi, in, block_size, 1);
        if (in_len <= 0)
            break;

        /* update checksum */
        if (flags & 1)
            checksum = ucl_adler32(checksum, in, in_len);

        /* compress block */
        r = UCL_E_ERROR;
        out_len = 0;
        if (method == 0x2b)
            r = ucl_nrv2b_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        else if (method == 0x2d)
            r = ucl_nrv2d_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        else if (method == 0x2e)
            r = ucl_nrv2e_99_compress(in, in_len, out, &out_len, 0, level, &cfg, NULL);
        if (r == UCL_E_OUT_OF_MEMORY)
        {
            printf("%s: out of memory in compress\n", progname);
            r = 1;
            goto err;
        }
        if (r != UCL_E_OK || out_len > in_len + get_overhead(method, in_len))
        {
            /* this should NEVER happen */
            printf("internal error - compression failed: %d\n", r);
            r = 2;
            goto err;
        }
        /* write uncompressed block size */
        xwrite32(fo, in_len);

        if (out_len < in_len)
        {
            /* write compressed block */
            xwrite32(fo, out_len);
            xwrite(fo, out, out_len);
        }
        else
        {
            /* not compressible - write uncompressed block */
            xwrite32(fo, in_len);
            xwrite(fo, in, in_len);
        }
    }

    /* write EOF marker */
    xwrite32(fo, 0);

    /* write checksum */
    if (flags & 1)
        xwrite32(fo, checksum);

    r = 0;
err:
    ucl_free(out);
    ucl_free(in);
    return r;
}

int decompress_GUT_Archive(const char *toc_filename, const char *dat_filename, const char *output_dir, BOOL recursive)
{
    FILE *toc_file, *dat_file;
    FILE *log;
    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, end_offset, zero_field, decompressed_size;
    ucl_uint actual_offset, actual_length;
    int file_index = 0;
    int r = -1;
    BOOL compressed = TRUE;
    BOOL dat_container = FALSE;
    char filename[256];
    char file_extension[6];
    char file_header[32];

    toc_file = fopen(toc_filename, "rb");
    if (toc_file == NULL)
    {
        perror("Failed to open .toc file");
        return EXIT_FAILURE;
    }
    dat_file = fopen(dat_filename, "rb");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        fclose(toc_file);
        return EXIT_FAILURE;
    }

    log = fopen("decomp.log", "w");

    xread(toc_file, &file_count, 4, 0);
    if (gameid == -2)
        file_count = swap_uint32(file_count);
    fseek(toc_file, 16, SEEK_SET);

#ifdef _WIN32
    _mkdir(output_dir);
#else
    mkdir(output_dir, 0700);
#endif
    printf("Extracting GUT Archive...\n");
    printf("File count: %d\n", file_count);
    while (TRUE)
    {
        dat_container = FALSE;
        if (file_index >= file_count)
        {
            progress_bar(100);
            printf("\n");
            break;
        }
        progress_bar((file_index * 100) / file_count);
        
        read_toc(toc_file, &start_offset, &compressed_size, &end_offset, &zero_field, &decompressed_size);

        actual_length = end_offset * 0x800;
        actual_offset = start_offset * 0x800;
        if (gameid == -2)
        {
            actual_offset = swap_uint32(start_offset) * 0x800;
            actual_length = swap_uint32(end_offset) * 0x800;
        }

        if (actual_offset == 0 && file_index > 1 && (gameid == 0 || gameid == -2))
        {
            zero_field = 1;
        }

        if (gameid == -2 && file_index == 0)
        { /*the file 0 is just cut-off repeat of file 1 in ITC*/
            zero_field = 1;
        }

        char log_line[256];
        sprintf(log_line, "File %d: TOC Offset: %08x, Mul. Offset: %08x, Compressed Size: %d, Length: %08x, Zero Check: %d, Decompressed Size: %d\n", file_index, start_offset, actual_offset, compressed_size, actual_length, zero_field, decompressed_size);
        fwrite(log_line, 1, strlen(log_line), log);

        if (zero_field == 1 || (gameid == -1 && file_index == 1))
        {
            file_index++;
            continue;
        }

        if (decompressed_size == 0)
        {
            compressed = FALSE;
        }
        else
        {
            compressed = TRUE;
        }

        fseek(dat_file, actual_offset, SEEK_SET);

        char *compressed_file_data = (char *)malloc(actual_length);
        if (compressed_file_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(toc_file);
            fclose(dat_file);
            return EXIT_FAILURE;
        }
        xread(dat_file, compressed_file_data, actual_length, 0);

        if (!compressed)
        {
            file_header[0] = 0;
            file_extension[0] = 0;
            memcpy(file_header, compressed_file_data, 32);
            strncpy(file_extension, find_file_extension(file_header), 5);
            if (strcmp(file_extension, "bin") == 0)
            {
                if (check_for_dat_container(compressed_file_data, actual_length))
                {
                    strncpy(file_extension, "dat", 5);
                    dat_container = TRUE;
                }
            }
            sprintf(filename, "%s/%08d.%s", output_dir, file_index, file_extension);

            FILE *output_file = fopen(filename, "wb");
            if (output_file == NULL)
            {
                perror("Failed to create output file");
                fclose(toc_file);
                fclose(dat_file);
                free(compressed_file_data);
                return EXIT_FAILURE;
            }
            xwrite(output_file, compressed_file_data, actual_length);
            fclose(output_file);

            /*recursive dat unpacking*/
            if (dat_container && recursive)
            {
                char dat_output_dir[256];
                sprintf(dat_output_dir, "%s/%08d", output_dir, file_index);
                extract_dat_container(filename, dat_output_dir, FALSE);
            }

            free(compressed_file_data);
            file_index++;
            continue;
        }

        FILE *temp_compressed_file;
        temp_compressed_file = tmpfile();
        if (temp_compressed_file == NULL)
        {
            perror("Failed to create temporary file");
            fclose(toc_file);
            fclose(dat_file);
            free(compressed_file_data);
            return EXIT_FAILURE;
        }

        xwrite(temp_compressed_file, compressed_file_data, actual_length);
        fseek(temp_compressed_file, 0, SEEK_SET);

        FILE *output_file;
        output_file = tmpfile();
        if (output_file == NULL)
        {
            perror("Failed to create temporary file");
            fclose(toc_file);
            fclose(dat_file);
            free(compressed_file_data);
            return EXIT_FAILURE;
        }
        sprintf(filename, "%s/%08d", output_dir, file_index);
        r = do_decompress(temp_compressed_file, output_file, 0, filename, &dat_container);
        fclose(temp_compressed_file);
        if (r != 0)
        {
            perror("Failed to decompress file");
            fclose(toc_file);
            fclose(dat_file);
            free(compressed_file_data);
            return EXIT_FAILURE;
        }

        fclose(output_file);
        /*recursive dat unpacking*/

        if (dat_container && recursive)
        {
            char dat_output_dir[256];
            /*stupid fix*/
            char filename2[261];
            sprintf(filename2, "%s.dat", filename);
            sprintf(dat_output_dir, "%s/%08d", output_dir, file_index);
            extract_dat_container(filename2, dat_output_dir, FALSE);
        }

        free(compressed_file_data);
        file_index++;
    }
    printf("Files extracted and decompressed successfully\n");
    fclose(toc_file);
    fclose(dat_file);
    fclose(log);
    if (!logs)
        remove("decomp.log");
    return EXIT_SUCCESS;
}

int reimport_to_GUT_Archive(const char *toc_filename, const char *dat_filename, const char *input_dir, BOOL recursive)
{
    FILE *toc_file, *dat_file;
    FILE *log;

    DIR *dir;
    struct dirent *entry;

    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, end_offset, zero_field, decompressed_size;
    ucl_uint actual_offset;
    ucl_uint32 new_len, new_decompressed_size;
    int file_index = 0;
    int r = -1;

    build_file *files;

    toc_file = fopen(toc_filename, "r+b");
    if (toc_file == NULL)
    {
        perror("Failed to open .toc file");
        return EXIT_FAILURE;
    }

    dat_file = fopen(dat_filename, "r+b");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        fclose(toc_file);
        return EXIT_FAILURE;
    }

    dir = opendir(input_dir);

    if (dir == NULL)
    {
        perror("Failed to open input directory");
        fclose(toc_file);
        fclose(dat_file);
        return EXIT_FAILURE;
    }

    log = fopen("reimport.log", "w");

    printf("Reading file info from TOC...\n");

    xread(toc_file, &file_count, 4, 0);
    if (gameid == -2)
        file_count = swap_uint32(file_count);
    fseek(toc_file, 16, SEEK_SET);

    files = (build_file *)malloc(file_count * sizeof(build_file));

    while (TRUE)
    {
        if (file_index >= file_count)
        {
            break;
        }
        
        read_toc(toc_file, &start_offset, &compressed_size, &end_offset, &zero_field, &decompressed_size);

        actual_offset = start_offset * 0x800;
        if (gameid == -2)
        {
            actual_offset = swap_uint32(start_offset) * 0x800;
        }

        if (zero_field == 1)
        {
            file_index++;
            continue;
        }
        if (decompressed_size == 0)
        {
            files[file_index].compressed = FALSE;
        }
        else
        {
            files[file_index].compressed = TRUE;
        }

        files[file_index].start_offset = start_offset;
        files[file_index].compressed_size = compressed_size;
        files[file_index].end_offset = end_offset;
        files[file_index].zero_field = zero_field;
        files[file_index].decompressed_size = decompressed_size;

        if (files[file_index].compressed == TRUE)
        {
            fseek(dat_file, actual_offset, SEEK_SET);
            fseek(dat_file, 14, SEEK_CUR);
            files[file_index].block_size = xread32(dat_file);
        }

        file_index++;
    }
    fseek(dat_file, 0, SEEK_SET);

    printf("Reimporting files to GUT Archive...\n");

    /*get file index by file name in input dir*/

    while ((entry = readdir(dir)) != NULL)
    {
        if (strstr(entry->d_name, ".") == NULL)
        {
            continue;
        }
        if (entry == NULL || entry->d_name[0] == '.')
        {
            continue;
        }

        /*check if dat*/
        if (strstr(entry->d_name, ".dat") != NULL)
        {
            if (recursive == TRUE)
            {

                char dat_name[320];
                char dir_name[320];
                sprintf(dat_name, "%s%s", input_dir, entry->d_name);
                printf("Processing DAT file: %s\n", dat_name);
                strcpy(dir_name, dat_name);
                strtok(dir_name, ".");
                if (build_dat_container(dat_name, dir_name) != EXIT_SUCCESS)
                {
                    printf("Skipping...\n");
                }
            }
        }

        char filename_index[300];
        strncpy(filename_index, entry->d_name, 260);
        strtok(filename_index, ".");

        file_index = atoi(filename_index);
        printf("Processing file id:%d\n", file_index);
        printf("Block size: %d\n", files[file_index].block_size);

        if (file_index < 0 || file_index >= file_count)
        {
            printf("Invalid file index\n");
            continue;
        }

        actual_offset = files[file_index].start_offset * 0x800;
        if (gameid == -2)
        {
            actual_offset = swap_uint32(files[file_index].start_offset) * 0x800;
        }

        if (files[file_index].zero_field == 1)
        {
            printf("Zero field != 0\n");
            continue;
        }

        char infilename[1024];
        sprintf(infilename, "%s/%s", input_dir, entry->d_name);

        FILE *input_file = fopen(infilename, "r+b");

        if (input_file == NULL)
        {
            perror("Failed to open input file");
            fclose(toc_file);
            fclose(dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(input_file, 0, SEEK_END);
        new_decompressed_size = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        if (!files[file_index].compressed)
        {
            if (new_decompressed_size > files[file_index].compressed_size)
            {
                printf("New file size is larger than the original, might break the archive!\n");
            }
            fseek(dat_file, actual_offset, SEEK_SET);
            char *uncompressed_file_data = (char *)malloc(new_decompressed_size);
            if (uncompressed_file_data == NULL)
            {
                perror("Failed to allocate memory");
                fclose(toc_file);
                fclose(dat_file);
                fclose(input_file);
                free(files);
                return EXIT_FAILURE;
            }
            xread(input_file, uncompressed_file_data, new_decompressed_size, 0);
            xwrite(dat_file, uncompressed_file_data, new_decompressed_size);
            files[file_index].compressed_size = new_decompressed_size;
            free(uncompressed_file_data);
            fclose(input_file);

            /*write changes to .toc*/
            fseek(toc_file, 0, SEEK_SET);
            fseek(toc_file, 16, SEEK_SET);
            switch (gameid)
            {
            case 0:
                fseek(toc_file, file_index * 16, SEEK_CUR);
                fseek(toc_file, 4, SEEK_CUR);
                xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
                fseek(toc_file, 8, SEEK_CUR);
                break;
            case -2:
                fseek(toc_file, file_index * 16, SEEK_CUR);
                fseek(toc_file, 4, SEEK_CUR);
                xwrite32(toc_file, files[file_index].compressed_size);
                fseek(toc_file, 8, SEEK_CUR);
                break;
            default:
                fseek(toc_file, file_index * 20, SEEK_CUR);
                fseek(toc_file, 4, SEEK_CUR);
                xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
                fseek(toc_file, 12, SEEK_CUR);
                break;
            }
            /*write to log*/
            char log_line[256];
            sprintf(log_line, "Uncompressed File %d, TOC offset %08x, DAT offset %08x, new size %d\n", file_index, files[file_index].start_offset, actual_offset, files[file_index].compressed_size);
            fwrite(log_line, 1, strlen(log_line), log);
            continue;
        }

        FILE *temp_compressed_file = tmpfile();
        if (temp_compressed_file == NULL)
        {
            perror("Failed to create temporary file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(temp_compressed_file, 0, SEEK_SET);

        r = do_compress(input_file, temp_compressed_file, 0x2b, 7, files[file_index].block_size);
        if (r != 0)
        {
            perror("Failed to compress file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            free(files);
            return EXIT_FAILURE;
        }

        new_len = ftell(temp_compressed_file);
        if (new_len > files[file_index].compressed_size)
        {
            printf("New file size is larger than the original, might break the archive!\n");
        }
        files[file_index].compressed_size = new_len;
        files[file_index].decompressed_size = new_decompressed_size;
        printf("Compressed size: %d\n", new_len);

        char *compressed_file_data = (char *)malloc(new_len);
        if (compressed_file_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(temp_compressed_file, 0, SEEK_SET);

#if REBUILDING_DEBUG == 1
        FILE *debug_file = fopen("debug.bin", "wb");
        if (debug_file == NULL)
        {
            perror("Failed to create debug file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            free(files);
            return EXIT_FAILURE;
        }
#endif

        xread(temp_compressed_file, compressed_file_data, new_len, 0);
        fseek(dat_file, 0, SEEK_SET);
        fseek(dat_file, actual_offset, SEEK_CUR);
        xwrite(dat_file, compressed_file_data, new_len);

#if REBUILDING_DEBUG == 1
        xwrite(debug_file, compressed_file_data, new_len);
#endif

        free(compressed_file_data);

        fclose(input_file);
        fclose(temp_compressed_file);

#if REBUILDING_DEBUG == 1
        fclose(debug_file);
#endif

        /*write changes to .toc*/
        fseek(toc_file, 0, SEEK_SET);
        fseek(toc_file, 16, SEEK_SET);
        switch (gameid)
        {
        case 0:
            fseek(toc_file, file_index * 16, SEEK_CUR);
            fseek(toc_file, 4, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
            xwrite32(toc_file, swap_uint32(files[file_index].decompressed_size));
            fseek(toc_file, 4, SEEK_CUR);
            break;
        case -2:
            fseek(toc_file, file_index * 16, SEEK_CUR);
            fseek(toc_file, 4, SEEK_CUR);
            xwrite32(toc_file, files[file_index].compressed_size);
            xwrite32(toc_file, files[file_index].decompressed_size);
            fseek(toc_file, 4, SEEK_CUR);
            break;
        default:
            fseek(toc_file, file_index * 20, SEEK_CUR);
            fseek(toc_file, 4, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
            fseek(toc_file, 8, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].decompressed_size));
            break;
        }
        /*write to log*/
        char log_line[256];
        sprintf(log_line, "Compressed File %d, TOC offset %08x, DAT offset %08x, New compressed size: %d, New decompressed size: %d\n", file_index, files[file_index].start_offset, actual_offset, files[file_index].compressed_size, files[file_index].decompressed_size);
        fwrite(log_line, 1, strlen(log_line), log);
    }

    printf("Files reimported to GUT Archive successfully\n");

    fclose(toc_file);
    fclose(dat_file);
    fclose(log);
    if (!logs)
        remove("reimport.log");
    free(files);
    closedir(dir);
    return EXIT_SUCCESS;
}

int rebuild_GUT_Archive(const char *toc_filename, const char *dat_filename, const char *input_dir, BOOL recursive)
{
    if (gameid != 1 && gameid != -2 && gameid != -1)
    {
        printf("Incompatible game for rebuilding method\n");
        printf("Reimporting files instead\n");
        return reimport_to_GUT_Archive(toc_filename, dat_filename, input_dir, recursive);
    }
    FILE *toc_file, *dat_file, *new_dat_file;
    FILE *log;

    DIR *dir;
    struct dirent *entry;

    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, end_offset, zero_field, decompressed_size;
    ucl_uint actual_offset;
    ucl_uint new_toc_offset;
    ucl_uint additional_offset = 0, new_additional_offset;
    ucl_uint32 new_len, old_len, new_decompressed_size;
    int file_index = 0;
    int r = -1;

    build_file *files;

    toc_file = fopen(toc_filename, "r+b");
    if (toc_file == NULL)
    {
        perror("Failed to open .toc file");
        return EXIT_FAILURE;
    }

    dat_file = fopen(dat_filename, "r+b");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        fclose(toc_file);
        return EXIT_FAILURE;
    }

    dir = opendir(input_dir);

    if (dir == NULL)
    {
        perror("Failed to open input directory");
        fclose(toc_file);
        fclose(dat_file);
        return EXIT_FAILURE;
    }

    log = fopen("rebuild.log", "w");

    printf("Reading file info from TOC...\n");

    xread(toc_file, &file_count, 4, 0);
    if (gameid == -2)
        file_count = swap_uint32(file_count);
    fseek(toc_file, 16, SEEK_SET);

    files = (build_file *)malloc(file_count * sizeof(build_file));

    while (TRUE)
    {
        if (file_index >= file_count)
        {
            break;
        }
        files[file_index].importing = FALSE;
        
        read_toc(toc_file, &start_offset, &compressed_size, &end_offset, &zero_field, &decompressed_size);

        actual_offset = start_offset * 0x800;
        if (gameid == -2)
        {
            actual_offset = swap_uint32(start_offset) * 0x800;
        }

        if (actual_offset == 0 && file_index > 1 && (gameid == 0 || gameid == -2))
        {
            zero_field = 1;
        }

        if (decompressed_size == 0)
        {
            files[file_index].compressed = FALSE;
        }
        else
        {
            files[file_index].compressed = TRUE;
        }

        files[file_index].start_offset = start_offset;
        files[file_index].compressed_size = compressed_size;
        files[file_index].end_offset = end_offset;
        files[file_index].zero_field = zero_field;
        files[file_index].decompressed_size = decompressed_size;

        if (files[file_index].compressed == TRUE)
        {
            fseek(dat_file, actual_offset, SEEK_SET);
            fseek(dat_file, 14, SEEK_CUR);
            files[file_index].block_size = xread32(dat_file);
        }

        file_index++;
    }
    fseek(dat_file, 0, SEEK_SET);

    printf("Rebuilding GUT Archive...\n");

    /*get file index by file name in input dir*/

    while ((entry = readdir(dir)) != NULL)
    {
        if (strstr(entry->d_name, ".") == NULL)
        {
            continue;
        }
        if (entry == NULL || entry->d_name[0] == '.')
        {
            continue;
        }

        /*check if dat*/
        if (strstr(entry->d_name, ".dat") != NULL)
        {
            if (recursive == TRUE)
            {
                char dat_name[320];
                char dir_name[320];
                sprintf(dat_name, "%s%s", input_dir, entry->d_name);
                printf("Processing DAT file: %s\n", dat_name);
                strcpy(dir_name, dat_name);
                strtok(dir_name, ".");
                if (build_dat_container(dat_name, dir_name) != EXIT_SUCCESS)
                {
                    printf("Skipping...\n");
                }
            }
        }

        char filename_index[300];
        strncpy(filename_index, entry->d_name, 260);
        strtok(filename_index, ".");

        file_index = atoi(filename_index);
        printf("Processing file id:%d\n", file_index);
        printf("Block size: %d\n", files[file_index].block_size);

        if (file_index < 0 || file_index >= file_count)
        {
            printf("Invalid file index for file %s\n", entry->d_name);
            continue;
        }

        files[file_index].importing = TRUE;
        sprintf(files[file_index].infilename, "%s/%s", input_dir, entry->d_name);
    }

    new_dat_file = fopen("new.dat", "wb");
    if (new_dat_file == NULL)
    {
        perror("Failed to create new .dat file");
        fclose(toc_file);
        fclose(dat_file);
        free(files);
        return EXIT_FAILURE;
    }
    for (file_index = 0; file_index < file_count; file_index++)
    {
        if (files[file_index].importing == FALSE)
        {
            if (files[file_index].zero_field == 0)
            {
                // move other files from old dat to new dat with added offset
                if (gameid == -2)
                {
                    fseek(dat_file, swap_uint32(files[file_index].start_offset) * 0x800, SEEK_SET);
                    fseek(new_dat_file, (swap_uint32(files[file_index].start_offset) + additional_offset) * 0x800, SEEK_SET);
                }
                else
                {
                    fseek(dat_file, files[file_index].start_offset * 0x800, SEEK_SET);
                    fseek(new_dat_file, (files[file_index].start_offset + additional_offset) * 0x800, SEEK_SET);
                }

                ucl_uint actual_length;
                if(gameid == -2)
                {
                    actual_length = swap_uint32(files[file_index].end_offset) * 0x800;
                }
                else
                {
                    actual_length = files[file_index].end_offset * 0x800;
                }

                char *file_data = (char *)malloc(actual_length);
                if (file_data == NULL)
                {
                    perror("Failed to allocate memory");
                    fclose(toc_file);
                    fclose(dat_file);
                    fclose(new_dat_file);
                    free(files);
                    return EXIT_FAILURE;
                }
                xread(dat_file, file_data, actual_length, 0);
                xwrite(new_dat_file, file_data, actual_length);
                free(file_data);
            }

            char log_line[256];
            if (gameid == -2)
            {
                sprintf(log_line, "Unchanged File %d: Old TOC: %08x, New TOC: %08x, Old DAT: %08x, New DAT: %08x\n", file_index, swap_uint32(files[file_index].start_offset), swap_uint32(files[file_index].start_offset) + additional_offset, swap_uint32(files[file_index].start_offset) * 0x800, (swap_uint32(files[file_index].start_offset) + additional_offset) * 0x800);
            }
            else
            {
                sprintf(log_line, "Unchanged File %d: Old TOC: %08x, New TOC: %08x, Old DAT: %08x, New DAT: %08x\n", file_index, files[file_index].start_offset, files[file_index].start_offset + additional_offset, files[file_index].start_offset * 0x800, (files[file_index].start_offset + additional_offset) * 0x800);
            }
            fwrite(log_line, 1, strlen(log_line), log);

            if (additional_offset > 0 && files[file_index].zero_field == 0)
            {
                /*write changes to .toc*/
                fseek(toc_file, 0, SEEK_SET);
                fseek(toc_file, 16, SEEK_SET);
                switch (gameid)
                {
                case 0: /*TXR:D2*/
                    fseek(toc_file, file_index * 16, SEEK_CUR);
                    xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
                    fseek(toc_file, 12, SEEK_CUR);
                    break;
                case -2: /*ITC Big endian*/
                    fseek(toc_file, file_index * 16, SEEK_CUR);
                    xwrite32(toc_file, swap_uint32(files[file_index].start_offset) + additional_offset);
                    fseek(toc_file, 12, SEEK_CUR);
                    break;
                default:
                    fseek(toc_file, file_index * 20, SEEK_CUR);
                    xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
                    fseek(toc_file, 4, SEEK_CUR);
                    xwrite32(toc_file, swap_uint32(files[file_index].end_offset));
                    fseek(toc_file, 8, SEEK_CUR);
                    break;
                }
            }
            continue;
        }
        // for imported files check if they are longer than the original, if so, add the difference to the additional offset

        actual_offset = (files[file_index].start_offset + additional_offset) * 0x800;
        new_toc_offset = files[file_index].start_offset + additional_offset;

        if (gameid == -2)
        {
            actual_offset = (swap_uint32(files[file_index].start_offset) + additional_offset) * 0x800;
            new_toc_offset = swap_uint32(files[file_index].start_offset) + additional_offset;
        }

        if (files[file_index].zero_field == 1)
        {
            printf("Zero field != 0\n");
            continue;
        }

        FILE *input_file = fopen(files[file_index].infilename, "r+b");

        if (input_file == NULL)
        {
            perror("Failed to open input file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(new_dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(input_file, 0, SEEK_END);
        new_decompressed_size = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        if (!files[file_index].compressed)
        {
            fseek(new_dat_file, actual_offset, SEEK_SET);
            char *uncompressed_file_data = (char *)malloc(new_decompressed_size);
            if (uncompressed_file_data == NULL)
            {
                perror("Failed to allocate memory");
                fclose(toc_file);
                fclose(dat_file);
                fclose(input_file);
                fclose(new_dat_file);
                free(files);
                return EXIT_FAILURE;
            }
#if REBUILDING_DEBUG == 1
            printf("[NOT UCL]New size: %d\n", new_decompressed_size);
            printf("[NOT UCL]Old size: %d\n", files[file_index].decompressed_size);
#endif

            new_additional_offset = additional_offset;

            if (gameid == -2)
            {
                while (new_toc_offset * 0x800 + new_decompressed_size > (swap_uint32(files[file_index].start_offset) + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                    files[file_index].end_offset += 1;
                }
            }
            else
            {
                while (new_toc_offset * 0x800 + new_decompressed_size > (files[file_index].start_offset + files[file_index].end_offset + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                    files[file_index].end_offset += 1;
                }
            }

            xread(input_file, uncompressed_file_data, new_decompressed_size, 0);
            xwrite(new_dat_file, uncompressed_file_data, new_decompressed_size);
            files[file_index].compressed_size = new_decompressed_size;
            free(uncompressed_file_data);
            fclose(input_file);

            /*write changes to .toc*/
            fseek(toc_file, 0, SEEK_SET);
            fseek(toc_file, 16, SEEK_SET);
            switch (gameid)
            {
            case 0: /*TXR:D2*/
                fseek(toc_file, file_index * 16, SEEK_CUR);
                xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
                xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
                fseek(toc_file, 8, SEEK_CUR);
                break;
            case -2: /*ITC Big endian*/
                fseek(toc_file, file_index * 16, SEEK_CUR);
                xwrite32(toc_file, swap_uint32(files[file_index].start_offset) + additional_offset);
                xwrite32(toc_file, files[file_index].compressed_size);
                fseek(toc_file, 8, SEEK_CUR);
                break;
            default:
                fseek(toc_file, file_index * 20, SEEK_CUR);
                xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
                xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
                xwrite32(toc_file, swap_uint32(files[file_index].end_offset));
                fseek(toc_file, 8, SEEK_CUR);
                break;
            }
            /*write to log*/
            char log_line[256];
            if (gameid == -2)
            {
                sprintf(log_line, "Uncompressed File %d, TOC offset %08x, Old DAT offset %08x, New DAT offset %08x, New size: %d\n", file_index, files[file_index].start_offset, files[file_index].start_offset * 0x800, actual_offset, files[file_index].compressed_size);
            }
            else
            {
                sprintf(log_line, "Uncompressed File %d, TOC offset %08x, Old DAT offset %08x, New DAT offset %08x, New size: %d\n", file_index, files[file_index].start_offset, files[file_index].start_offset * 0x800, actual_offset, files[file_index].compressed_size);
            }
            fwrite(log_line, 1, strlen(log_line), log);

            additional_offset = new_additional_offset;

            continue;
        }

        FILE *temp_compressed_file = tmpfile();
        if (temp_compressed_file == NULL)
        {
            perror("Failed to create temporary file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            fclose(new_dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(temp_compressed_file, 0, SEEK_SET);

        r = do_compress(input_file, temp_compressed_file, 0x2b, 7, files[file_index].block_size);
        if (r != 0)
        {
            perror("Failed to compress file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            fclose(new_dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        new_len = ftell(temp_compressed_file);

#if REBUILDING_DEBUG == 1
        printf("[UCL]New size: %d\n", new_len);
        printf("[UCL]Old size: %d\n", files[file_index].compressed_size);
        printf("[UCL]additional_offset: %d\n", additional_offset);
        printf("[UCL]start_offset + new_len: %d\n", files[file_index].start_offset * 0x800 + new_len);
        printf("[UCL]start_offset + additional_offset: %d\n", (files[file_index].start_offset + additional_offset + files[file_index].end_offset) * 0x800);
#endif

        new_additional_offset = additional_offset;

        if (gameid == -2)
        {
            while (new_toc_offset * 0x800 + new_len > (swap_uint32(files[file_index].start_offset) + new_additional_offset) * 0x800)
            {
                new_additional_offset += 1;
                files[file_index].end_offset += 1;
            }
        }
        else
        {
            while (new_toc_offset * 0x800 + new_len > (files[file_index].start_offset + files[file_index].end_offset + new_additional_offset) * 0x800)
            {
                new_additional_offset += 1;
                files[file_index].end_offset += 1;
            }
        }
        old_len = files[file_index].compressed_size;
        files[file_index].compressed_size = new_len;
        files[file_index].decompressed_size = new_decompressed_size;

        char *compressed_file_data = (char *)malloc(new_len);
        if (compressed_file_data == NULL)
        {
            perror("Failed to allocate memory");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            fclose(new_dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        fseek(temp_compressed_file, 0, SEEK_SET);

        xread(temp_compressed_file, compressed_file_data, new_len, 0);
        fseek(new_dat_file, 0, SEEK_SET);
        fseek(new_dat_file, actual_offset, SEEK_CUR);
        xwrite(new_dat_file, compressed_file_data, new_len);

        free(compressed_file_data);

        fclose(input_file);
        fclose(temp_compressed_file);

        /*write changes to .toc*/
        fseek(toc_file, 0, SEEK_SET);
        fseek(toc_file, 16, SEEK_SET);
        switch (gameid)
        {
        case 0: /*TXR:D2*/
            fseek(toc_file, file_index * 16, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
            xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
            xwrite32(toc_file, swap_uint32(files[file_index].decompressed_size));
            fseek(toc_file, 4, SEEK_CUR);
            break;
        case -2: /*ITC Big endian*/
            fseek(toc_file, file_index * 16, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].start_offset) + additional_offset);
            xwrite32(toc_file, files[file_index].compressed_size);
            xwrite32(toc_file, files[file_index].decompressed_size);
            fseek(toc_file, 4, SEEK_CUR);
            break;
        default:
            fseek(toc_file, file_index * 20, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].start_offset + additional_offset));
            xwrite32(toc_file, swap_uint32(files[file_index].compressed_size));
            xwrite32(toc_file, swap_uint32(files[file_index].end_offset));
            fseek(toc_file, 4, SEEK_CUR);
            xwrite32(toc_file, swap_uint32(files[file_index].decompressed_size));
            break;
        }
        /*write to log*/
        char log_line[256];
        if (gameid == -2)
        {
            sprintf(log_line, "Compressed File %d, TOC offset %08x, Old DAT offset %08x, New DAT offset %08x, Old compressed size: %d, New compressed size: %d, Decompressed size: %d\n", file_index, files[file_index].start_offset, files[file_index].start_offset * 0x800, actual_offset, old_len, new_len, files[file_index].decompressed_size);
        }
        else
        {
            sprintf(log_line, "Compressed File %d, TOC offset %08x, Old DAT offset %08x, New DAT offset %08x, Old compressed size: %d, New compressed size: %d, Decompressed size: %d\n", file_index, files[file_index].start_offset, files[file_index].start_offset * 0x800, actual_offset, old_len, new_len, files[file_index].decompressed_size);
        }
        fwrite(log_line, 1, strlen(log_line), log);

        additional_offset = new_additional_offset;
    }

    printf("GUT Archive rebuilt successfully\n");

    fclose(toc_file);
    fclose(dat_file);

    // delete old dat file and rename new dat file
    remove(dat_filename);
    fclose(new_dat_file);
    rename("new.dat", dat_filename);
    fclose(log);
    if (!logs)
        remove("rebuild.log");
    free(files);
    closedir(dir);
    return EXIT_SUCCESS;
}

int add_file_to_archive(const char *toc_filename, const char *dat_filename, const char *input_file){
    FILE *toc_file, *dat_file, *new_file;
    FILE *log;

    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, end_offset, zero_field, decompressed_size;
    char log_line[256];

    toc_file = fopen(toc_filename, "r+b");
    if (toc_file == NULL)
    {
        perror("Failed to open .toc file");
        return EXIT_FAILURE;
    }

    dat_file = fopen(dat_filename, "r+b");
    if (dat_file == NULL)
    {
        perror("Failed to open .dat file");
        fclose(toc_file);
        return EXIT_FAILURE;
    }

    new_file = fopen(input_file, "r+b");
    if (new_file == NULL)
    {
        perror("Failed to open input file");
        fclose(toc_file);
        fclose(dat_file);
        return EXIT_FAILURE;
    }

    log = fopen("add.log", "w");

    printf("Reading file info from TOC...\n");

    xread(toc_file, &file_count, 4, 0);
    if (gameid == -2)
        file_count = swap_uint32(file_count);

    int toc_info_size = (gameid == 0 || gameid== -2 || gameid == -3) ? 16 : 20;
    
    //fseek last file
    fseek(toc_file, 16, SEEK_SET);
    fseek(toc_file, (file_count - 1) * toc_info_size, SEEK_CUR);

    build_file last_file;
    read_toc(toc_file, &last_file.start_offset, &last_file.compressed_size, &last_file.end_offset, &last_file.zero_field, &last_file.decompressed_size);

    sprintf(log_line, "Last file: TOC offset %08x, DAT offset %08x, compressed size %d, decompressed size %d, end offset %d\n", last_file.start_offset, last_file.start_offset * 0x800, last_file.compressed_size, last_file.decompressed_size, last_file.end_offset);
    fwrite(log_line, 1, strlen(log_line), log);

    start_offset = last_file.start_offset + last_file.end_offset;
    //no ucl for now
    fseek(new_file, 0, SEEK_END);
    compressed_size = ftell(new_file);
    fseek(new_file, 0, SEEK_SET);
    decompressed_size = 0;
    end_offset = 1 + compressed_size / 0x800;
    zero_field = 0;

    sprintf(log_line, "New file: TOC offset %08x, DAT offset %08x, compressed size %d, decompressed size %d, end offset %d\n", start_offset, start_offset * 0x800, compressed_size, decompressed_size, end_offset);
    fwrite(log_line, 1, strlen(log_line), log);

    fseek(toc_file, 0, SEEK_SET);
    if(gameid == -2)
        xwrite32(toc_file, file_count + 1);
    else
        xwrite32(toc_file, swap_uint32(file_count + 1));

    fseek(toc_file, 0, SEEK_END);
    switch (gameid)
    {
    case 0: /*TXR:D2*/
        xwrite32(toc_file, swap_uint32(start_offset));
        xwrite32(toc_file, swap_uint32(compressed_size));
        xwrite32(toc_file, swap_uint32(decompressed_size));
        xwrite32(toc_file, swap_uint32(end_offset));
        xwrite32(toc_file, swap_uint32(zero_field));
        break;
    case -2: /*ITC Big endian*/
        xwrite32(toc_file, start_offset);
        xwrite32(toc_file, compressed_size);
        xwrite32(toc_file, decompressed_size);
        xwrite32(toc_file, end_offset);
        xwrite32(toc_file, zero_field);
        break;
    default:
        xwrite32(toc_file, swap_uint32(start_offset));
        xwrite32(toc_file, swap_uint32(compressed_size));
        xwrite32(toc_file, swap_uint32(end_offset));
        xwrite32(toc_file, swap_uint32(zero_field));
        xwrite32(toc_file, swap_uint32(decompressed_size));
        break;
    }
    //fseek(dat_file, start_offset * 0x800, SEEK_SET);
    fseek(dat_file, 0, SEEK_END);

    sprintf(log_line, "Adding file to DAT at offset %08x\n", start_offset * 0x800);
    fwrite(log_line, 1, strlen(log_line), log);

    fseek(new_file, 0, SEEK_SET);
    char *file_data = (char *)calloc(1,end_offset * 0x800);
    if (file_data == NULL)
    {
        perror("Failed to allocate memory");
        fclose(toc_file);
        fclose(dat_file);
        fclose(new_file);
        return EXIT_FAILURE;
    }
    xread(new_file, file_data, compressed_size, 0);
    xwrite(dat_file, file_data, end_offset * 0x800);
    free(file_data);

    printf("File added to GUT Archive successfully\n");

    fclose(toc_file);
    fclose(dat_file);
    fclose(new_file);
    fclose(log);
    if (!logs)
        remove("add.log");
    return EXIT_SUCCESS;
}

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
    printf("    -0: Tokyo Xtreme Racer DRIFT 2, Kaidou Battle 3\n");
    printf("    -1: Tokyo Xtreme Racer 3, Shutokou Battle 01\n");
    printf("    -2: Import Tuner Challenge, Shutokou Battle X\n");
    printf("    -3: Kaidou Battle 1 Taikenban\n\n");
    printf("  Logs:\n");
    printf("    -log: Creates a log file for the operation\n\n");
    printf("\n");
}

int __acc_cdecl_main main(int argc, char *argv[])
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
                logs = TRUE;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
        }
        if (REBUILDING_ALLOWED == 0)
        {
            printf("Rebuilding is disabled in this release, still in development\n");
            return 1;
        }
        result = rebuild_GUT_Archive(toc_file, dat_file, directory, FALSE);
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
                logs = TRUE;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
        }
        result = decompress_GUT_Archive(toc_file, dat_file, output_dir, FALSE);
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
                logs = TRUE;
        }
        if (argc > 5)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
        }
        strncpy(dat_file, argv[2], 255);
        strncpy(output_dir, argv[3], 255);
        result = extract_dat_container(dat_file, output_dir, TRUE);
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
                logs = TRUE;
        }
        if (argc > 5)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
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
                logs = TRUE;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(output_dir, argv[4], 255);
        if (RECURSIVE_ALLOWED == 0)
        {
            printf("Recursive extraction is disabled in this release, still in development\n");
            return 1;
        }
        result = decompress_GUT_Archive(toc_file, dat_file, output_dir, TRUE);
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
                logs = TRUE;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
        }
        strncpy(toc_file, argv[2], 255);
        strncpy(dat_file, argv[3], 255);
        strncpy(directory, argv[4], 255);
        if (RECURSIVE_ALLOWED == 0 || REBUILDING_DAT == 0)
        {
            printf("Recursive rebuilding is disabled in this release, still in development\n");
            return 1;
        }
        result = rebuild_GUT_Archive(toc_file, dat_file, directory, TRUE);
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
                logs = TRUE;
        }
        if (argc > 6)
        {
            if (strcmp(argv[6], "-log") == 0)
                logs = TRUE;
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
