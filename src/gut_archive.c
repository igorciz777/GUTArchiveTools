#include <ucl/ucl.h>
/* portability layer */
#define WANT_UCL_MALLOC 1
#define WANT_UCL_FREAD 1
#define WANT_UCL_WILDARGV 1
#include "portab.h"
#include "gnk_headers.h"
#include <dirent.h>
#include <stdio.h>

#define TEMP_DIR "temp"

static unsigned long total_in = 0;
static unsigned long total_out = 0;

static int gameid = -1;

static ucl_uint get_overhead(int method, ucl_uint size)
{
    if (method == 0x2b || method == 0x2d || method == 0x2e)
        return size / 8 + 256;
    return 0;
}

typedef struct
{
    ucl_uint start_offset;
    ucl_uint compressed_size;
    ucl_uint length_in_dat;
    ucl_uint zero_field;
    ucl_uint decompressed_size;
    BOOL compressed;
} build_file;

static char method_name[64];

static ucl_bool set_method_name(int method, int level)
{
    method_name[0] = 0;
    if (level < 1 || level > 10)
        return 0;
    if (method == 0x2b)
        sprintf(method_name, "NRV2B-99/%d", level);
    else if (method == 0x2d)
        sprintf(method_name, "NRV2D-99/%d", level);
    else if (method == 0x2e)
        sprintf(method_name, "NRV2E-99/%d", level);
    else
        return 0;
    return 1;
}

static char *find_file_extension(const char *file_header)
{
    if (memcmp(file_header, XVI, 8) == 0)
    {
        return "xvi";
    }
    else if (memcmp(file_header, TIM2, 4) == 0)
    {
        return "tm2";
        /*}else if(memcmp(file_header, XMDL, 8) == 0){
            return "xmdl";
        }else if(memcmp(file_header, DAT, 8) == 0){
            return "dat";*/
    }
    else if (memcmp(file_header, HD, 8) == 0)
    {
        return "hd";
    }
    else if (memcmp(file_header, BD, 16) == 0)
    {
        return "bd";
    }
    else if (memcmp(file_header, BD_2, 16) == 0)
    {
        return "bd";
    }
    else
    {
        return "bin";
    }
}

ucl_uint swap_uint16(ucl_uint val)
{
    return (val << 8) | (val >> 8);
}

ucl_int swap_int16(ucl_int val)
{
    return (val << 8) | ((val >> 8) & 0xFF);
}

