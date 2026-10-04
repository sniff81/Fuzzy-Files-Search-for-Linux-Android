#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <termios.h>
#include <errno.h>

#define MAX_PATH_LEN 1024
#define MAX_TOKENS 16
#define MAX_TOKEN_LEN 64

#define COLOR_RESET "\033[0m"
#define COLOR_PATH  "\033[1;34m"
#define COLOR_MATCH "\033[1;31m"
#define COLOR_INFO  "\033[1;32m"
#define COLOR_DATE  "\033[1;34m"

static volatile sig_atomic_t keep_running = 1;

static unsigned long long scanned_entries = 0;
static unsigned long long scanned_files = 0;
static unsigned long long scanned_directories = 0;
static unsigned long long skipped_directories = 0;
static unsigned long long skipped_symlinks = 0;
static unsigned long long stat_errors = 0;

typedef struct {
    char internal[MAX_PATH_LEN];
    char external[MAX_PATH_LEN];
    int has_external;
} StoragePaths;

typedef struct {
    char tokens[MAX_TOKENS][MAX_TOKEN_LEN];
    int count;
} QueryConfig;

typedef struct {
    int number;
    const char *name;
    const char *extensions;
} SearchCategory;


/* ============================================================
 * EXTENSION LISTS
 * ============================================================ */

static const char *ALL_EXTENSIONS[] = {

    /* Photos / images */
    ".jpg",".jpeg",".jpe",".jfif",".png",".gif",".bmp",".dib",".webp",
    ".tif",".tiff",".heic",".heif",".avif",".ico",".icns",".raw",".cr2",
    ".cr3",".nef",".nrw",".arw",".orf",".rw2",".raf",".dng",".pef",
    ".srw",".x3f",".3fr",".iiq",".psd",".xcf",".ai",".eps",".svg",
    ".svgz",".apng",".jp2",".j2k",".jpf",".jpx",".jpm",".mj2",".dds",
    ".tga",".pcx",".ppm",".pgm",".pbm",".pnm",".hdr",".exr",".qoi",
    ".kra",".ora",".clip",".ase",".aseprite",".xcf",

    /* Audio / music */
    ".mp3",".wav",".wave",".flac",".m4a",".aac",".ogg",".oga",".opus",
    ".wma",".aiff",".aif",".aifc",".mid",".midi",".amr",".awb",".ape",
    ".alac",".ac3",".dts",".mka",".mpc",".spx",".ra",".rm",".voc",
    ".au",".snd",".caf",".dsf",".dff",".wv",".tta",".tak",".ofr",".ofs",

    /* Documents */
    ".txt",".text",".md",".markdown",".rst",".rtf",".csv",".tsv",".log",
    ".xml",".json",".yaml",".yml",".ini",".cfg",".conf",".properties",
    ".pdf",".doc",".docx",".docm",".dot",".dotx",".dotm",".xls",".xlsx",
    ".xlsm",".xlsb",".xlt",".xltx",".ppt",".pptx",".pptm",".pps",".ppsx",
    ".odt",".ott",".ods",".ots",".odp",".otp",".odg",".odf",".epub",
    ".mobi",".azw",".azw3",".azw4",".fb2",".djvu",".djv",".tex",".bib",
    ".pages",".numbers",".key",".wpd",".wps",".msg",".eml",".ics",".vcf",
    ".html",".htm",".xhtml",".mht",".mhtml",".srt",".vtt",".ass",".ssa",

    /* Archives */
    ".zip",".zipx",".rar",".7z",".tar",".gz",".tgz",".bz",".bz2",".tbz",
    ".tbz2",".xz",".txz",".zst",".lz",".lz4",".lzh",".lha",".cab",".arj",
    ".ace",".iso",".img",".dmg",".pkg",".deb",".rpm",".apk",".aab",
    ".jar",".war",".ear",".cpio",".z",".lzip",".sit",".sitx",".bin",

    /* Video */
    ".mp4",".m4v",".mkv",".avi",".mov",".qt",".wmv",".asf",".webm",
    ".flv",".f4v",".f4p",".f4a",".f4b",".mpeg",".mpg",".mpe",".m2v",
    ".m2ts",".mts",".ts",".vob",".ogv",".3gp",".3g2",".rmvb",".rm",
    ".divx",".xvid",".mxf",".m2p",".dv",".mod",".tod",".m1v",".mjpeg",

    /* Applications */
    ".apk",".aab",".exe",".msi",".msp",".app",".dmg",".pkg",".deb",".rpm",
    ".flatpak",".snap",".appimage",".jar",".war",".ear",".bin",".com",
    ".dll",".so",".dylib",".a",".lib",".o",".obj",".class",".dex",
    ".ipa",".xapk",".apks",".xpi",".crx",

    /* Scripts / source */
    ".c",".h",".cc",".cpp",".cxx",".hpp",".hh",".java",".kt",".kts",
    ".swift",".m",".mm",".rs",".go",".zig",".dart",".py",".pyw",".pyc",
    ".pyo",".pyi",".js",".jsx",".mjs",".cjs",".ts",".tsx",".rb",".php",
    ".pl",".pm",".lua",".r",".R",".sh",".bash",".zsh",".fish",".ksh",
    ".bat",".cmd",".ps1",".psm1",".vbs",".vbe",".wsf",".sql",".asm",
    ".s",".pas",".pp",".f",".f90",".f95",".f03",".f08",".cs",".fs",
    ".fsx",".vb",".v",".sv",".svh",".vhd",".vhdl",".clj",".cljs",".groovy",
    ".scala",".ex",".exs",".erl",".hrl",".hs",".lhs",".elm",".jl",".nim",
    ".cr",".sol",".move",".asm",".make",".mk",".cmake",".gradle",".toml"
};

