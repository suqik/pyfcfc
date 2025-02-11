/*******************************************************************************
* 2pt_box/load_conf.c: this file is part of the FCFC program.

* FCFC: Fast Correlation Function Calculator.

* Github repository:
        https://github.com/cheng-zhao/FCFC

* Copyright (c) 2020 -- 2022 Cheng Zhao <zhaocheng03@gmail.com>  [MIT license]

*******************************************************************************/

#include "define.h"
#include "load_conf.h"
#include "libcfg.h"
#include "libast.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

/*============================================================================*\
                           Macros for error handling
\*============================================================================*/
/* Check existence of configuration parameters. */
#define CHECK_EXIST_PARAM(name, cfg, var)                       \
  if (!cfg_is_set((cfg), (var))) {                              \
    P_ERR(FMT_KEY(name) " is not set\n");                       \
    return FCFC_ERR_CFG;                                        \
  }
#define CHECK_EXIST_ARRAY(name, cfg, var, num)                  \
  if (!(num = cfg_get_size((cfg), (var)))) {                    \
    P_ERR(FMT_KEY(name) " is not set\n");                       \
    return FCFC_ERR_CFG;                                        \
  }

/* Check length of array. */
#define CHECK_ARRAY_LENGTH(name, cfg, var, fmt, num, nexp)      \
  if (num < (nexp)) {                                           \
    P_ERR("too few elements of " FMT_KEY(name) "\n");           \
    return FCFC_ERR_CFG;                                        \
  }                                                             \
  if (num > (nexp)) {                                           \
    P_WRN("omitting the following " FMT_KEY(name) ":");         \
    for (int i = (nexp); i < num; i++)                          \
      fprintf(stderr, " " fmt, (var)[i]);                       \
    fprintf(stderr, "\n");                                      \
  }

#define CHECK_STR_ARRAY_LENGTH(name, cfg, var, num, nexp)       \
  if (num < (nexp)) {                                           \
    P_ERR("too few elements of " FMT_KEY(name) "\n");           \
    return FCFC_ERR_CFG;                                        \
  }                                                             \
  if (num > (nexp)) {                                           \
    P_WRN("omitting the following " FMT_KEY(name) ":\n");       \
    for (int i = (nexp); i < num; i++)                          \
      fprintf(stderr, "  %s\n", (var)[i]);                      \
  }

/* Release memory for configuration parameters. */
#define FREE_ARRAY(x)           {if(x) free(x);}
#define FREE_STR_ARRAY(x)       {if(x) {if (*(x)) free(*(x)); free(x);}}

/* Print the warning and error messages. */
#define P_CFG_WRN(cfg)  cfg_pwarn(cfg, stderr, FMT_WARN);
#define P_CFG_ERR(cfg)  {                                       \
  cfg_perror(cfg, stderr, FMT_ERR);                             \
  cfg_destroy(cfg);                                             \
  return NULL;                                                  \
}


/*============================================================================*\
                    Functions called via command line flags
\*============================================================================*/

/******************************************************************************
Function `usage`:
  Print the usage of command line options.
******************************************************************************/
static void usage(void *args) {
  (void) args;
  printf(FCFC_LOGO "\nUsage: " FCFC_CODE_NAME " [OPTION]\n\
Compute the 2-point correlation functions of catalogs in periodic boxes.\n\
  -h, --help\n\
        Display this message and exit\n\
  -V, --version\n\
        Display the version information\n\
  -t, --template\n\
        Print a template configuration file to the standard output and exit\n\
  -c, --conf            " FMT_KEY(CONFIG_FILE) "     String\n\
        Specify the configuration file (default: `%s')\n\
  -i, --input           " FMT_KEY(CATALOG) "         String array\n\
        Specify the input catalogs\n\
  -l, --label           " FMT_KEY(CATALOG_LABEL) "   Character array\n\
        Specify the labels of the input catalogs\n\
  -T, --type            " FMT_KEY(CATALOG_TYPE) "    Integer array\n\
        Type (format) of the input catalogs\n\
      --skip            " FMT_KEY(ASCII_SKIP) "      Long integer array\n\
        Numbers of lines to be skipped for the ASCII format input catalogs\n\
      --comment         " FMT_KEY(ASCII_COMMENT) "   Character array\n\
        Comment symbols for the ASCII format input catalogs\n\
  -f, --formatter       " FMT_KEY(ASCII_FORMATTER) " String array\n\
        Formatters for columns of ASCII format input catalogs\n\
  -x, --position        " FMT_KEY(POSITION) "        String array\n\
        Column indicator or expression for the 3-D positions of the inputs\n\
  -w, --weight          " FMT_KEY(WEIGHT) "          String array\n\
        Column indicator or expression for weights of the inputs\n\
  -s, --select          " FMT_KEY(SELECTION) "       String array\n\
        Expressions for sample selection criteria\n\
  -b, --box             " FMT_KEY(BOX_SIZE) "        Double array\n\
        Side lengths of the periodic box for distance evaluations\n\
  -S, --data-struct     " FMT_KEY(DATA_STRUCT) "     Integer\n\
        Specify the data structure for pair counting\n\
  -B, --bin             " FMT_KEY(BINNING_SCHEME) "  Integer\n\
        Specify the binning scheme of the correlation functions\n\
  -p, --pair            " FMT_KEY(PAIR_COUNT) "      String array\n\
        Specify pairs to be counted or read, using the catalog labels\n\
  -P, --pair-output     " FMT_KEY(PAIR_COUNT_FILE) " String array\n\
        Specify the output files for pair counts\n\
  -e, --cf              " FMT_KEY(CF_ESTIMATOR) "    String array\n\
        Expressions for correlation function estimators based on pair counts\n\
  -E, --cf-output       " FMT_KEY(CF_OUTPUT_FILE) "  String array\n\
        Specify the output files for correlation functions\n\
  -m, --multipole       " FMT_KEY(MULTIPOLE) "       Integer array\n\
        Orders of Legendre multipoles of correlation functions to be evaluated\n\
  -M, --mp-output       " FMT_KEY(MULTIPOLE_FILE) "  String array\n\
        Specify the output files for correlation function multipoles\n\
  -u, --wp              " FMT_KEY(PROJECTED_CF) "    Boolean\n\
        Indicate whether to compute the projected correlation functions\n\
  -U, --wp-output       " FMT_KEY(PROJECTED_FILE) "  String array\n\
        Specify the output files for projected correlation functions\n\
      --s-file          " FMT_KEY(SEP_BIN_FILE) "    String\n\
        Specify the file defining edges of separation (or s_perp) bins\n\
      --s-min           " FMT_KEY(SEP_BIN_MIN) "     Double\n\
        Specify the lower limit of linear separation (or s_perp) bins\n\
      --s-max           " FMT_KEY(SEP_BIN_MAX) "     Double\n\
        Specify the upper limit of linear separation (or s_perp) bins\n\
      --s-step          " FMT_KEY(SEP_BIN_SIZE) "    Double\n\
        Specify the width of linear separation (or s_perp) bins\n\
      --mu-num          " FMT_KEY(MU_BIN_NUM) "      Integer\n\
        Specify the number of linear mu bins in the range [0,1"
#ifdef WITH_MU_ONE
      "]"
#else
      ")"
#endif
"\n\
      --pi-file         " FMT_KEY(PI_BIN_FILE) "     String\n\
        Specify the file defining edges of pi (a.k.a. s_para) bins\n\
      --pi-min          " FMT_KEY(PI_BIN_MIN) "      Double\n\
        Specify the lower limit of linear pi bins\n\
      --pi-max          " FMT_KEY(PI_BIN_MAX) "      Double\n\
        Specify the upper limit of linear pi bins\n\
      --pi-step         " FMT_KEY(PI_BIN_SIZE) "     Double\n\
        Specify the width of linear pi bins\n\
  -F, --out-format      " FMT_KEY(OUTPUT_FORMAT) "   Integer\n\
        Format of the output pair count files\n\
  -O, --overwrite       " FMT_KEY(OVERWRITE) "       Integer\n\
        Indicate whether to overwrite existing output files\n\
  -v, --verbose         " FMT_KEY(VERBOSE) "         Boolean\n\
        Indicate whether to display detailed standard outputs\n\
Consult the -t option for more information on the parameters\n\
Github repository: https://github.com/cheng-zhao/FCFC\n\
Licence: MIT\n",
      DEFAULT_CONF_FILE);
  exit(0);
}

