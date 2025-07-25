#ifndef __GUT_ARCHIVE_INCLUDED
#define __GUT_ARCHIVE_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <ucl/ucl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "ucl_func.h"
#include "gut_file.h"

enum GAME_ID
{
    /* No comp. flag*/
    DEFAULT,
    /* KB3 engine games: KB3, TXRD2, KR2, WM Portable, Ninkyouden */
    KB3_T,
    /* ITC engine games: ITC, SBX */
    ITC_T,
    /* KB1 Taikenban only weird format */
    KB1T_T,
    /* KB2 PurePure 2 Volume 10 Demo only format */
    KB2D_T
};

static int _GAME_ID = DEFAULT;
static bool _LOGS = false;

typedef struct toc_entry_t
{
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t compressed_size;
    uint32_t decompressed_size;
    uint32_t zero_field;
} toc_entry_t;

typedef struct rebuild_entry_t
{
    bool importing;
    bool compressed;
    bool skip;
    uint32_t block_size;
    char infilename[300];
    toc_entry_t toc_entry;
} rebuild_entry_t;

/**
 * cool progress bar B-)
 */
inline void progress_bar(int percentage)
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

/**
 * Set _GAME_ID flag from command line argument
 */
void set_game_id(char *arg)
{
    if (arg[0] == '-' && arg[1] == '0')
    {
        _GAME_ID = KB3_T;
    }
    else if (arg[0] == '-' && arg[1] == '2')
    {
        _GAME_ID = ITC_T;
    }
    else if (arg[0] == '-' && arg[1] == '3')
    {
        _GAME_ID = KB1T_T;
    }
    else if (arg[0] == '-' && arg[1] == '4')
    {
        _GAME_ID = KB2D_T;
    }
}

/**
 * Checks if a file is a .dat datafile
 */
bool datafile_check(FILE* file)
{
    uint32_t file_size;
    uint32_t file_count;
    uint32_t offset;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0x8)
    {
        return false; // Not enough data for a valid .dat file
    }
    xread(file, &file_count, 4, 0);
    if(file_count == 0 || file_count > ((file_size / 0x4) + 0x4))
    {
        return false; // Invalid file count or too large for a .dat file
    }
    for(uint32_t i = 0; i < file_count; i++)
    {
        xread(file, &offset, 4, 0);
        if (offset == 0 || offset >= file_size || offset== 0xFFFFFFFF)
        {
            return false; // Invalid offset
        }
        if (file_count == 0x0F && offset == 0x29 && i < 1)
        {
            return false; // Bakumatsuden edge case
        }
    }
    fseek(file, 0, SEEK_SET);
    return true; // Valid .dat file
}

/**
 * Finds file extension in file header or footer
 * */
const char* get_file_extension(FILE* file)
{
    char header[32];
    char footer[32];

    if(datafile_check(file))
    {
        return "dat";
    }

    fseek(file, 0, SEEK_SET);
    xread(file, header, 32, 0);
    fseek(file, -32, SEEK_END);
    xread(file, footer, 32, 0);
    fseek(file, 0, SEEK_SET);

    const char *ext = find_file_extension_header(header);
    if(strcmp(ext, "bin") == 0)
    {
        ext = find_file_extension_footer(footer,32);
    }
    return ext;
}

/**
 * Reads the table of contents from BUILD.TOC
 * Returns the file count
 */