#define ALL_EXTENSION_COUNT \
    (sizeof(ALL_EXTENSIONS) / sizeof(ALL_EXTENSIONS[0]))


static const char *PHOTO_EXTENSIONS[] = {
    ".jpg",".jpeg",".jpe",".jfif",".png",".gif",".bmp",".dib",".webp",
    ".tif",".tiff",".heic",".heif",".avif",".ico",".icns",".raw",".cr2",
    ".cr3",".nef",".nrw",".arw",".orf",".rw2",".raf",".dng",".pef",
    ".srw",".x3f",".3fr",".iiq",".psd",".xcf",".ai",".eps",".svg",
    ".svgz",".apng",".jp2",".j2k",".jpf",".jpx",".jpm",".mj2",".dds",
    ".tga",".pcx",".ppm",".pgm",".pbm",".pnm",".hdr",".exr",".qoi",
    ".kra",".ora",".clip",".ase",".aseprite"
};

static const char *AUDIO_EXTENSIONS[] = {
    ".mp3",".wav",".wave",".flac",".m4a",".aac",".ogg",".oga",".opus",
    ".wma",".aiff",".aif",".aifc",".mid",".midi",".amr",".awb",".ape",
    ".alac",".ac3",".dts",".mka",".mpc",".spx",".ra",".rm",".voc",
    ".au",".snd",".caf",".dsf",".dff",".wv",".tta",".tak",".ofr",".ofs"
};

static const char *DOCUMENT_EXTENSIONS[] = {
    ".txt",".text",".md",".markdown",".rst",".rtf",".csv",".tsv",".log",
    ".xml",".json",".yaml",".yml",".ini",".cfg",".conf",".properties",
    ".pdf",".doc",".docx",".docm",".dot",".dotx",".dotm",".xls",".xlsx",
    ".xlsm",".xlsb",".xlt",".xltx",".ppt",".pptx",".pptm",".pps",".ppsx",
    ".odt",".ott",".ods",".ots",".odp",".otp",".odg",".odf",".epub",
    ".mobi",".azw",".azw3",".azw4",".fb2",".djvu",".djv",".tex",".bib",
    ".pages",".numbers",".key",".wpd",".wps",".msg",".eml",".ics",".vcf",
    ".html",".htm",".xhtml",".mht",".mhtml",".srt",".vtt",".ass",".ssa"
};