/******************************************************************************
Function `version`:
  Print the version information.
******************************************************************************/
static void version(void *args) {
  (void) args;
  printf(FCFC_LOGO "\n\x1B[35C\x1B[33;1mv" FCFC_VERSION "\n\x1B[32C"
      FCFC_CODE_NAME "\x1B[0m\n"
      "\n- Parallelization schemes\n"
      "  * MPI: "
#ifdef MPI
      "enabled\n"
#else
      "disabled (enable with -DMPI)\n"
#endif
      "  * OpenMP: "
#ifdef OMP
      "enabled\n"
#else
      "disabled (enable with -DOMP)\n"
#endif
      "  * SIMD: "
#if             FCFC_SIMD  ==  FCFC_SIMD_NONE
      "disabled (enable with -DWITH_SIMD)"
#elif           FCFC_SIMD  ==  FCFC_SIMD_AVX
      "AVX"
  #ifdef        FCFC_SIMD_FMA
      " + FMA"
  #endif
#elif           FCFC_SIMD  ==  FCFC_SIMD_AVX2
      "AVX2"
  #ifdef        FCFC_SIMD_FMA
      " + FMA"
  #endif
#else        /* FCFC_SIMD  ==  FCFC_SIMD_AVX512 */
      "AVX-512F"
  #ifdef        FCFC_SIMD_AVX512DQ
      " + AVX-512DQ"
  #endif
#endif
      "\n- Compilation options\n"
      "  * Floating-point precision: "
#ifdef SINGLE_PREC
      "single (-DSINGLE_PREC enabled)\n"
#else
      "double (-DSINGLE_PREC disabled)\n"
#endif
      "  * (s,mu) pairs with mu = 1: "
#ifdef WITH_MU_ONE
      "included (-DWITH_MU_ONE enabled)\n"
#else
      "excluded (-DWITH_MU_ONE disabled)\n"
#endif
      "- External libraries\n"
      "  * CFITSIO: "
#ifdef WITH_CFITSIO
      "enabled\n"
#else
      "disabled (enable with -DWITH_CFITSIO)\n"
#endif
      "  * HDF5: "
#ifdef WITH_HDF5
      "enabled\n"
#else
      "disabled (enable with -DWITH_HDF5)\n"
#endif
      "\n\
- Copyright (c) 2020 -- 2022 Cheng ZHAO.\n\
- Github repository: https://github.com/cheng-zhao/FCFC\n\
- Licence: MIT\n");
  exit(0);
}

