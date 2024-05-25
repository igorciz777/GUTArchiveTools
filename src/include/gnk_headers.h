/**
 * Magic numbers for files used by Genki
*/

static const unsigned char UCL_MAGIC[8] = {0x00, 0xe9, 0x55, 0x43, 0x4c, 0xff, 0x01, 0x1a};
static const unsigned char XVI[8] = {0x30, 0x49, 0x56, 0x58, 0x30, 0x30, 0x2E, 0x31};
static const unsigned char TIM2[4] = {0x54, 0x49, 0x4D, 0x32};
static const unsigned char HD[8] = {0x49, 0x45, 0x43, 0x53, 0x73, 0x72, 0x65, 0x56};
static const unsigned char BD[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char BD_2[16] = {0x00, 0x07, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
static const unsigned char XDR2[4] = {0x58, 0x50, 0x52, 0x32};
static const unsigned char TX2D[4] = {0x54, 0x58, 0x32, 0x44};

static const unsigned char GIM[4] = {0x4D, 0x49, 0x47, 0x2E};
static const unsigned char GMO[4] = {0x4F, 0x4D, 0x47, 0x2E};
static const unsigned char GXT[4] = {0x47, 0x58, 0x54, 0x00};
static const unsigned char GIM_BIG[4] = {0x2E, 0x47, 0x49, 0x4D};
static const unsigned char GMO_BIG[4] = {0x2E, 0x47, 0x4D, 0x4F};
static const unsigned char GXT_BIG[4] = {0x00, 0x54, 0x58, 0x47};

static const unsigned char DDS[4] = {0x44, 0x44, 0x53, 0x20};
static const unsigned char VAG[4] = {0x56, 0x41, 0x47, 0x70};
static const unsigned char HIDE[4] = {0x45, 0x44, 0x49, 0x48};
static const unsigned char GINF[4] = {0x46, 0x4E, 0x49, 0x47};

static const unsigned char PNG[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
static const unsigned char JPG[3] = {0xFF, 0xD8, 0xFF};
static const unsigned char BMP[2] = {0x42, 0x4D};
static const unsigned char GIF[3] = {0x47, 0x49, 0x46};
static const unsigned char TGA[18] = {0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x20, 0x00, 0x00, 0x00};

/*not sure about endian of this one, found in c1gp demo*/
static const unsigned char BSPR[4] = {0x52, 0x50, 0x53, 0x42};


typedef struct
{
    const char extension[16];
    const unsigned char *magic;
    int magic_size;
} file_extension;

static const file_extension file_extensions[] = {
    {"ucl", UCL_MAGIC, 8},
    {"tm2", TIM2, 4},
    {"hd", HD, 8},
    {"bd", BD, 16},
    {"bd", BD_2, 16},
    {"xdr", XDR2, 4},
    {"txd", TX2D, 4},
    {"gim", GIM, 4},
    {"gmo", GMO, 4},
    {"gxt", GXT, 4},
    {"gim", GIM_BIG, 4},
    {"gmo", GMO_BIG, 4},
    {"gxt", GXT_BIG, 4},
    {"hide", HIDE, 4},
    {"dds", DDS, 4},
    {"vag", VAG, 4},
    {"ginf", GINF, 4},
    {"png", PNG, 8},
    {"jpg", JPG, 3},
    {"bmp", BMP, 2},
    {"gif", GIF, 3},
    {"tga", TGA, 18},
    {"bspr", BSPR, 4},
    {"xvi", XVI, 8}
};

static const char *find_file_extension(const char *file_header)
{
    for (int i = 0; i < sizeof(file_extensions) / sizeof(file_extension); i++)
    {
        if (memcmp(file_header, file_extensions[i].magic, file_extensions[i].magic_size) == 0)
        {
            return file_extensions[i].extension;
        }
    }
    return "bin";
}