static const char *ARCHIVE_EXTENSIONS[] = {
    ".zip",".zipx",".rar",".7z",".tar",".gz",".tgz",".bz",".bz2",".tbz",
    ".tbz2",".xz",".txz",".zst",".lz",".lz4",".lzh",".lha",".cab",".arj",
    ".ace",".iso",".img",".dmg",".pkg",".deb",".rpm",".apk",".aab",
    ".jar",".war",".ear",".cpio",".z",".lzip",".sit",".sitx",".bin"
};

static const char *VIDEO_EXTENSIONS[] = {
    ".mp4",".m4v",".mkv",".avi",".mov",".qt",".wmv",".asf",".webm",
    ".flv",".f4v",".f4p",".f4a",".f4b",".mpeg",".mpg",".mpe",".m2v",
    ".m2ts",".mts",".ts",".vob",".ogv",".3gp",".3g2",".rmvb",".rm",
    ".divx",".xvid",".mxf",".m2p",".dv",".mod",".tod",".m1v",".mjpeg"
};

static const char *APPLICATION_EXTENSIONS[] = {
    ".apk",".aab",".exe",".msi",".msp",".app",".dmg",".pkg",".deb",".rpm",
    ".flatpak",".snap",".appimage",".jar",".war",".ear",".bin",".com",
    ".dll",".so",".dylib",".a",".lib",".o",".obj",".class",".dex",
    ".ipa",".xapk",".apks",".xpi",".crx"
};

static const char *SCRIPT_EXTENSIONS[] = {
    ".c",".h",".cc",".cpp",".cxx",".hpp",".hh",".java",".kt",".kts",
    ".swift",".m",".mm",".rs",".go",".zig",".dart",".py",".pyw",".pyc",
    ".pyo",".pyi",".js",".jsx",".mjs",".cjs",".ts",".tsx",".rb",".php",
    ".pl",".pm",".lua",".r",".R",".sh",".bash",".zsh",".fish",".ksh",
    ".bat",".cmd",".ps1",".psm1",".vbs",".vbe",".wsf",".sql",".asm",
    ".s",".pas",".pp",".f",".f90",".f95",".f03",".f08",".cs",".fs",
    ".fsx",".vb",".v",".sv",".svh",".vhd",".vhdl",".clj",".cljs",".groovy",
    ".scala",".ex",".exs",".erl",".hrl",".hs",".lhs",".elm",".jl",".nim",
    ".cr",".sol",".move",".make",".mk",".cmake",".gradle",".toml"
};

#define PHOTO_COUNT \
    (sizeof(PHOTO_EXTENSIONS) / sizeof(PHOTO_EXTENSIONS[0]))

#define AUDIO_COUNT \
    (sizeof(AUDIO_EXTENSIONS) / sizeof(AUDIO_EXTENSIONS[0]))

#define DOCUMENT_COUNT \
    (sizeof(DOCUMENT_EXTENSIONS) / sizeof(DOCUMENT_EXTENSIONS[0]))

#define ARCHIVE_COUNT \
    (sizeof(ARCHIVE_EXTENSIONS) / sizeof(ARCHIVE_EXTENSIONS[0]))

#define VIDEO_COUNT \
    (sizeof(VIDEO_EXTENSIONS) / sizeof(VIDEO_EXTENSIONS[0]))

#define APPLICATION_COUNT \
    (sizeof(APPLICATION_EXTENSIONS) / sizeof(APPLICATION_EXTENSIONS[0]))

#define SCRIPT_COUNT \
    (sizeof(SCRIPT_EXTENSIONS) / sizeof(SCRIPT_EXTENSIONS[0]))


static const SearchCategory CATEGORIES[] = {
    {1, "ALL FILES", "all"},
    {2, "PHOTOS / IMAGES", "photo"},
    {3, "AUDIO / MUSIC", "audio"},
    {4, "DOCUMENTS", "document"},
    {5, "ARCHIVES", "archive"},
    {6, "VIDEO", "video"},
    {7, "APPLICATIONS", "application"},
    {8, "SCRIPTS / PROGRAMS", "script"},
    {9, "RESERVED", "reserved"},
    {10, "RESERVED", "reserved"}
};