uint32_t get_TOC_entries(FILE *toc_file, toc_entry_t **entries)
{
    uint32_t file_count;

    xread(toc_file, &file_count, 4, 0);

    if(_GAME_ID == ITC_T) file_count = swap_uint32(file_count);

    fseek(toc_file, 0x10, SEEK_SET);

    *entries = (toc_entry_t *)malloc(file_count * sizeof(toc_entry_t));
    if (*entries == NULL)
    {
        perror("Failed to allocate memory for TOC entries");
        return 0;
    }

    for (uint32_t i = 0; i < file_count; i++)
    {
        toc_entry_t *entry = &(*entries)[i];
        switch(_GAME_ID)
        {
            case KB3_T:
                xread(toc_file, &entry->start_offset, 4, 0);
                xread(toc_file, &entry->compressed_size, 4, 0);
                xread(toc_file, &entry->decompressed_size, 4, 0);
                xread(toc_file, &entry->zero_field, 4, 0);
                entry->end_offset = 1 + (entry->compressed_size / 0x800);
                break;
            case ITC_T:
                xread(toc_file, &entry->start_offset, 4, 0);
                xread(toc_file, &entry->compressed_size, 4, 0);
                entry->compressed_size = swap_uint32(entry->compressed_size);
                xread(toc_file, &entry->decompressed_size, 4, 0);
                entry->decompressed_size = swap_uint32(entry->decompressed_size);
                xread(toc_file, &entry->zero_field, 4, 0);
                if (entry->zero_field != 0)
                    entry->end_offset = 0;
                else
                    entry->end_offset = swap_uint32(1 + (entry->compressed_size / 0x800));
                break;
            case KB1T_T:
                xread(toc_file, &entry->start_offset, 4, 0);
                xread(toc_file, &entry->compressed_size, 4, 0);
                xread(toc_file, &entry->end_offset, 4, 0);
                xread(toc_file, &entry->zero_field, 4, 0);
                entry->decompressed_size = 0;
                if (entry->compressed_size == 0)
                    entry->zero_field = 1;
                break;
            default:
                xread(toc_file, &entry->start_offset, 4, 0);
                xread(toc_file, &entry->compressed_size, 4, 0);
                xread(toc_file, &entry->end_offset, 4, 0);
                xread(toc_file, &entry->zero_field, 4, 0);
                xread(toc_file, &entry->decompressed_size, 4, 0);
                break;
        }
    }
    return file_count;
}

/**
 * Writes changes to TOC from an array of toc_entry_t
 */
void write_TOC_entries(FILE *toc_file, rebuild_entry_t *entries, uint32_t file_count)
{
    fseek(toc_file, 0x10, SEEK_SET);
    for (uint32_t i = 0; i < file_count; i++)
    {
        toc_entry_t *entry = &entries[i].toc_entry;
        switch(_GAME_ID)
        {
            case KB3_T:
                xwrite(toc_file, (void*)&entry->start_offset, 0x4);
                xwrite(toc_file, (void*)&entry->compressed_size, 0x4);
                xwrite(toc_file, (void*)&entry->decompressed_size, 0x4);
                xwrite(toc_file, (void*)&entry->zero_field, 0x4);
                break;
            case ITC_T:
                xwrite(toc_file, (void*)&entry->start_offset, 0x4);
                xwrite32(toc_file, entry->compressed_size);
                xwrite32(toc_file, entry->decompressed_size);
                xwrite(toc_file, (void*)&entry->zero_field, 0x4);
                break;
            case KB1T_T:
                xwrite(toc_file, (void*)&entry->start_offset, 0x4);
                xwrite(toc_file, (void*)&entry->compressed_size, 0x4);
                xwrite(toc_file, (void*)&entry->end_offset, 0x4);
                xwrite(toc_file, (void*)&entry->zero_field, 0x4);
                break;
            default:
                xwrite(toc_file, (void*)&entry->start_offset, 0x4);
                xwrite(toc_file, (void*)&entry->compressed_size, 0x4);
                xwrite(toc_file, (void*)&entry->end_offset, 0x4);
                xwrite(toc_file, (void*)&entry->zero_field, 0x4);
                xwrite(toc_file, (void*)&entry->decompressed_size, 0x4);
                break;
        }
    }
}

/**
 * Extracts all entries from a .dat datafile to a specified directory
 * Returns 0 on success, non-zero on failure
 */