ucl_uint32 swap_uint32(ucl_uint32 val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

ucl_int32 swap_int32(ucl_int32 val)
{
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | ((val >> 16) & 0xFFFF);
}

ucl_uint xwrite(FILE *f, const ucl_voidp buf, ucl_uint len)
{
    ucl_uint l;

    if (f != NULL)
    {
        l = (ucl_uint)ucl_fwrite(f, buf, len);
        if (l != len)
        {
            fprintf(stderr, "\nwrite error [%ld %ld]  (disk full ?)\n",
                    (long)len, (long)l);
            exit(1);
        }
    }
    total_out += len;
    return len;
}

ucl_uint xread(FILE *f, ucl_voidp buf, ucl_uint len, ucl_bool allow_eof)
{
    ucl_uint l;

    l = (ucl_uint)ucl_fread(f, buf, len);
    if (l > len)
    {
        fprintf(stderr, "\nsomething's wrong with your C library !!!\n");
        exit(1);
    }
    if (l != len && !allow_eof)
    {
        fprintf(stderr, "\nread error - premature end of file\n");
        exit(1);
    }
    total_in += l;
    return l;
}

ucl_uint32 xread32(FILE *f)
{
    unsigned char b[4];
    ucl_uint32 v;

    xread(f, b, 4, 0);
    v = (ucl_uint32)b[3] << 0;
    v |= (ucl_uint32)b[2] << 8;
    v |= (ucl_uint32)b[1] << 16;
    v |= (ucl_uint32)b[0] << 24;
    return v;
}

void xwrite32(FILE *f, ucl_uint32 v)
{
    unsigned char b[4];

    b[3] = (unsigned char)(v >> 0);
    b[2] = (unsigned char)(v >> 8);
    b[1] = (unsigned char)(v >> 16);
    b[0] = (unsigned char)(v >> 24);
    xwrite(f, b, 4);
}

int xgetc(FILE *f)
{
    unsigned char c;
    xread(f, (ucl_voidp)&c, 1, 0);
    return c;
}

void xputc(FILE *f, int c)
{
    unsigned char cc = (unsigned char)c;
    xwrite(f, (const ucl_voidp)&cc, 1);
}

int swap32(int v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | ((v & 0xff000000) >> 24);
}

/*TODO: taken straight from uclpack, needs a lighter alternative rewrite*/
int do_decompress(FILE *fi, FILE *fo, unsigned long benchmark_loops, const char *filename)
{
    char progname[] = "gut_archive";
    int opt_fast = 1;
    int r = 0;
    ucl_bytep buf = NULL;
    ucl_uint buf_len;
    unsigned char m[sizeof(UCL_MAGIC)];
    ucl_uint32 flags;
    int method;
    int level;
    ucl_uint block_size;
    ucl_uint32 checksum;
    ucl_uint overhead = 0;
    char header[16];
    header[0] = 0;
    char file_extension[6];
    char output_filename[256];

    total_in = total_out = 0;

    /*
     * Step 1: check UCL_MAGIC header, read flags & block size, init checksum
     */
    if (xread(fi, m, sizeof(UCL_MAGIC), 1) != sizeof(UCL_MAGIC) ||
        memcmp(m, UCL_MAGIC, sizeof(UCL_MAGIC)) != 0)
    {
        printf("%s: header error - this file is not compressed by uclpack\n", progname);
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
            if (header[0] == 0)
            {
                fclose(fo);
                memcpy(header, out, 16);
                strncpy(file_extension, find_file_extension(header), 5);
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

/*************************************************************************
// compress, also taken from uclpack
**************************************************************************/

int do_compress(FILE *fi, FILE *fo, int method, int level, ucl_uint block_size)
{
    int r = 0;
    int opt_fast = 1;
    char progname[] = "gut_archive";
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

    total_in = total_out = 0;

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

void usage(const char *progname)
{
    printf("Usage: %s [-r tocFile datFile inDirectory] [-d tocFile datFile outputDirectory] -0\n", progname);
    printf("  -r tocFile datFile inDirectory: rebuild files in inDirectory into datFile\n");
    printf("  -d tocFile datFile outputDirectory: decompress and output the archive to outputDirectory\n");
    printf("  -0,...: switch between compatible games (optional, use only if stated in compatible game list)\n");
    printf("    -0: Tokyo Xtreme Racer DRIFT 2\n");
}

int decompress_GUT_Archive(const char *toc_filename, const char *dat_filename, const char *output_dir)
{
    FILE *toc_file, *dat_file;
    FILE *log;
    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, length_in_dat, zero_field, decompressed_size;
    ucl_uint actual_offset, actual_length;
    int file_index = 0;
    int r = -1;
    BOOL compressed = TRUE;
    char filename[256];
    char file_extension[6];
    char file_header[16];

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

    /*TODO: do something with the first 4 values instead of skipping
    fseek(toc_file, 16, SEEK_SET);*/
    xread(toc_file, &file_count, 4, 0);
    fseek(toc_file, 16, SEEK_SET);

#ifdef _WIN32
    _mkdir(output_dir);
#else
    mkdir(output_dir, 0700);
#endif
    printf("Extracting GUT Archive...\n");
    while (TRUE)
    {
        if (file_index >= file_count)
        {
            break;
        }
        /*experimental switch for different games*/
        switch (gameid)
        {
        case 0: /*TXR:D2*/
            xread(toc_file, &start_offset, 4, 0);
            xread(toc_file, &compressed_size, 4, 0);
            xread(toc_file, &decompressed_size, 4, 0);
            xread(toc_file, &zero_field, 4, 0);
            xread(toc_file, &length_in_dat, 4, 0);
            if (length_in_dat == 0)
            {
                fseek(toc_file, -4, SEEK_CUR);
                zero_field = 1;
                break;
            }
            length_in_dat = length_in_dat - start_offset;
            fseek(toc_file, -4, SEEK_CUR);
            break;
        default:
            xread(toc_file, &start_offset, 4, 0);
            xread(toc_file, &compressed_size, 4, 0);
            xread(toc_file, &length_in_dat, 4, 0);
            xread(toc_file, &zero_field, 4, 0);
            xread(toc_file, &decompressed_size, 4, 0);
            break;
        }

        actual_length = length_in_dat * 0x800;
        actual_offset = start_offset * 0x800;

        if (actual_offset == 0 && file_index > 1 && gameid == 0)
        {
            zero_field = 1;
        }

        char log_line[256];
        sprintf(log_line, "File %d: TOC Offset: %08x, Mul. Offset: %08x, Compressed Size: %d, Length: %08x, Zero Check: %d, Decompressed Size: %d\n", file_index, start_offset, actual_offset, compressed_size, actual_length, zero_field, decompressed_size);
        fwrite(log_line, 1, strlen(log_line), log);

        if (zero_field == 1)
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
            memcpy(file_header, compressed_file_data, 16);
            strncpy(file_extension, find_file_extension(file_header), 5);

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
        r = do_decompress(temp_compressed_file, output_file, 0, filename);
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
        free(compressed_file_data);
        file_index++;
    }
    printf("Files extracted and decompressed successfully\n");
    fclose(toc_file);
    fclose(dat_file);
    fclose(log);
    return EXIT_SUCCESS;
}

int rebuild_GUT_Archive(const char *toc_filename, const char *dat_filename, const char *input_dir)
{
    FILE *toc_file, *dat_file;
    FILE *log;

    DIR *dir;
    struct dirent *entry;
    
    ucl_uint file_count;
    ucl_uint start_offset, compressed_size, length_in_dat, zero_field, decompressed_size;
    ucl_uint actual_offset, actual_length;
    int file_index = 0;
    int r = -1;

    build_file *files;

    toc_file = fopen(toc_filename, "rb");
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
    fseek(toc_file, 16, SEEK_SET);

    files = (build_file *)malloc(file_count * sizeof(build_file));

    while (TRUE)
    {
        if (file_index >= file_count)
        {
            break;
        }
        switch (gameid)
        {
        case 0: /*TXR:D2*/
            xread(toc_file, &start_offset, 4, 0);
            xread(toc_file, &compressed_size, 4, 0);
            xread(toc_file, &decompressed_size, 4, 0);
            xread(toc_file, &zero_field, 4, 0);
            xread(toc_file, &length_in_dat, 4, 0);
            if (length_in_dat == 0)
            {
                fseek(toc_file, -4, SEEK_CUR);
                zero_field = 1;
                break;
            }
            length_in_dat = length_in_dat - start_offset;
            fseek(toc_file, -4, SEEK_CUR);
            break;
        default:
            xread(toc_file, &start_offset, 4, 0);
            xread(toc_file, &compressed_size, 4, 0);
            xread(toc_file, &length_in_dat, 4, 0);
            xread(toc_file, &zero_field, 4, 0);
            xread(toc_file, &decompressed_size, 4, 0);
            break;
        }

        actual_length = length_in_dat * 0x800;
        actual_offset = start_offset * 0x800;

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
        files[file_index].length_in_dat = length_in_dat;
        files[file_index].zero_field = zero_field;
        files[file_index].decompressed_size = decompressed_size;

        file_index++;
    }

    printf("Rebuilding GUT Archive...\n");

    /*get file index by file name in input dir*/

    while ((entry = readdir(dir)) != NULL)
    {
        if(entry == NULL || entry->d_name[0] == '.')
        {
            continue;
        }
        
        
        char filename_index[300];
        strncpy(filename_index, entry->d_name, 260);
        strtok(filename_index, ".");
        
        file_index = atoi(filename_index);
        printf("Processing file id:%d\n", file_index);

        if (file_index < 0 || file_index >= file_count)
        {
            printf("Invalid file index\n");
            continue;
        }

        actual_length = files[file_index].length_in_dat * 0x800;
        actual_offset = files[file_index].start_offset * 0x800;

        if (files[file_index].zero_field == 1)
        {
            printf("Zero field != 0\n");
            continue;
        }

        char infilename[1024];
        sprintf(infilename, "%s/%s", input_dir, entry->d_name);

        FILE *input_file = fopen(infilename, "rb");
        
        if (input_file == NULL)
        {
            perror("Failed to open input file");
            fclose(toc_file);
            fclose(dat_file);
            free(files);
            return EXIT_FAILURE;
        }

        if(!files[file_index].compressed){
            fseek(dat_file, actual_offset, SEEK_SET);
            char *uncompressed_file_data = (char *)malloc(actual_length);
            if (uncompressed_file_data == NULL)
            {
                perror("Failed to allocate memory");
                fclose(toc_file);
                fclose(dat_file);
                fclose(input_file);
                free(files);
                return EXIT_FAILURE;
            }
            xread(input_file, uncompressed_file_data, actual_length, 0);
            xwrite(dat_file, uncompressed_file_data, actual_length);
            free(uncompressed_file_data);
            fclose(input_file);
            char log_line[256];
            sprintf(log_line, "File %d, TOC offset %08x, DAT offset %08x, not ucl compressed, reinserted.\n", file_index, files[file_index].start_offset, actual_offset);
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

        r = do_compress(input_file, temp_compressed_file, 0x2b, 7, 0x4000);
        if (r != 0)
        {
            perror("Failed to compress file");
            fclose(toc_file);
            fclose(dat_file);
            fclose(input_file);
            free(files);
            return EXIT_FAILURE;
        }

        char *compressed_file_data = (char *)malloc(actual_length);
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
        fseek(dat_file, actual_offset, SEEK_SET);
        xread(temp_compressed_file, compressed_file_data, files[file_index].compressed_size, 0);
        xwrite(dat_file, compressed_file_data, actual_length);

        free(compressed_file_data);

        fclose(input_file);
        fclose(temp_compressed_file);

        char log_line[256];
        sprintf(log_line, "File %d, TOC offset %08x, DAT offset %08x, ucl compressed, reinserted.\n", file_index, files[file_index].start_offset, actual_offset); 
        fwrite(log_line, 1, strlen(log_line), log);
    }

    printf("GUT Archive rebuilt successfully\n");

    fclose(toc_file);
    fclose(dat_file);
    fclose(log);
    free(files);
    closedir(dir);
    return EXIT_SUCCESS;
}
int __acc_cdecl_main main(int argc, char *argv[])
{
    char toc_file[256], dat_file[256], output_dir[256], directory[256];
    char mode;
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
        mode = 'r';
        result = rebuild_GUT_Archive(toc_file, dat_file, directory);
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
            char game[3];
            strncpy(game, argv[5], 2);
            gameid = atoi(game);
        }
        mode = 'd';
        result = decompress_GUT_Archive(toc_file, dat_file, output_dir);
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