static int selected_category = 1;
static int total_matches = 0;


/* ============================================================
 * FUNCTION PROTOTYPES
 *
 * IMPORTANT:
 * print_highlighted_result() is declared here because
 * search_directory_recursive() calls it before its definition.
 * ============================================================ */

static void handle_sigint(int sig);

static int should_skip_directory(const char *path);

static void print_scan_summary(void);

static void lowercase_copy(
    const char *src,
    char *dst,
    size_t size
);

void auto_discover_storage(
    StoragePaths *paths
);

int tokenize_query(
    const char *query,
    QueryConfig *q_cfg
);

int execute_fzf_v1_match(
    const char *text,
    const QueryConfig *q_cfg,
    char *marker_mask
);

void print_highlighted_result(
    const char *text,
    const char *marker_mask,
    time_t modification_time
);

void search_directory_recursive(
    const char *dir_path,
    const QueryConfig *q_cfg
);

void print_category_menu(void);

int read_category_toggle(void);

int extension_matches_category(
    const char *filename,
    int category
);

int has_extension(
    const char *filename,
    const char *ext
);

const char *get_extension(
    const char *filename
);

int extension_in_list(
    const char *ext,
    const char *list[],
    size_t count
);


/* ============================================================
 * SIGNAL HANDLER
 * ============================================================ */

static void handle_sigint(int sig)
{
    (void)sig;

    /*
     * Do not printf() from a signal handler.
     * Just tell the search loop to stop.
     */
    keep_running = 0;
}


/* ============================================================
 * LINUX SYSTEM DIRECTORY FILTER
 * ============================================================ */

static int should_skip_directory(const char *path)
{
    /*
     * These directories are virtual/dynamic Linux filesystems.
     * Recursively searching them can make a root search appear
     * frozen and can expose constantly changing kernel entries.
     */

    if (strcmp(path, "/proc") == 0)
        return 1;

    if (strcmp(path, "/sys") == 0)
        return 1;

    if (strcmp(path, "/dev") == 0)
        return 1;

    if (strcmp(path, "/run") == 0)
        return 1;

    return 0;
}


/* ============================================================
 * SCAN SUMMARY
 * ============================================================ */

static void print_scan_summary(void)
{
    printf("\n");

    printf(
        COLOR_INFO
        "========== SCAN SUMMARY =========="
        COLOR_RESET
        "\n"
    );

    printf(
        "Entries examined : %llu\n",
        scanned_entries
    );

    printf(
        "Files examined   : %llu\n",
        scanned_files
    );

    printf(
        "Directories      : %llu\n",
        scanned_directories
    );

    printf(
        "Skipped dirs     : %llu\n",
        skipped_directories
    );

    printf(
        "Skipped symlinks : %llu\n",
        skipped_symlinks
    );

    printf(
        "Metadata errors  : %llu\n",
        stat_errors
    );

    printf(
        "Matches          : %d\n",
        total_matches
    );

    printf(
        COLOR_INFO
        "=================================="
        COLOR_RESET
        "\n"
    );

    fflush(stdout);
}


/* ============================================================
 * STRING HELPERS
 * ============================================================ */

static void lowercase_copy(
    const char *src,
    char *dst,
    size_t size
)
{
    size_t i;

    if (size == 0)
        return;

    for (i = 0;
         i + 1 < size && src[i];
         ++i) {

        dst[i] =
            (char)tolower(
                (unsigned char)src[i]
            );
    }

    dst[i] = '\0';
}


/* ============================================================
 * EXTENSION HANDLING
 * ============================================================ */

const char *get_extension(const char *filename)
{
    const char *base =
        strrchr(filename, '/');

    const char *dot =
        strrchr(filename, '.');

    if (!dot ||
        (base && dot < base) ||
        dot == filename ||
        dot[1] == '\0') {

        return NULL;
    }

    return dot;
}