int extract_datafile(FILE *datafile, const char *output_dir)
{
    uint32_t file_size;
    uint32_t file_count;
    uint32_t *offsets;

    if(!datafile_check(datafile))
    {
        fprintf(stderr, "Error: Not a valid .dat file\n");
        return EXIT_FAILURE;
    }
    fseek(datafile, 0, SEEK_END);
    file_size = ftell(datafile);
    fseek(datafile, 0, SEEK_SET);
    xread(datafile, &file_count, 4, 0);

    offsets = (uint32_t *)malloc((file_count+1) * sizeof(uint32_t));
    if (offsets == NULL)
    {
        perror("Failed to allocate memory for offsets");
        return EXIT_FAILURE;
    }
    for (uint32_t i = 0; i < file_count; i++)
    {
        xread(datafile, &offsets[i], 4, 0);
        if (offsets[i] == 0 || offsets[i] >= file_size || offsets[i] == 0xFFFFFFFF)
        {
            fprintf(stderr, "Error: Invalid offset in .dat file\n");
            free(offsets);
            return EXIT_FAILURE;
        }
    }
    offsets[file_count] = file_size; // Last offset is the end of the file

#ifdef _WIN32
    mkdir(output_dir);
#else
    mkdir(output_dir, 0700);
#endif

    for (uint32_t file_idx = 0; file_idx < file_count; file_idx++)
    {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/temp%d", output_dir, file_idx);
        FILE *out = fopen(filename, "wb");
        if (out == NULL)
        {
            perror("Failed to create output file");
            free(offsets);
            return EXIT_FAILURE;
        }

        uint32_t actual_length = offsets[file_idx+1] - offsets[file_idx];
        uint32_t actual_offset = offsets[file_idx];
        fseek(datafile, actual_offset, SEEK_SET);
        char *file_data = (char *)malloc(actual_length);
        if (file_data == NULL)
        {
            perror("Failed to allocate memory for file data");
            fclose(out);
            free(offsets);
            return EXIT_FAILURE;
        }
        xread(datafile, file_data, actual_length, 0);
        xwrite(out, file_data, actual_length);
        free(file_data);
        fclose(out);

        out = fopen(filename, "rb");
        if (out == NULL)
        {
            perror("Failed to reopen output file");
            free(offsets);
            return EXIT_FAILURE;
        }
        const char *ext = get_file_extension(out);
        fclose(out);
        
        char new_filename[256];
        snprintf(new_filename, sizeof(new_filename), "%s/%08d.%s", output_dir, file_idx, ext);
        if (rename(filename, new_filename) != 0)
        {
            perror("Failed to rename output file");
            remove(filename);
        }
    }

    free(offsets);
    printf("All files extracted successfully\n");
    fclose(datafile);
    return EXIT_SUCCESS;
}

/**
 * Rebuilds a .dat datafile from a directory of files
 * Returns 0 on success, non-zero on failure
 */
int rebuild_datafile(FILE *dat_file, const char *input_dir)
{
    // Implementation of rebuilding the .dat file
    return 0;
}

/**
 * Extracts a single entry from the GUT archive
 * Returns 0 on success, non-zero on failure
 */