/******************************************************************************
Function `conf_template`:
  Print a template configuration file.
******************************************************************************/
void conf_template(void *args) {
  (void) args;
  printf("# Configuration file for " FCFC_CODE_NAME " (default: `"
DEFAULT_CONF_FILE "').\n\
# Format: keyword = value # comment\n\
#     or: keyword = [element1, element2]\n\
#    see: https://github.com/cheng-zhao/libcfg for details.\n\
# Some of the entries allow expressions, see\n\
#         https://github.com/cheng-zhao/libast for details.\n\
# NOTE that command line options have priority over this file.\n\
# Unnecessary entries can be left unset.\n\
\n\
##########################################\n\
#  Specifications of the input catalogs  #\n\
##########################################\n\
\n\
CATALOG         = \n\
    # Filename of the input catalogs, string or string array.\n\
CATALOG_LABEL   = \n\
    # Label of the input catalogs, must be non-repetitive uppercase letters.\n\
    # Character, same dimension as `CATALOG`.\n\
    # If unset, catalogs are labelled in alphabetical order, i.e. [A,B,...].\n\
CATALOG_TYPE    = \n\
    # File format of the input catalogs (unset: %d).\n\
    # Integer, same dimension as `CATALOG`.\n\
    # Allowed values are:\n\
    # * %d: ASCII text file"
#ifdef WITH_CFITSIO
    ";\n    # * %d: FITS table"
#endif
#ifdef WITH_HDF5
    ";\n    # * %d: HDF5 file"
#endif
".\nASCII_SKIP      = \n\
    # Number of lines to be skipped for ASCII catalogs (unset: %ld).\n\
    # Long integer, same dimension as `CATALOG`.\n\
ASCII_COMMENT   = \n\
    # Character indicating comment lines for ASCII catalogs (unset: '%c%s.\n\
    # Character, same dimension as `CATALOG`.\n\
    # Empty character ('') for disabling comments.\n\
ASCII_FORMATTER = \n\
    # C99-style formatter for parsing lines of ASCII catalogs.\n\
    # String, same dimension as `DATA_CATALOG` (e.g. \"%%d %%ld %%f %%lf %%s\").\n\
    # If a column is suppressed by *, it is not counted for the column number.\n\
    # E.g., for \"%%d %%*s %%f\", the float number corresponds to column %c2.\n\
    # See https://en.cppreference.com/w/c/io/fscanf for details on the format.\n\
POSITION        = \n\
    # 3-D comoving coordinates, in the order of {x,y,z}.\n\
    # String array, 3 times the length of `CATALOG`.\n\
    # They can be column indicator or expressions, e.g.,\n\
    #     \"(%c1 * %c%c10%c) %% 100\" / \"%c%cRA%c\" / \"%c%cgroup/dataset%c2%c%c\"\n\
    # Allowed values enclosed by %c%c%c:\n\
    # * long integer: column number of an ASCII file (starting from 1);\n\
    # * string: column name of a FITS file;\n\
    # * string%cinteger%c: dataset name and column index (starting from 1)\n\
    #                    of an HDF5 file.\n\
WEIGHT          = \n\
    # Weights for pair counts (unset: 1, i.e. no weight).\n\
    # Column indicator or expression, same dimension as `DATA_CATALOG`.\n\
SELECTION       = \n\
    # Selection criteria for the catalogs (unset: no selection).\n\
    # Logical expression, same dimension as `CATALOG` (e.g. \"%c3 > 0.5\").\n\
BOX_SIZE        = \n\
    # Side lengths of the periodic box for the input catalogs.\n\
    # Double-precision number (for cubic box) or 3-element double array.\n\
\n\
################################################################\n\
#  Configurations for the 2-point correlation function (2PCF)  #\n\
################################################################\n\
\n\
DATA_STRUCT     = \n\
    # Data structure for evaluating pair counts, integer (unset: %d).\n\
    # Allowed values are:\n\
    # * %d: k-d tree;\n\
    # * %d: ball tree.\n\
BINNING_SCHEME  = \n\
    # Binning scheme of the 2PCFs, integer (unset: %d).\n\
    # Allowed values are:\n\
    # * %d: isotropic separation bins;\n\
    # * %d: (s, mu) bins (required by 2PCF multipoles);\n\
    # * %d: (s_perp, pi) bins (required by projected 2PCFs);\n\
PAIR_COUNT      = \n\
    # Identifiers of pairs to be counted or read, string or string array.\n\
    # Pairs are labelled by their source catalogs.\n\
    # E.g., \"DD\" denotes auto pairs from the catalog 'D',\n\
    # while \"DR\" denotes cross pairs from catalogs 'D' and 'R'.\n\
PAIR_COUNT_FILE = \n\
    # Name of the files for storing pair counts.\n\
    # String, same dimension as `PAIR_COUNT`.\n\
    # Depending on `OVERWRITE`, pair counts can be read from existing files.\n\
CF_ESTIMATOR    = \n\
    # Estimator of the 2PCFs to be evaluated, string or string array.\n\
    # It must be an expression with pair identifiers.\n\
    # In particular, \"%c%c\" denotes the analytical RR pair counts.\n\
CF_OUTPUT_FILE  = \n\
    # Name of the files for saving 2PCFs with the desired binning scheme.\n\
    # String, same dimension as `CF_ESTIMATOR`.\n\
MULTIPOLE       = \n\
    # Orders of Legendre multipoles to be evaluated, integer or integer array.\n\
MULTIPOLE_FILE  = \n\
    # Name of the files for saving 2PCF multipoles.\n\
    # String, same dimension as `CF_ESTIMATOR`.\n\
PROJECTED_CF    = \n\
    # Boolean option, indicate whether computing the projected 2PCFs (unset: %c).\n\
PROJECTED_FILE  = \n\
    # Name of the files for saving projected 2PCFs.\n\
    # String, same dimension as `CF_ESTIMATOR`.\n\
\n\
#############################\n\
#  Definitions of the bins  #\n\
#############################\n\
\n\
SEP_BIN_FILE    = \n\
    # Filename of the table defining edges of separation (or s_perp) bins.\n\
    # It mush be a text file with the first two columns being\n\
    # the lower and upper limits of the distance bins, respectively.\n\
    # Lines starting with '%c' are omitted.\n\
SEP_BIN_MIN     = \n\
SEP_BIN_MAX     = \n\
SEP_BIN_SIZE    = \n\
    # Lower and upper limits, and width of linear separation (or s_perp) bins.\n\
    # Double-precision numbers. They are only used if `SEP_BIN_FILE` is unset.\n\
MU_BIN_NUM      = \n\
    # Number of linear mu bins in the range [0,1"
#ifdef WITH_MU_ONE
    "]"
#else
    ")"
#endif
", integer.\n\
PI_BIN_FILE     = \n\
    # Filename of the table defining edges of pi (a.k.a. s_para) bins.\n\
    # Lines starting with '%c' are omitted.\n\
PI_BIN_MIN      = \n\
PI_BIN_MAX      = \n\
PI_BIN_SIZE     = \n\
    # Lower and upper limits, and width of linear pi bins.\n\
    # Double-precision numbers. They are only used if `PI_BIN_FILE` is unset.\n\
\n\
####################\n\
#  Other settings  #\n\
####################\n\
\n\
OUTPUT_FORMAT   = \n\
    # Format of the output `PAIR_COUNT_FILE`, integer (unset: %d).\n\
    # Allowed values are:\n\
    # * %d: FCFC binary format;\n\
    # * %d: ASCII text format.\n\
OVERWRITE       = \n\
    # Flag indicating whether to overwrite existing files, integer (unset: %d).\n\
    # Allowed values are:\n\
    # * %d: quit the program when an output file exist;\n\
    # * %d: overwrite 2PCF files silently, but keep existing pair count files;\n\
    # * %d or larger: overwrite all files silently;\n\
    # * negative: notify for decisions, and the maximum allowed number of failed\n\
    #             trials are given by the absolute value of this number.\n\
VERBOSE         = \n\
    # Boolean option, indicate whether to show detailed outputs (unset: %c).\n",
      DEFAULT_FILE_TYPE, FCFC_FFMT_ASCII,
#ifdef WITH_CFITSIO
      FCFC_FFMT_FITS,
#endif
#ifdef WITH_HDF5
      FCFC_FFMT_HDF5,
#endif
      (long) DEFAULT_ASCII_SKIP,
      DEFAULT_ASCII_COMMENT ? DEFAULT_ASCII_COMMENT : '\'',
      DEFAULT_ASCII_COMMENT ? "')" : ")",
      AST_VAR_FLAG, AST_VAR_FLAG, AST_VAR_FLAG, AST_VAR_START, AST_VAR_END,
      AST_VAR_FLAG, AST_VAR_START, AST_VAR_END,
      AST_VAR_FLAG, AST_VAR_START, FCFC_COL_IDX_START, FCFC_COL_IDX_END,
      AST_VAR_END, AST_VAR_FLAG, AST_VAR_START, AST_VAR_END,
      FCFC_COL_IDX_START, FCFC_COL_IDX_END, AST_VAR_FLAG,
      DEFAULT_STRUCT, FCFC_STRUCT_KDTREE, FCFC_STRUCT_BALLTREE,
      DEFAULT_BINNING, FCFC_BIN_ISO,
      FCFC_BIN_SMU, FCFC_BIN_SPI, FCFC_SYM_ANA_RR, FCFC_SYM_ANA_RR,
      DEFAULT_PROJECTED_CF ? 'T' : 'F', FCFC_READ_COMMENT, FCFC_READ_COMMENT,
      DEFAULT_OUTPUT_FORMAT, FCFC_OFMT_BIN, FCFC_OFMT_ASCII,
      DEFAULT_OVERWRITE, FCFC_OVERWRITE_NONE, FCFC_OVERWRITE_CFONLY,
      FCFC_OVERWRITE_ALL, DEFAULT_VERBOSE ? 'T' : 'F');
  exit(0);
}