int has_extension(
    const char *filename,
    const char *ext
)
{
    const char *actual =
        get_extension(filename);

    char a[32];
    char b[32];

    if (!actual)
        return 0;

    lowercase_copy(
        actual,
        a,
        sizeof(a)
    );

    lowercase_copy(
        ext,
        b,
        sizeof(b)
    );

    return strcmp(a, b) == 0;
}


int extension_in_list(
    const char *ext,
    const char *list[],
    size_t count
)
{
    char lower_ext[32];

    if (!ext)
        return 0;

    lowercase_copy(
        ext,
        lower_ext,
        sizeof(lower_ext)
    );

    for (size_t i = 0;
         i < count;
         ++i) {

        char lower_list[32];

        lowercase_copy(
            list[i],
            lower_list,
            sizeof(lower_list)
        );

        if (strcmp(
                lower_ext,
                lower_list
            ) == 0) {

            return 1;
        }
    }

    return 0;
}


int extension_matches_category(
    const char *filename,
    int category
)
{
    const char *ext =
        get_extension(filename);

    if (!ext)
        return 0;

    switch (category) {

        case 1:
            return extension_in_list(
                ext,
                ALL_EXTENSIONS,
                ALL_EXTENSION_COUNT
            );

        case 2:
            return extension_in_list(
                ext,
                PHOTO_EXTENSIONS,
                PHOTO_COUNT
            );

        case 3:
            return extension_in_list(
                ext,
                AUDIO_EXTENSIONS,
                AUDIO_COUNT
            );

        case 4:
            return extension_in_list(
                ext,
                DOCUMENT_EXTENSIONS,
                DOCUMENT_COUNT
            );

        case 5:
            return extension_in_list(
                ext,
                ARCHIVE_EXTENSIONS,
                ARCHIVE_COUNT
            );

        case 6:
            return extension_in_list(
                ext,
                VIDEO_EXTENSIONS,
                VIDEO_COUNT
            );

        case 7:
            return extension_in_list(
                ext,
                APPLICATION_EXTENSIONS,
                APPLICATION_COUNT
            );

        case 8:
            return extension_in_list(
                ext,
                SCRIPT_EXTENSIONS,
                SCRIPT_COUNT
            );

        default:
            return 0;
    }
}


/* ============================================================
 * CATEGORY MENU
 * ============================================================ */

void print_category_menu(void)
{
    printf(
        "\n"
        COLOR_INFO
        "Search category toggles:"
        COLOR_RESET
        "\n"
    );

    printf("  1  ALL FILES\n");
    printf("  2  PHOTOS / IMAGES\n");
    printf("  3  AUDIO / MUSIC\n");
    printf("  4  DOCUMENTS\n");
    printf("  5  ARCHIVES\n");
    printf("  6  VIDEO\n");
    printf("  7  APPLICATIONS\n");
    printf("  8  SCRIPTS / PROGRAMS\n");
    printf("  9  RESERVED\n");
    printf("  0  RESERVED\n");

    printf(
        "Current: "
        COLOR_INFO "%s"
        COLOR_RESET
        "\n",
        CATEGORIES[
            selected_category - 1
        ].name
    );
}


int read_category_toggle(void)
{
    struct termios oldt;
    struct termios newt;

    unsigned char ch = 0;

    if (tcgetattr(
            STDIN_FILENO,
            &oldt
        ) != 0) {

        return 0;
    }

    newt = oldt;

    newt.c_lflag &=
        (tcflag_t)~(ICANON | ECHO);

    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;

    if (tcsetattr(
            STDIN_FILENO,
            TCSANOW,
            &newt
        ) != 0) {

        return 0;
    }

    printf(
        "\n"
        "Press 1 through 8 to select a category.\n"
    );

    printf(
        "Press Enter when ready to enter "
        "the search query.\n"
    );

    fflush(stdout);

    while (read(
               STDIN_FILENO,
               &ch,
               1
           ) == 1) {

        int category = 0;

        switch (ch) {

            case '1':
                category = 1;
                break;

            case '2':
                category = 2;
                break;

            case '3':
                category = 3;
                break;

            case '4':
                category = 4;
                break;

            case '5':
                category = 5;
                break;

            case '6':
                category = 6;
                break;

            case '7':
                category = 7;
                break;

            case '8':
                category = 8;
                break;

            case '9':
                printf(
                    "\n9 is reserved.\n"
                );

                fflush(stdout);
                continue;

            case '0':
                printf(
                    "\n0 is reserved.\n"
                );

                fflush(stdout);
                continue;

            case '\n':
            case '\r':

                tcsetattr(
                    STDIN_FILENO,
                    TCSANOW,
                    &oldt
                );

                return selected_category;

            default:
                continue;
        }

        selected_category = category;

        printf(
            "\nSelected: "
            COLOR_INFO "%s"
            COLOR_RESET
            "\n",
            CATEGORIES[
                selected_category - 1
            ].name
        );

        printf(
            "Press Enter to search, "
            "or 1..8 to change category.\n"
        );

        fflush(stdout);
    }

    tcsetattr(
        STDIN_FILENO,
        TCSANOW,
        &oldt
    );

    return selected_category;
}


