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
static const unsigned char HIDE[4] = {0x45, 0x44, 0x49, 0x48};

static char *find_file_extension(const char *file_header)
{
    if (memcmp(file_header, XVI, 8) == 0)
    {
        return "xvi";
    }
    else if (memcmp(file_header, TIM2, 4) == 0)
    {
        return "tm2";
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
    else if (memcmp(file_header, XDR2, 4) == 0)
    {
        return "xdr";
    }
    else if (memcmp(file_header, TX2D, 4) == 0)
    {
        return "txd";
    }
    else if (memcmp(file_header, GIM, 4) == 0)
    {
        return "gim";
    }
    else if (memcmp(file_header, GMO, 4) == 0)
    {
        return "gmo";
    }
    else if (memcmp(file_header, HIDE, 4) == 0)
    {
        return "hide";
    }
    else
    {
        return "bin";
    }
}