/*============================================================================*\
                      Function for reading configurations
\*============================================================================*/

/******************************************************************************
Function `conf_init`:
  Initialise the structure for storing configurations.
Return:
  Address of the structure.
******************************************************************************/
static CONF *conf_init(void) {
  CONF *conf = calloc(1, sizeof *conf);
  if (!conf) return NULL;
  conf->fconf = conf->label = conf->comment = conf->fsbin = conf->fpbin = NULL;
  conf->input = conf->fmtr = conf->pos = conf->wt = conf->sel = NULL;
  conf->pc = conf->pcout = conf->cf = conf->cfout = NULL;
  conf->mpout = conf->wpout = NULL;
  conf->ftype = conf->poles = NULL;
  conf->bsize = NULL;
  conf->has_wt = conf->comp_pc = NULL;
  conf->skip = NULL;
  return conf;
}

/******************************************************************************
Function `conf_read`:
  Read configurations.
Arguments:
  * `conf`:     structure for storing configurations;
  * `argc`:     number of arguments passed via command line;
  * `argv`:     array of command line arguments.
Return:
  Interface of libcfg.
******************************************************************************/
static cfg_t *conf_read(CONF *conf, const int argc, char *const *argv) {
  if (!conf) {
    P_ERR("the structure for configurations is not initialised\n");
    return NULL;
  }
  cfg_t *cfg = cfg_init();
  if (!cfg) P_CFG_ERR(cfg);

  /* Functions to be called via command line flags. */
  const cfg_func_t funcs[] = {
    {'h', "help"        , usage            ,            NULL},
    {'V', "version"     , version          ,            NULL},
    {'t', "template"    , conf_template    ,            NULL}
  };

  /* Configuration parameters. */
  const cfg_param_t params[] = {
    {'c', "conf"        , "CONFIG_FILE"    , CFG_DTYPE_STR , &conf->fconf   },
    {'l', "label"       , "CATALOG_LABEL"  , CFG_ARRAY_CHAR, &conf->label   },
    {'w', "weight"      , "WEIGHT"         , CFG_ARRAY_STR , &conf->wt      },
    {'b', "box"         , "BOX_SIZE"       , CFG_ARRAY_DBL , &conf->bsize   },
    {'S', "data-struct" , "DATA_STRUCT"    , CFG_DTYPE_INT , &conf->dstruct },
    {'B', "bin"         , "BINNING_SCHEME" , CFG_DTYPE_INT , &conf->bintype },
    {'p', "pair"        , "PAIR_COUNT"     , CFG_ARRAY_STR , &conf->pc      },
    {'P', "pair-output" , "PAIR_COUNT_FILE", CFG_ARRAY_STR , &conf->pcout   },
    {'e', "cf"          , "CF_ESTIMATOR"   , CFG_ARRAY_STR , &conf->cf      },
    {'E', "cf-output"   , "CF_OUTPUT_FILE" , CFG_ARRAY_STR , &conf->cfout   },
    {'m', "multipole"   , "MULTIPOLE"      , CFG_ARRAY_INT , &conf->poles   },
    {'M', "mp-output"   , "MULTIPOLE_FILE" , CFG_ARRAY_STR , &conf->mpout   },
    {'u', "wp"          , "PROJECTED_CF"   , CFG_DTYPE_BOOL, &conf->wp      },
    {'U', "wp-output"   , "PROJECTED_FILE" , CFG_ARRAY_STR , &conf->wpout   },
    {'F', "out-format"  , "OUTPUT_FORMAT"  , CFG_DTYPE_INT , &conf->ofmt    },
    {'O', "overwrite"   , "OVERWRITE"      , CFG_DTYPE_INT , &conf->ovwrite },
    {'v', "verbose"     , "VERBOSE"        , CFG_DTYPE_BOOL, &conf->verbose }
  };

  /* Register functions and parameters. */
  if (cfg_set_funcs(cfg, funcs, sizeof(funcs) / sizeof(funcs[0])))
      P_CFG_ERR(cfg);
  P_CFG_WRN(cfg);
  if (cfg_set_params(cfg, params, sizeof(params) / sizeof(params[0])))
      P_CFG_ERR(cfg);
  P_CFG_WRN(cfg);

  /* Read configurations from command line options. */
  int optidx;
  if (cfg_read_opts(cfg, argc, argv, FCFC_PRIOR_CMD, &optidx))
    P_CFG_ERR(cfg);
  P_CFG_WRN(cfg);

  /* Read parameters from configuration file. */
  if (!cfg_is_set(cfg, &conf->fconf)) conf->fconf = DEFAULT_CONF_FILE;
  if (access(conf->fconf, R_OK))
    P_WRN("cannot access the configuration file: `%s'\n", conf->fconf);
  else if (cfg_read_file(cfg, conf->fconf, FCFC_PRIOR_FILE)) P_CFG_ERR(cfg);
  P_CFG_WRN(cfg);

  return cfg;
}