/* ============================================================
 * LINUX ROOT DISCOVERY
 * ============================================================ */

void auto_discover_storage(
    StoragePaths *paths
)
{
    memset(
        paths,
        0,
        sizeof(*paths)
    );

    /*
     * Linux root filesystem.
     */
    strncpy(
        paths->internal,
        "/",
        MAX_PATH_LEN - 1
    );

    paths->internal[
        MAX_PATH_LEN - 1
    ] = '\0';

    /*
     * There is no Android-style external SD-card
     * discovery in the Linux version.
     */
    paths->has_external = 0;
}


/* ============================================================
 * QUERY TOKENIZATION
 * ============================================================ */

int tokenize_query(
    const char *query,
    QueryConfig *q_cfg
)
{
    q_cfg->count = 0;

    char temp[256];

    strncpy(
        temp,
        query,
        sizeof(temp) - 1
    );

    temp[
        sizeof(temp) - 1
    ] = '\0';

    /*
     * Accept spaces, tabs and newlines.
     */
    char *token =
        strtok(
            temp,
            " \t\r\n"
        );

    while (
        token != NULL &&
        q_cfg->count < MAX_TOKENS
    ) {

        strncpy(
            q_cfg->tokens[
                q_cfg->count
            ],
            token,
            MAX_TOKEN_LEN - 1
        );

        q_cfg->tokens[
            q_cfg->count
        ][MAX_TOKEN_LEN - 1] = '\0';

        q_cfg->count++;

        token =
            strtok(
                NULL,
                " \t\r\n"
            );
    }

    return q_cfg->count;
}


/* ============================================================
 * MATCHING
 * ============================================================ */

int execute_fzf_v1_match(
    const char *text,
    const QueryConfig *q_cfg,
    char *marker_mask
)
{
    int text_len =
        (int)strlen(text);

    if (text_len <= 0 ||
        text_len >= MAX_PATH_LEN) {

        return 0;
    }

    memset(
        marker_mask,
        0,
        (size_t)text_len
    );

    char *lower_text =
        malloc(
            (size_t)text_len + 1
        );

    if (!lower_text)
        return 0;

    for (int i = 0;
         i < text_len;
         i++) {

        lower_text[i] =
            (char)tolower(
                (unsigned char)text[i]
            );
    }

    lower_text[text_len] = '\0';

    /*
     * Every token must occur somewhere
     * in the path.
     */
    for (int t = 0;
         t < q_cfg->count;
         t++) {

        char lower_tok[MAX_TOKEN_LEN];

        int tok_len =
            (int)strlen(
                q_cfg->tokens[t]
            );

        if (tok_len <= 0)
            continue;

        if (tok_len >= MAX_TOKEN_LEN) {

            free(lower_text);
            return 0;
        }

        for (int i = 0;
             i < tok_len;
             i++) {

            lower_tok[i] =
                (char)tolower(
                    (unsigned char)
                    q_cfg->tokens[t][i]
                );
        }

        lower_tok[tok_len] = '\0';

        char *match_ptr =
            strstr(
                lower_text,
                lower_tok
            );

        if (!match_ptr) {

            free(lower_text);
            return 0;
        }

        /*
         * Mark every occurrence.
         */
        while (match_ptr) {

            int start_idx =
                (int)(
                    match_ptr -
                    lower_text
                );

            for (int m = 0;
                 m < tok_len;
                 m++) {

                if (
                    start_idx + m <
                    text_len
                ) {

                    marker_mask[
                        start_idx + m
                    ] = 1;
                }
            }

            match_ptr =
                strstr(
                    lower_text +
                    start_idx + 1,
                    lower_tok
                );
        }
    }

    free(lower_text);

    return 1;
}