int extract_GUTArchive_entry(FILE* toc_file, FILE* dat_file, FILE* out, toc_entry_t *entry)
{
    bool compressed = false;
    uint32_t actual_length;
    uint32_t actual_offset;

    actual_length = entry->end_offset * 0x800;
    actual_offset = entry->start_offset * 0x800;
    if (_GAME_ID == ITC_T)
    {
        actual_length = swap_uint32(entry->end_offset) * 0x800;
        actual_offset = swap_uint32(entry->start_offset) * 0x800;
    }

    if(entry->decompressed_size == 0) compressed = false;
    else compressed = true;

    fseek(dat_file, actual_offset, SEEK_SET);
    char *file_data = (char *)malloc(actual_length);
    if (file_data == NULL)
    {
        perror("Failed to allocate memory");
        return EXIT_FAILURE;
    }

    xread(dat_file, file_data, actual_length, 0);

    if(!compressed)
    {
        xwrite(out, file_data, actual_length);
    }
    else
    {
        FILE *temp_compressed_file = tmpfile();
        if (temp_compressed_file == NULL)
        {
            perror("Failed to create temporary file");
            free(file_data);
            return EXIT_FAILURE;
        }
        xwrite(temp_compressed_file, file_data, actual_length);
        fseek(temp_compressed_file, 0, SEEK_SET);
        free(file_data);

        if(do_decompress(temp_compressed_file, out) != 0)
        {
            fprintf(stderr, "Decompression failed for entry %08x\n", entry->start_offset);
            fclose(out);
            fclose(temp_compressed_file);
            fclose(toc_file);
            fclose(dat_file);
            return EXIT_FAILURE;
        }
        fclose(temp_compressed_file);
    }

    fseek(out, 0, SEEK_SET);
    
    return EXIT_SUCCESS;
}

/**
 * Extracts all entries from the GUT archive to a specified directory
 * Returns 0 on success, non-zero on failure
 */