/*============================================================================*\
                      Functions for parameter verification
\*============================================================================*/

/******************************************************************************
Function `check_input`:
  Check whether an input file can be read.
Arguments:
  * `fname`:    filename of the input file;
  * `key`:      keyword of the input file.
Return:
  Zero on success; non-zero on error.
******************************************************************************/
static inline int check_input(const char *fname, const char *key) {
  if (!fname || *fname == '\0') {
    P_ERR("the input " FMT_KEY(%s) " is not set\n", key);
    return FCFC_ERR_CFG;
  }
  if (access(fname, R_OK)) {
    P_ERR("cannot access " FMT_KEY(%s) ": `%s'\n", key, fname);
    return FCFC_ERR_FILE;
  }
  return 0;
}

/******************************************************************************
Function `check_output`:
  Check whether an output file can be written.
Arguments:
  * `fname`:    filename of the input file;
  * `key`:      keyword of the input file;
  * `ovwrite`:  option for overwriting exisiting files;
  * `force:     flag for overwriting files without notification.
Return:
  Zero on success; non-zero on error.
******************************************************************************/
static int check_output(char *fname, const char *key, int ovwrite,
    const int force) {
  if (!fname || *fname == '\0') {
    P_ERR("the output file " FMT_KEY(%s) " is not set\n", key);
    return FCFC_ERR_CFG;
  }

  /* Check if the file exists. */
  if (!access(fname, F_OK)) {
    if (ovwrite < 0) {                          /* ask for decision */
      P_WRN("the output file " FMT_KEY(%s) " exists: `%s'\n", key, fname);
      char confirm = 0;
      for (int i = 0; i != ovwrite; i--) {
        fprintf(stderr, "Are you going to overwrite it? (y/n): ");
        if (scanf("%c", &confirm) != 1) continue;
        int c;
        while((c = getchar()) != '\n' && c != EOF) continue;
        if (confirm == 'n' || confirm == 'N') {
          ovwrite = force - 1;
          break;
        }
        else if (confirm == 'y' || confirm == 'Y') {
          ovwrite = force;
          break;
        }
      }
      if (confirm != 'y' && confirm != 'Y' &&
          confirm != 'n' && confirm != 'N') {
        P_ERR("too many failed inputs\n");
        return FCFC_ERR_FILE;
      }
    }

    if (ovwrite <= FCFC_OVERWRITE_NONE) {       /* not overwriting */
      P_ERR("abort to avoid overwriting " FMT_KEY(%s) ": `%s'\n", key, fname);
      return FCFC_ERR_FILE;
    }
    else if (ovwrite >= force) {                /* force overwriting */
      P_WRN(FMT_KEY(%s) " will be overwritten: `%s'\n", key, fname);
    }
    else {                                      /* this is an input file */
      if (access(fname, R_OK)) {
        P_ERR("cannot access " FMT_KEY(%s) ": `%s'\n", key, fname);
        return FCFC_ERR_FILE;
      }
      return FCFC_ERR_SAVE;     /* indicate that the file will be read */
    }

    /* Check file permission for overwriting. */
    if (access(fname, W_OK)) {
      P_ERR("cannot write to file: `%s'\n", fname);
      return FCFC_ERR_FILE;
    }
  }
  /* Check the path permission. */
  else {
    char *end;
    if ((end = strrchr(fname, FCFC_PATH_SEP)) != NULL) {
      *end = '\0';
      if (access(fname, X_OK)) {
        P_ERR("cannot access the directory `%s'\n", fname);
        return FCFC_ERR_FILE;
      }
      *end = FCFC_PATH_SEP;
    }
  }
  return 0;
}

