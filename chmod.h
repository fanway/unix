#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fts.h>
#include "stat-macros.h"
//#include "xalloc.h"



// static struct option const long_options[] =
// {
//   {"changes", no_argument, NULL, 'c'},
//   {"recursive", no_argument, NULL, 'R'},
//   {"no-preserve-root", no_argument, NULL, NO_PRESERVE_ROOT},
//   {"preserve-root", no_argument, NULL, PRESERVE_ROOT},
//   {"quiet", no_argument, NULL, 'f'},
//   {"reference", required_argument, NULL, REFERENCE_FILE_OPTION},
//   {"silent", no_argument, NULL, 'f'},
//   {"verbose", no_argument, NULL, 'v'},
//   {GETOPT_HELP_OPTION_DECL},
//   {GETOPT_VERSION_OPTION_DECL},
//   {NULL, 0, NULL, 0}
// };

#define SUID 04000
#define SGID 02000
#define SVTX 01000
#define RUSR 00400
#define WUSR 00200
#define XUSR 00100
#define RGRP 00040
#define WGRP 00020
#define XGRP 00010
#define ROTH 00004
#define WOTH 00002
#define XOTH 00001
#define ALLM 07777 /* all octal mode bits */

/* Use this to suppress gcc's '...may be used before initialized' warnings. */
#ifdef lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code) /* empty */
#endif

#define	FTS_DEFER_STAT 0x0400
#define FTS_CWDFD 0x0200

typedef int bool;
#define true 1
#define false 0

static mode_t umask_value; // mask

// FTS *
// xfts_open (char * const *argv, int options,
//            int (*compar) (const FTSENT **, const FTSENT **))
// {
//   FTS *fts = fts_open (argv, options, compar);
//   if (fts == NULL)
//     {
//       /* This can fail in two ways: out of memory or with errno==EINVAL,
//          which indicates it was called with invalid bit_flags.  */
//       xalloc_die ();
//     }

//   return fts;
// }

static struct mode_change *change;

struct mode_change
{
  char op;                      /* One of "=+-".  */
  char flag;                    /* Special operations flag.  */
  mode_t affected;              /* Set for u, g, o, or a.  */
  mode_t value;                 /* Bits to add/remove.  */
  mode_t mentioned;             /* Bits explicitly mentioned.  */
};

/* Special operations flags.  */
enum
  {
    /* For the sentinel at the end of the mode changes array.  */
    MODE_DONE,

    /* The typical case.  */
    MODE_ORDINARY_CHANGE,

    /* In addition to the typical case, affect the execute bits if at
       least one execute bit is set already, or if the file is a
       directory.  */
    MODE_X_IF_ANY_X,

    /* Instead of the typical case, copy some existing permissions for
       u, g, or o onto the other two.  Which of u, g, or o is copied
       is determined by which bits are set in the 'value' field.  */
    MODE_COPY_EXISTING
  };

static mode_t octal_to_mode (unsigned int octal);
mode_t mode_adjust (mode_t oldmode, bool dir, mode_t umask_value, struct mode_change const *changes, mode_t *pmode_bits);
static struct mode_change *make_node_op_equals (mode_t new_mode, mode_t mentioned);
struct mode_change * mode_compile (char const *mode_string);
static bool process_file (FTS *fts, FTSENT *ent);
static bool process_files (char **files, int bit_flags);
int shcmd_chmod(char* cmd, char* params[]);