/* ============================================================
 * RECURSIVE ROOT SEARCH
 * ============================================================ */

void search_directory_recursive(
    const char *dir_path,
    const QueryConfig *q_cfg
)
{
    if (!keep_running)
        return;

    /*
     * Never enter Linux virtual filesystems.
     */
    if (should_skip_directory(
            dir_path
        )) {

        skipped_directories++;
        return;
    }

    DIR *dir =
        opendir(dir_path);

    /*
     * Permission denied and inaccessible
     * directories are simply skipped.
     */
    if (!dir) {

        skipped_directories++;
        return;
    }

    struct dirent *entry;

    char mask[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];

    while (
        keep_running &&
        (entry = readdir(dir)) != NULL
    ) {

        /*
         * Ignore . and ..
         */
        if (
            strcmp(
                entry->d_name,
                "."
            ) == 0 ||

            strcmp(
                entry->d_name,
                ".."
            ) == 0
        ) {

            continue;
        }

        scanned_entries++;

        /*
         * Protect the path buffer.
         */
        size_t dir_len =
            strlen(dir_path);

        size_t name_len =
            strlen(entry->d_name);

        if (
            dir_len + 1 + name_len >=
            sizeof(full_path)
        ) {

            stat_errors++;
            continue;
        }

        snprintf(
            full_path,
            sizeof(full_path),
            "%s/%s",
            dir_path,
            entry->d_name
        );

        struct stat statbuf;

        /*
         * IMPORTANT:
         *
         * lstat(), not stat().
         *
         * This prevents symbolic links from being
         * followed into other filesystem locations
         * or back into recursive paths.
         */
        if (
            lstat(
                full_path,
                &statbuf
            ) != 0
        ) {

            stat_errors++;
            continue;
        }

        /*
         * Never follow symbolic links.
         */
        if (
            S_ISLNK(
                statbuf.st_mode
            )
        ) {

            skipped_symlinks++;
            continue;
        }

        /*
         * Regular file.
         */
        if (
            S_ISREG(
                statbuf.st_mode
            )
        ) {

            scanned_files++;

            if (
                extension_matches_category(
                    full_path,
                    selected_category
                )
            ) {

                if (
                    execute_fzf_v1_match(
                        full_path,
                        q_cfg,
                        mask
                    )
                ) {

                    /*
                     * IMPORTANT:
                     *
                     * No carriage return.
                     * No ANSI line clearing.
                     * No cursor movement.
                     *
                     * Each result gets its own
                     * permanent terminal line.
                     */
                    print_highlighted_result(
                        full_path,
                        mask,
                        statbuf.st_mtime
                    );

                    fflush(stdout);

                    total_matches++;
                }
            }

            continue;
        }

        /*
         * Directory.
         */
        if (
            S_ISDIR(
                statbuf.st_mode
            )
        ) {

            scanned_directories++;

            /*
             * Skip /proc, /sys, /dev and /run.
             */
            if (
                should_skip_directory(
                    full_path
                )
            ) {

                skipped_directories++;
                continue;
            }

            search_directory_recursive(
                full_path,
                q_cfg
            );
        }
    }

    closedir(dir);
}


/* ============================================================
 * RESULT DISPLAY
 * ============================================================ */

void print_highlighted_result(
    const char *text,
    const char *marker_mask,
    time_t modification_time
)
{
    char date_buffer[64];

    struct tm time_info;

    struct tm *local =
        localtime(
            &modification_time
        );

    if (local) {

        time_info = *local;

        if (
            strftime(
                date_buffer,
                sizeof(date_buffer),
                "%Y-%m-%d %H:%M:%S",
                &time_info
            ) == 0
        ) {

            strcpy(
                date_buffer,
                "unknown date"
            );
        }

    } else {

        strcpy(
            date_buffer,
            "unknown date"
        );
    }

    /*
     * Modification date.
     */
    printf(
        COLOR_DATE
        "[%s]"
        COLOR_RESET
        " ",
        date_buffer
    );

    int len =
        (int)strlen(text);

    int inside_highlight = 0;

    for (int i = 0;
         i < len;
         i++) {

        if (
            marker_mask[i] &&
            !inside_highlight
        ) {

            printf(COLOR_MATCH);

            inside_highlight = 1;

        } else if (
            !marker_mask[i] &&
            inside_highlight
        ) {

            printf(COLOR_RESET);

            inside_highlight = 0;
        }

        putchar(text[i]);
    }

    if (inside_highlight)
        printf(COLOR_RESET);

    /*
     * Every result permanently occupies
     * its own terminal line.
     */
    printf("\n");
}


/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    signal(
        SIGINT,
        handle_sigint
    );

    while (keep_running) {

        StoragePaths paths;

        auto_discover_storage(
            &paths
        );

        printf(
            COLOR_INFO
            "--- Linux Root Filesystem Search ---"
            COLOR_RESET
            "\n"
        );

        printf(
            "Search Root : "
            COLOR_PATH
            "%s"
            COLOR_RESET
            "\n",
            paths.internal
        );

        printf(
            "Excluded    : "
            "/proc /sys /dev /run\n"
        );

        printf(
            "Symlinks    : "
            "not followed\n"
        );

        printf(
            "Results     : "
            "permanently printed\n"
        );

        printf(
            "--------------------------------------\n"
        );

        print_category_menu();

        /*
         * Interactive category selector.
         */
        if (
            isatty(STDIN_FILENO)
        ) {

            read_category_toggle();
        }

        char query_buffer[256];

        printf(
            "\nEnter multi-word fuzzy search query: "
        );

        fflush(stdout);

        if (
            !fgets(
                query_buffer,
                sizeof(query_buffer),
                stdin
            )
        ) {

            break;
        }

        query_buffer[
            strcspn(
                query_buffer,
                "\n"
            )
        ] = '\0';

        QueryConfig q_cfg;

        if (
            !tokenize_query(
                query_buffer,
                &q_cfg
            )
        ) {

            printf(
                "Empty or invalid query string.\n"
            );

            continue;
        }

        /*
         * Reset scan statistics.
         */
        scanned_entries = 0;
        scanned_files = 0;
        scanned_directories = 0;
        skipped_directories = 0;
        skipped_symlinks = 0;
        stat_errors = 0;
        total_matches = 0;

        keep_running = 1;

        printf(
            "\n"
            COLOR_INFO
            "Category: %s"
            COLOR_RESET
            "\n",
            CATEGORIES[
                selected_category - 1
            ].name
        );

        printf(
            COLOR_INFO
            "Scanning Linux root recursively."
            COLOR_RESET
            "\n"
        );

        printf(
            "Excluded system trees: "
            "/proc /sys /dev /run\n"
        );

        printf(
            "Symbolic links will not be followed.\n"
        );

        printf(
            "Every matching result will remain visible.\n"
        );

        printf(
            "Press Ctrl+C to stop the scan.\n\n"
        );

        fflush(stdout);

        /*
         * Search from Linux root.
         */
        search_directory_recursive(
            paths.internal,
            &q_cfg
        );

        if (!keep_running) {

            printf(
                "\n\n"
                COLOR_INFO
                "Scan interrupted by Ctrl+C."
                COLOR_RESET
                "\n"
            );
        }

        print_scan_summary();

        /*
         * Ctrl+C terminates the program instead of
         * immediately beginning another search.
         */
        if (!keep_running)
            break;

        printf("\n");
    }

    printf(
        COLOR_INFO
        "Search program terminated."
        COLOR_RESET
        "\n"
    );

    return 0;
}