/******************************************************************************
Function `conf_verify`:
  Verify configuration parameters.
Arguments:
  * `cfg`:      interface of libcfg;
  * `conf`:     structure for storing configurations.
Return:
  Zero on success; non-zero on error.
******************************************************************************/
static int conf_verify(const cfg_t *cfg, CONF *conf) {
  int e, num;
  
  /* CATALOG_LABEL */
  num = cfg_get_size(cfg, &conf->label);
  conf->ninput = cfg_get_size(cfg, &conf->label);
  if (!num) {
    char *tmp = realloc(conf->label, conf->ninput * sizeof *tmp);
    if (!tmp) {
      P_ERR("failed to allocate memory for " FMT_KEY(CATALOG_LABEL) "\n");
      return FCFC_ERR_MEMORY;
    }
    conf->label = tmp;
    for (int i = 0; i < conf->ninput; i++) conf->label[i] = 'A' + i;
  }
  else {
    CHECK_ARRAY_LENGTH(CATALOG_LABEL, cfg, conf->label, "%c", num,
        conf->ninput);
    for (int i = 0; i < conf->ninput; i++) {
      if (conf->label[i] < 'A' || conf->label[i] > 'Z') {
        P_ERR("invalid " FMT_KEY(CATALOG_LABEL) ": %c\n", conf->label[i]);
        return FCFC_ERR_CFG;
      }
    }
    /* Check duplicates. */
    for (int i = 0; i < conf->ninput - 1; i++) {
      for (int j = i + 1; j < conf->ninput; j++) {
        if (conf->label[i] == conf->label[j]) {
          P_ERR("duplicate " FMT_KEY(CATALOG_LABEL) ": %c\n", conf->label[i]);
          return FCFC_ERR_CFG;
        }
      }
    }
  }

  /* WEIGHT */
  if (!(conf->has_wt = malloc(conf->ninput * sizeof(bool)))) {
    P_ERR("failed to allocate memory for " FMT_KEY(WEIGHT) "\n");
    return FCFC_ERR_MEMORY;
  }
  // We will manage weighting defaults from p/cython 
  for (int i = 0; i < conf->ninput; i++) conf->has_wt[i] = true;

  /* BOX_SIZE */
  CHECK_EXIST_ARRAY(BOX_SIZE, cfg, &conf->bsize, num);
  if (num == 1) {
    double *tmp = realloc(conf->bsize, 3 * sizeof *tmp);
    if (!tmp) {
      P_ERR("failed to allocate memory for " FMT_KEY(BOX_SIZE) "\n");
      return FCFC_ERR_MEMORY;
    }
    conf->bsize = tmp;
    conf->bsize[1] = conf->bsize[2] = conf->bsize[0];
  }
  else {
    CHECK_ARRAY_LENGTH(BOX_SIZE, cfg, conf->bsize, OFMT_DBL, num, 3);
  }
  for (int i = 0; i < 3; i++) {
    if (!isfinite(conf->bsize[i]) || conf->bsize[i] <= 0) {
      P_ERR(FMT_KEY(BOX_SIZE) " must be finite and positive\n");
      return FCFC_ERR_CFG;
    }
  }

  /* OVERWRITE */
  if (!cfg_is_set(cfg, &conf->ovwrite)) conf->ovwrite = DEFAULT_OVERWRITE;

  /* DATA_STRUCT */
  if (!cfg_is_set(cfg, &conf->dstruct)) conf->dstruct = DEFAULT_STRUCT;
  switch (conf->dstruct) {
    case FCFC_STRUCT_KDTREE:
    case FCFC_STRUCT_BALLTREE:
      break;
    default:
      P_ERR("invalid " FMT_KEY(DATA_STRUCT) ": %d\n", conf->dstruct);
      return FCFC_ERR_CFG;
  }

  /* BINNING_SCHEME */
  if (!cfg_is_set(cfg, &conf->bintype)) conf->bintype = DEFAULT_BINNING;
  switch (conf->bintype) {
    case FCFC_BIN_ISO:
    case FCFC_BIN_SMU:
    case FCFC_BIN_SPI:
      break;
    default:
      P_ERR("invalid " FMT_KEY(BINNING_SCHEME) ": %d\n", conf->bintype);
      return FCFC_ERR_CFG;
  }

  /* PAIR_COUNT */
  CHECK_EXIST_ARRAY(PAIR_COUNT, cfg, &conf->pc, conf->npc);
  /* Simple validation. */
  for (int i = 0; i < conf->npc; i++) {
    char *s = conf->pc[i];
    if (s[0] < 'A' || s[0] > 'Z' || s[1] < 'A' || s[1] > 'Z' || s[2]) {
      P_ERR("invalid " FMT_KEY(PAIR_COUNT) ": %s\n", s);
      return FCFC_ERR_CFG;
    }
  }
  /* Check duplicates. */
  for (int i = 0; i < conf->npc - 1; i++) {
    for (int j = i + 1; j < conf->npc; j++) {
      if (conf->pc[i][0] == conf->pc[j][0] &&
          conf->pc[i][1] == conf->pc[j][1]) {
        P_ERR("duplicate " FMT_KEY(PAIR_COUNT) ": %s\n", conf->pc[i]);
        return FCFC_ERR_CFG;
      }
    }
  }

  if (!(conf->comp_pc = malloc(conf->npc * sizeof(bool)))) {
    P_ERR("failed to allocate memory for checking pair counts\n");
    return FCFC_ERR_MEMORY;
  }

  /* PAIR_COUNT_FILE */
  if (cfg_is_set(cfg, &conf->pcout)){
    CHECK_EXIST_ARRAY(PAIR_COUNT_FILE, cfg, &conf->pcout, num);
    CHECK_STR_ARRAY_LENGTH(PAIR_COUNT_FILE, cfg, conf->pcout, num, conf->npc);
    for (int i = 0; i < conf->npc; i++) {
      e = check_output(conf->pcout[i], "PAIR_COUNT_FILE", conf->ovwrite,
          FCFC_OVERWRITE_ALL);
      if (!e) conf->comp_pc[i] = true;
      else if (e == FCFC_ERR_SAVE) conf->comp_pc[i] = false;
      else return e;

      /* Check if the labels exist if evaluating pair counts. */
      if (conf->comp_pc[i]) {
        int label_found = 0;
        for (int j = 0; j < conf->ninput; j++) {
          if (conf->pc[i][0] == conf->label[j]) label_found += 1;
          if (conf->pc[i][1] == conf->label[j]) label_found += 1;
        }
        if (label_found != 2) {
          P_ERR("catalog label not found for " FMT_KEY(PAIR_COUNT) ": %s\n",
              conf->pc[i]);
          return FCFC_ERR_CFG;
        }
      }
    }
  }
  else{
    conf->pcout = NULL;
    for (int i = 0; i < conf->npc; i++) {
      conf->comp_pc[i] = true;
      /* Check if the labels exist if evaluating pair counts. */
      if (conf->comp_pc[i]) {
        int label_found = 0;
        for (int j = 0; j < conf->ninput; j++) {
          if (conf->pc[i][0] == conf->label[j]) label_found += 1;
          if (conf->pc[i][1] == conf->label[j]) label_found += 1;
        }
        if (label_found != 2) {
          P_ERR("catalog label not found for " FMT_KEY(PAIR_COUNT) ": %s\n",
              conf->pc[i]);
          return FCFC_ERR_CFG;
        }
      }
    }
  }

  /* CF_ESTIMATOR */
  if ((conf->ncf = cfg_get_size(cfg, &conf->cf))) {
    for (int i = 0; i < conf->ncf; i++) {
      if (!conf->cf[i] || !(*conf->cf[i])) {
        P_ERR("unexpected empty " FMT_KEY(CF_ESTIMATOR) "\n");
        return FCFC_ERR_CFG;
      }
    }
    /* CF_OUTPUT_FILE */ 
    if (cfg_is_set(cfg, &conf->cfout)){
      CHECK_EXIST_ARRAY(CF_OUTPUT_FILE, cfg, &conf->cfout, num);
      CHECK_STR_ARRAY_LENGTH(CF_OUTPUT_FILE, cfg, conf->cfout, num, conf->ncf);
      for (int i = 0; i < conf->ncf; i++) {
        if ((e = check_output(conf->cfout[i], "CF_OUTPUT_FILE", conf->ovwrite,
            FCFC_OVERWRITE_CFONLY))) return e;
      }
    }
    else conf->cfout = NULL;

    if (conf->bintype == FCFC_BIN_SMU) {
      /* MULTIPOLE */
      if ((conf->npole = cfg_get_size(cfg, &conf->poles))) {
        /* Sort multipoles and remove duplicates. */
        if (conf->npole > 1) {
          /* 5-line insertion sort from https://doi.org/10.1145/3812.315108 */
          for (int i = 1; i < conf->npole; i++) {
            int tmp = conf->poles[i];
            for (num = i; num > 0 && conf->poles[num - 1] > tmp; num--)
              conf->poles[num] = conf->poles[num - 1];
            conf->poles[num] = tmp;
          }
          /* Remove duplicates from the sorted array. */
          num = 0;
          for (int i = 1; i < conf->npole; i++) {
            if (conf->poles[i] != conf->poles[num]) {
              num++;
              conf->poles[num] = conf->poles[i];
            }
          }
          conf->npole = num + 1;
        }
        if (conf->poles[0] < 0 || conf->poles[conf->npole - 1] > FCFC_MAX_ELL) {
          P_ERR(FMT_KEY(MULTIPOLE) " must be between 0 and %d\n", FCFC_MAX_ELL);
          return FCFC_ERR_CFG;
        }

        /* MULTIPOLE_FILE */
        if (cfg_is_set(cfg, &conf->mpout)){
          CHECK_EXIST_ARRAY(MULTIPOLE_FILE, cfg, &conf->mpout, num);
          CHECK_STR_ARRAY_LENGTH(MULTIPOLE_FILE, cfg, conf->mpout,
              num, conf->ncf);
          for (int i = 0; i < conf->ncf; i++) {
            if ((e = check_output(conf->mpout[i], "MULTIPOLE_FILE",
                conf->ovwrite, FCFC_OVERWRITE_CFONLY))) return e;
          }
        }
        else conf->mpout = NULL;
      }
    }
    else if (conf->bintype == FCFC_BIN_SPI) {
      /* PROJECTED_CF */
      if (!cfg_is_set(cfg, &conf->wp)) conf->wp = DEFAULT_PROJECTED_CF;
      if (conf->wp) {
        /* PROJECTED_FILE */
        if (cfg_is_set(cfg, &conf->wpout)){
          CHECK_EXIST_ARRAY(PROJECTED_FILE, cfg, &conf->wpout, num);
          CHECK_STR_ARRAY_LENGTH(PROJECTED_FILE, cfg, conf->wpout,
              num, conf->ncf);
          for (int i = 0; i < conf->ncf; i++) {
            if ((e = check_output(conf->wpout[i], "PROJECTED_FILE",
                conf->ovwrite, FCFC_OVERWRITE_CFONLY))) return e;
          }
        }
        else conf->wpout = NULL;
      }
    }
  }

  /* SEP_BIN_FILE */
  
  /* OUTPUT_FORMAT */
  if (!cfg_is_set(cfg, &conf->ofmt)) conf->ofmt = DEFAULT_OUTPUT_FORMAT;
  switch (conf->ofmt) {
    case FCFC_OFMT_BIN:
    case FCFC_OFMT_ASCII:
      break;
    default:
      P_ERR("invalid " FMT_KEY(OUTPUT_FORMAT) ": %d\n", conf->ofmt);
      return FCFC_ERR_CFG;
  }

  /* VERBOSE */
  if (!cfg_is_set(cfg, &conf->verbose)) conf->verbose = DEFAULT_VERBOSE;

  return 0;
}


/*============================================================================*\
                      Function for printing configurations
\*============================================================================*/

/******************************************************************************
Function `conf_print`:
  Print configuration parameters.
Arguments:
  * `conf`:     structure for storing configurations;
  * `para`:     structure for parallelisms.
******************************************************************************/
static void conf_print(const CONF *conf
#ifdef WITH_PARA
    , const PARA *para
#endif
    ) {
  /* Configuration file */
  printf("\n  CONFIG_FILE     = %s", conf->fconf);

  /* Input catalogs. */
//  printf("\n  CATALOG         = %s", conf->input[0]);
//  for (int i = 1; i < conf->ninput; i++)
//    printf("\n                    %s", conf->input[i]);
  printf("\n  CATALOG_LABEL   = '%c'", conf->label[0]);
  for (int i = 1; i < conf->ninput; i++) printf(" , '%c'", conf->label[i]);

//  const char *ftype[] = {"ASCII", "FITS", "HDF5"};
//  const int ntype = sizeof(ftype) / sizeof(ftype[0]);
//  if (!conf->ftype) {
//    printf("\n  CATALOG_TYPE    = %d (%s)",
//        DEFAULT_FILE_TYPE,
//        DEFAULT_FILE_TYPE < ntype ? ftype[DEFAULT_FILE_TYPE] : "unknown");
//  }
//  else {
//    printf("\n  CATALOG_TYPE    = %d (%s)",
//        conf->ftype[0],
//        conf->ftype[0] < ntype ? ftype[conf->ftype[0]] : "unknown");
//    for (int i = 1; i < conf->ninput; i++) {
//      printf("\n                    %d (%s)",
//          conf->ftype[i],
//          conf->ftype[i] < ntype ? ftype[conf->ftype[i]] : "unknown");
//    }
//  }
//
//  if (conf->ascii) {
//    if (!conf->skip) {
//      printf("\n  ASCII_SKIP      = %ld", (long) DEFAULT_ASCII_SKIP);
//    }
//    else {
//      printf("\n  ASCII_SKIP      = %ld", conf->skip[0]);
//      for (int i = 1; i < conf->ninput; i++) printf(" , %ld", conf->skip[i]);
//    }
//
//    if (!conf->comment) {
//      if (DEFAULT_ASCII_COMMENT == 0) printf("\n  ASCII_COMMENT   = ''");
//      else printf("\n  ASCII_COMMENT   = '%c'", DEFAULT_ASCII_COMMENT);
//    }
//    else {
//      int type = conf->ftype ? conf->ftype[0] : DEFAULT_FILE_TYPE;
//      if (type != FCFC_FFMT_ASCII || conf->comment[0] == 0)
//        printf("\n  ASCII_COMMENT   = ''");
//      else printf("\n  ASCII_COMMENT   = '%c'", conf->comment[0]);
//      for (int i = 1; i < conf->ninput; i++) {
//        type = conf->ftype ? conf->ftype[i] : DEFAULT_FILE_TYPE;
//        if (type != FCFC_FFMT_ASCII || conf->comment[i] == 0) printf(" , ''");
//        else printf(" , '%c'", conf->comment[i]);
//      }
//    }
//
//    printf("\n  ASCII_FORMATTER = %s", conf->fmtr[0]);
//    for (int i = 1; i < conf->ninput; i++)
//      printf("\n                    %s", conf->fmtr[i]);
//  }

  //printf("\n  POSITION        = %s , %s , %s",
  //    conf->pos[0], conf->pos[1], conf->pos[2]);
  //
  //for (int i = 1; i < conf->ninput; i++) {
  //  printf("\n                    %s , %s , %s",
  //      conf->pos[i * 3], conf->pos[i * 3 + 1], conf->pos[i * 3 + 2]);
  //}

//  if (conf->wt) {
//    printf("\n  WEIGHT          = %s", conf->wt[0]);
//    for (int i = 1; i < conf->ninput; i++)
//      printf("\n                    %s", conf->wt[i]);
//  }
//
//  if (conf->sel) {
//    printf("\n  SELECTION       = %s", conf->sel[0]);
//    for (int i = 1; i < conf->ninput; i++)
//      printf("\n                    %s", conf->sel[i]);
//  }
  printf("\n  BOX_SIZE        = " OFMT_DBL " , " OFMT_DBL " , " OFMT_DBL,
      conf->bsize[0], conf->bsize[1], conf->bsize[2]);

  /* 2PCF configurations. */
  const char *tname[] = {"k-d tree", "ball tree"};
  const int ntname = sizeof(tname) / sizeof(tname[0]);
  printf("\n  DATA_STRUCT     = %d (%s)", conf->dstruct,
      conf->dstruct < ntname ? tname[conf->dstruct] : "unknown");

  const char *bname[] = {"s", "s & mu", "s_perp & pi"};
  const int nbname = sizeof(bname) / sizeof(bname[0]);
  printf("\n  BINNING_SCHEME  = %d (%s)", conf->bintype,
      conf->bintype < nbname ? bname[conf->bintype] : "unknown");
  printf("\n  PAIR_COUNT      = %s", conf->pc[0]);
  for (int i = 1; i < conf->npc; i++) printf(" , %s", conf->pc[i]);

  if (conf->pcout){
    printf("\n  PAIR_COUNT_FILE = <%c> %s",
        conf->comp_pc[0] ? 'W' : 'R', conf->pcout[0]);
    for (int i = 1; i < conf->npc; i++) {
      printf("\n                    <%c> %s",
          conf->comp_pc[i] ? 'W' : 'R', conf->pcout[i]);
    }
  }

  if (conf->ncf) {
    printf("\n  CF_ESTIMATOR    = %s", conf->cf[0]);
    for (int i = 1; i < conf->ncf; i++)
      printf("\n                    %s", conf->cf[i]);
    if (conf->cfout){  
      printf("\n  CF_OUTPUT_FILE  = %s", conf->cfout[0]);
      for (int i = 1; i < conf->ncf; i++)
        printf("\n                    %s", conf->cfout[i]);
    }

    if (conf->bintype == FCFC_BIN_SMU && conf->npole) {
      printf("\n  MULTIPOLE       = %d", conf->poles[0]);
      for (int i = 1; i < conf->npole; i++) printf(" , %d", conf->poles[i]);
      if (conf->mpout){  
        printf("\n  MULTIPOLE_FILE  = %s", conf->mpout[0]);
        for (int i = 1; i < conf->ncf; i++)
          printf("\n                    %s", conf->mpout[i]);
      }
    }

    if (conf->bintype == FCFC_BIN_SPI) {
      printf("\n  PROJECTED_CF    = %c", conf->wp ? 'T' : 'F');
      if (conf->wp) {
        if (conf->wpout){  
          printf("\n  PROJECTED_FILE  = %s", conf->wpout[0]);
          for (int i = 1; i < conf->ncf; i++)
            printf("\n                    %s", conf->wpout[i]);
        }
      }
    }
  }

  /* Bin definitions. */
  
  /* Others. */
  const char *sname[] = {"binary", "ASCII"};
  const int nsname = sizeof(sname) / sizeof(sname[0]);
  if (conf->wpout || conf->cfout || conf->pcout || conf->mpout){  
    printf("\n  OUTPUT_FORMAT   = %d (%s)", conf->ofmt,
        conf->ofmt < nsname ? sname[conf->ofmt] : "unknown");
    printf("\n  OVERWRITE       = %d", conf->ovwrite);
  }

#ifdef MPI
  printf("\n  MPI_NUM_TASKS   = %d", para->ntask);
#endif
#ifdef OMP
  printf("\n  OMP_NUM_THREADS = %d", para->nthread);
#endif
  printf("\n");
}


/*============================================================================*\
                      Interface for loading configurations
\*============================================================================*/

/******************************************************************************
Function `load_conf`:
  Read, check, and print configurations.
Arguments:
  * `argc`:     number of arguments passed via command line;
  * `argv`:     array of command line arguments;
  * `para`:     structure for parallelisms.
Return:
  The structure for storing configurations.
******************************************************************************/
CONF *load_conf(const int argc, char *const *argv
#ifdef WITH_PARA
    , const PARA *para
#endif
    ) {
  CONF *conf = conf_init();
  if (!conf) return NULL;

  cfg_t *cfg = conf_read(conf, argc, argv);
  if (!cfg) {
    conf_destroy(conf);
    return NULL;
  }

  printf("Loading configurations ...");
  fflush(stdout);

  if (conf_verify(cfg, conf)) {
    if (cfg_is_set(cfg, &conf->fconf)) free(conf->fconf);
    conf_destroy(conf);
    cfg_destroy(cfg);
    return NULL;
  }

  if (conf->verbose)
    conf_print(conf
#ifdef WITH_PARA
        , para
#endif
        );

  if (cfg_is_set(cfg, &conf->fconf)) free(conf->fconf);
  cfg_destroy(cfg);

  printf(FMT_DONE);
#ifdef MPI
  fflush(stdout);
#endif
  return conf;
}

/******************************************************************************
Function `conf_destroy`:
  Release memory allocated for the configurations.
Arguments:
  * `conf`:     the structure for storing configurations.
******************************************************************************/
void conf_destroy(CONF *conf) {
  if (!conf) return;
  FREE_ARRAY(conf->label);
  FREE_STR_ARRAY(conf->wt);
  FREE_ARRAY(conf->has_wt);
  FREE_ARRAY(conf->bsize);
  FREE_STR_ARRAY(conf->pc);
  FREE_ARRAY(conf->comp_pc);
  FREE_STR_ARRAY(conf->pcout);
  FREE_STR_ARRAY(conf->cf);
  FREE_STR_ARRAY(conf->cfout);
  FREE_ARRAY(conf->poles);
  FREE_STR_ARRAY(conf->mpout);
  FREE_STR_ARRAY(conf->wpout);
  free(conf);
}