int extract_GUTArchive_all(FILE* toc_file, FILE* dat_file, const char *output_dir)
{
    uint32_t file_count = 0;
    toc_entry_t *entries = NULL;

    file_count = get_TOC_entries(toc_file, &entries);
    if (file_count == 0)
    {
        fprintf(stderr, "Failed to read TOC entries\n");
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    mkdir(output_dir);
#else
    mkdir(output_dir, 0700);
#endif

    for (uint32_t file_idx = 0; file_idx < file_count; file_idx++)
    {
        if ((entries[file_idx].start_offset == 0 && file_idx > 1 && (_GAME_ID == KB3_T || _GAME_ID == ITC_T || _GAME_ID == KB2D_T)) ||
            (entries[file_idx].zero_field == 1 && _GAME_ID != KB3_T) ||
            (file_idx == 0 && _GAME_ID == ITC_T)) // file 0 is a cut off copy of file 1 in ITC/SBX
        {
            continue;
        }
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/temp%d", output_dir, file_idx);
        FILE *out = fopen(filename, "wb");
        if (out == NULL)
        {
            perror("Failed to create output file");
            free(entries);
            return EXIT_FAILURE;
        }

        if (extract_GUTArchive_entry(toc_file, dat_file, out, &entries[file_idx]) != 0)
        {
            fprintf(stderr, "Failed to extract entry %08x\n", entries[file_idx].start_offset);
            free(entries);
            fclose(out);
            return EXIT_FAILURE;
        }
        progress_bar((file_idx * 100) / file_count);

        fclose(out);

        out = fopen(filename, "rb");
        if (out == NULL)
        {
            perror("Failed to reopen output file");
            free(entries);
            return EXIT_FAILURE;
        }
        const char *ext = get_file_extension(out);

        fclose(out);
        
        char new_filename[256];
        snprintf(new_filename, sizeof(new_filename), "%s/%08d.%s", output_dir, file_idx, ext);
        if (rename(filename, new_filename) != 0)
        {
            perror("Failed to rename output file");
            remove(filename);
        }
    }
    progress_bar(100);
    printf("\n");
    free(entries);
    fclose(toc_file);
    fclose(dat_file);
    printf("All files extracted successfully\n");
    return EXIT_SUCCESS;
}

/**
 * Rebuilds a GUT archive from a directory of files
 * Returns 0 on success, non-zero on failure
 */
// TODO: rewrite needed
int rebuild_GUTArchive(const char *toc_filename, const char *dat_filename, const char *input_dir)
{
    DIR *dir;
    struct dirent *entry;

    ucl_uint file_count;
    ucl_uint actual_offset;
    ucl_uint new_toc_offset;
    ucl_uint additional_offset = 0, new_additional_offset;
    ucl_uint32 new_compressed_size, new_decompressed_size;
    ucl_uint32 padded_length;
    ucl_uint file_index = 0;
    int result = -1;

    toc_entry_t *toc_entries;
    rebuild_entry_t *files;

    FILE *toc_file = fopen(toc_filename, "r+b");
    if (toc_file == NULL)
    {
        perror("Failed to open .toc file");
        return EXIT_FAILURE;
    }

    FILE *dat_file = fopen(dat_filename, "r+b");
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

    printf("Reading file info from TOC...\n");

    file_count = get_TOC_entries(toc_file, &toc_entries);
    if (file_count == 0)
    {
        fprintf(stderr, "Failed to read TOC entries\n");
        return EXIT_FAILURE;
    }
    files = (rebuild_entry_t *)malloc(file_count * sizeof(rebuild_entry_t));
    if (files == NULL)
    {
        perror("Failed to allocate memory for files");
        fclose(toc_file);
        fclose(dat_file);
        closedir(dir);
        return EXIT_FAILURE;
    }

    for (file_index = 0; file_index < file_count; file_index++)
    {
        files[file_index].importing = false;
        files[file_index].compressed = false;
        files[file_index].skip = false;
        files[file_index].block_size = 0;
        files[file_index].toc_entry = toc_entries[file_index];

        if (_GAME_ID == KB3_T)
            files[file_index].skip = false;

        actual_offset = toc_entries[file_index].start_offset * 0x800;
        if (_GAME_ID == ITC_T)
        {
            actual_offset = swap_uint32(toc_entries[file_index].start_offset) * 0x800;
        }

        if (actual_offset == 0 && file_index > 1 && (_GAME_ID == KB3_T || _GAME_ID == ITC_T))
        {
            files[file_index].skip = true;
        }

        if (toc_entries[file_index].decompressed_size == 0)
        {
            files[file_index].compressed = false;
        }
        else
        {
            files[file_index].compressed = true;
        }

        if (files[file_index].compressed == true)
        {
            fseek(dat_file, actual_offset, SEEK_SET);
            fseek(dat_file, 14, SEEK_CUR);
            files[file_index].block_size = xread32(dat_file);
        }
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

        char filename_index[300];
        strncpy(filename_index, entry->d_name, 260);
        strtok(filename_index, ".");

        file_index = atoi(filename_index);
        printf("Processing file id:%d\n", file_index);
        printf("Block size: %d\n", files[file_index].block_size);

        if (file_index >= file_count)
        {
            printf("Invalid file index for file %s\n", entry->d_name);
            continue;
        }

        files[file_index].importing = true;
        sprintf(files[file_index].infilename, "%s/%s", input_dir, entry->d_name);
    }

    FILE *new_dat_file = fopen("new.dat", "wb");
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
        if (files[file_index].importing == false)
        {
            if (!files[file_index].skip)
            {
                // move other files from old dat to new dat with added offset
                if (_GAME_ID == ITC_T)
                {
                    fseek(dat_file, swap_uint32(files[file_index].toc_entry.start_offset) * 0x800, SEEK_SET);
                    fseek(new_dat_file, (swap_uint32(files[file_index].toc_entry.start_offset) + additional_offset) * 0x800, SEEK_SET);
                }
                else
                {
                    fseek(dat_file, files[file_index].toc_entry.start_offset * 0x800, SEEK_SET);
                    fseek(new_dat_file, (files[file_index].toc_entry.start_offset + additional_offset) * 0x800, SEEK_SET);
                }

                uint32_t actual_length;
                if (_GAME_ID == ITC_T)
                {
                    actual_length = swap_uint32(files[file_index].toc_entry.end_offset) * 0x800;
                }
                else
                {
                    actual_length = files[file_index].toc_entry.end_offset * 0x800;
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

                if (additional_offset > 0)
                {
                    files[file_index].toc_entry.start_offset += additional_offset;
                }
            }
            continue;
        }
        // for imported files check if they are longer than the original, if so, add the difference to the additional offset
        if (_GAME_ID == ITC_T)
        {
            actual_offset = (swap_uint32(files[file_index].toc_entry.start_offset) + additional_offset) * 0x800;
            new_toc_offset = swap_uint32(files[file_index].toc_entry.start_offset) + additional_offset;
        }else
        {
            actual_offset = (files[file_index].toc_entry.start_offset + additional_offset) * 0x800;
            new_toc_offset = files[file_index].toc_entry.start_offset + additional_offset;
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

            new_additional_offset = additional_offset;
            padded_length = 1 + (new_decompressed_size / 0x800);

            char *uncompressed_file_data = (char *)calloc(padded_length * 0x800, sizeof(char));
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

            if (_GAME_ID == ITC_T)
            {
                while ((new_toc_offset + padded_length) * 0x800 > (swap_uint32(files[file_index].toc_entry.start_offset) + swap_uint32(files[file_index].toc_entry.end_offset) + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                }
            }
            else
            {
                while ((new_toc_offset + padded_length) * 0x800 > (files[file_index].toc_entry.start_offset + files[file_index].toc_entry.end_offset + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                }
            }

            xread(input_file, uncompressed_file_data, new_decompressed_size, 0);
            xwrite(new_dat_file, uncompressed_file_data, padded_length * 0x800);
            free(uncompressed_file_data);
            fclose(input_file);

            files[file_index].toc_entry.start_offset += additional_offset;
            files[file_index].toc_entry.compressed_size = new_decompressed_size;
            files[file_index].toc_entry.end_offset = padded_length;

            additional_offset = new_additional_offset;
        }
        else // compressed
        {
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

            result = do_compress(input_file, temp_compressed_file, 0x2b, 7, files[file_index].block_size);
            if (result != 0)
            {
                perror("Failed to compress file");
                fclose(toc_file);
                fclose(dat_file);
                fclose(input_file);
                fclose(new_dat_file);
                free(files);
                return EXIT_FAILURE;
            }

            new_compressed_size = ftell(temp_compressed_file);

            new_additional_offset = additional_offset;
            padded_length = 1 + (new_compressed_size / 0x800);

            if (_GAME_ID == ITC_T)
            {
                while ((new_toc_offset + padded_length) * 0x800 >
                (swap_uint32(files[file_index].toc_entry.start_offset)
                + swap_uint32(files[file_index].toc_entry.end_offset) + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                }
            }
            else
            {
                while ((new_toc_offset + padded_length) * 0x800 >
                (files[file_index].toc_entry.start_offset + files[file_index].toc_entry.end_offset + new_additional_offset) * 0x800)
                {
                    new_additional_offset += 1;
                }
            }
            char *compressed_file_data = (char *)calloc(padded_length * 0x800, sizeof(char));
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

            xread(temp_compressed_file, compressed_file_data, new_compressed_size, 0);
            fseek(new_dat_file, actual_offset, SEEK_SET);
            xwrite(new_dat_file, compressed_file_data, padded_length * 0x800);

            free(compressed_file_data);

            fclose(input_file);
            fclose(temp_compressed_file);

            files[file_index].toc_entry.start_offset += additional_offset;
            files[file_index].toc_entry.compressed_size = new_compressed_size;
            files[file_index].toc_entry.decompressed_size = new_decompressed_size;
            files[file_index].toc_entry.end_offset = padded_length;

            additional_offset = new_additional_offset;
        }
    }
    // write changes to TOC
    write_TOC_entries(toc_file, files, file_count);

    printf("GUT Archive rebuilt successfully\n");

    fclose(toc_file);
    fclose(dat_file);

    // delete old dat file and rename new dat file
    remove(dat_filename);
    fclose(new_dat_file);
    rename("new.dat", dat_filename);
    free(files);
    closedir(dir);
    return EXIT_SUCCESS;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */
