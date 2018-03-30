#include "chmod.h"

static mode_t octal_to_mode (unsigned int octal)
{
  /* Help the compiler optimize the usual case where mode_t uses
     the traditional octal representation.  */
  return ((S_ISUID == SUID && S_ISGID == SGID && S_ISVTX == SVTX
           && S_IRUSR == RUSR && S_IWUSR == WUSR && S_IXUSR == XUSR
           && S_IRGRP == RGRP && S_IWGRP == WGRP && S_IXGRP == XGRP
           && S_IROTH == ROTH && S_IWOTH == WOTH && S_IXOTH == XOTH)
          ? octal
          : (mode_t) ((octal & SUID ? S_ISUID : 0)
                      | (octal & SGID ? S_ISGID : 0)
                      | (octal & SVTX ? S_ISVTX : 0)
                      | (octal & RUSR ? S_IRUSR : 0)
                      | (octal & WUSR ? S_IWUSR : 0)
                      | (octal & XUSR ? S_IXUSR : 0)
                      | (octal & RGRP ? S_IRGRP : 0)
                      | (octal & WGRP ? S_IWGRP : 0)
                      | (octal & XGRP ? S_IXGRP : 0)
                      | (octal & ROTH ? S_IROTH : 0)
                      | (octal & WOTH ? S_IWOTH : 0)
                      | (octal & XOTH ? S_IXOTH : 0)));
}


mode_t
mode_adjust (mode_t oldmode, bool dir, mode_t umask_value,
             struct mode_change const *changes, mode_t *pmode_bits)
{
  /* The adjusted mode.  */
  mode_t newmode = oldmode & CHMOD_MODE_BITS;

  /* File mode bits that CHANGES cares about.  */
  mode_t mode_bits = 0;

  for (; changes->flag != MODE_DONE; changes++)
    {
      mode_t affected = changes->affected;
      mode_t omit_change =
        (dir ? S_ISUID | S_ISGID : 0) & ~ changes->mentioned;
      mode_t value = changes->value;

      switch (changes->flag)
        {
        case MODE_ORDINARY_CHANGE:
          break;

        case MODE_COPY_EXISTING:
          /* Isolate in 'value' the bits in 'newmode' to copy.  */
          value &= newmode;

          /* Copy the isolated bits to the other two parts.  */
          value |= ((value & (S_IRUSR | S_IRGRP | S_IROTH)
                     ? S_IRUSR | S_IRGRP | S_IROTH : 0)
                    | (value & (S_IWUSR | S_IWGRP | S_IWOTH)
                       ? S_IWUSR | S_IWGRP | S_IWOTH : 0)
                    | (value & (S_IXUSR | S_IXGRP | S_IXOTH)
                       ? S_IXUSR | S_IXGRP | S_IXOTH : 0));
          break;

        case MODE_X_IF_ANY_X:
          /* Affect the execute bits if execute bits are already set
             or if the file is a directory.  */
          if ((newmode & (S_IXUSR | S_IXGRP | S_IXOTH)) | dir)
            value |= S_IXUSR | S_IXGRP | S_IXOTH;
          break;
        }

      /* If WHO was specified, limit the change to the affected bits.
         Otherwise, apply the umask.  Either way, omit changes as
         requested.  */
      value &= (affected ? affected : ~umask_value) & ~ omit_change;

      switch (changes->op)
        {
        case '=':
          /* If WHO was specified, preserve the previous values of
             bits that are not affected by this change operation.
             Otherwise, clear all the bits.  */
          {
            mode_t preserved = (affected ? ~affected : 0) | omit_change;
            mode_bits |= CHMOD_MODE_BITS & ~preserved;
            newmode = (newmode & preserved) | value;
            break;
          }

        case '+':
          mode_bits |= value;
          newmode |= value;
          break;

        case '-':
          mode_bits |= value;
          newmode &= ~value;
          break;
        }
    }

  if (pmode_bits)
    *pmode_bits = mode_bits;
  return newmode;
}

static struct mode_change *
make_node_op_equals (mode_t new_mode, mode_t mentioned)
{
  struct mode_change *p = malloc (2 * sizeof *p);
  p->op = '=';
  p->flag = MODE_ORDINARY_CHANGE;
  p->affected = CHMOD_MODE_BITS;
  p->value = new_mode;
  p->mentioned = mentioned;
  p[1].flag = MODE_DONE;
  return p;
}

struct mode_change * mode_compile (char const *mode_string)
{
  /* The array of mode-change directives to be returned.  */
  struct mode_change *mc;
  size_t used = 0;
  char const *p;

  if ('0' <= *mode_string && *mode_string < '8')
    {
      unsigned int octal_mode = 0;
      mode_t mode;
      mode_t mentioned;

      p = mode_string;
      do
        {
          octal_mode = 8 * octal_mode + *p++ - '0';
          if (ALLM < octal_mode)
            return NULL;
        }
      while ('0' <= *p && *p < '8');

      if (*p)
        return NULL;

      mode = octal_to_mode (octal_mode);
      mentioned = (p - mode_string < 5
                   ? (mode & (S_ISUID | S_ISGID)) | S_ISVTX | S_IRWXUGO
                   : CHMOD_MODE_BITS);
      return make_node_op_equals (mode, mentioned);
    }

  /* Allocate enough space to hold the result.  */
  {
    size_t needed = 1;
    for (p = mode_string; *p; p++)
      needed += (*p == '=' || *p == '+' || *p == '-');
    mc = malloc (needed * sizeof *mc);
  }

  /* One loop iteration for each
     '[ugoa]*([-+=]([rwxXst]*|[ugo]))+|[-+=][0-7]+'.  */
  for (p = mode_string; ; p++)
    {
      /* Which bits in the mode are operated on.  */
      mode_t affected = 0;

      /* Turn on all the bits in 'affected' for each group given.  */
      for (;; p++)
        switch (*p)
          {
          default:
            goto invalid;
          case 'u':
            affected |= S_ISUID | S_IRWXU;
            break;
          case 'g':
            affected |= S_ISGID | S_IRWXG;
            break;
          case 'o':
            affected |= S_ISVTX | S_IRWXO;
            break;
          case 'a':
            affected |= CHMOD_MODE_BITS;
            break;
          case '=': case '+': case '-':
            goto no_more_affected;
          }
    no_more_affected:;

      do
        {
          char op = *p++;
          mode_t value;
          mode_t mentioned = 0;
          char flag = MODE_COPY_EXISTING;
          struct mode_change *change;

          switch (*p)
            {
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
              {
                unsigned int octal_mode = 0;

                do
                  {
                    octal_mode = 8 * octal_mode + *p++ - '0';
                    if (ALLM < octal_mode)
                      goto invalid;
                  }
                while ('0' <= *p && *p < '8');

                if (affected || (*p && *p != ','))
                  goto invalid;
                affected = mentioned = CHMOD_MODE_BITS;
                value = octal_to_mode (octal_mode);
                flag = MODE_ORDINARY_CHANGE;
                break;
              }

            case 'u':
              /* Set the affected bits to the value of the "u" bits
                 on the same file.  */
              value = S_IRWXU;
              p++;
              break;
            case 'g':
              /* Set the affected bits to the value of the "g" bits
                 on the same file.  */
              value = S_IRWXG;
              p++;
              break;
            case 'o':
              /* Set the affected bits to the value of the "o" bits
                 on the same file.  */
              value = S_IRWXO;
              p++;
              break;

            default:
              value = 0;
              flag = MODE_ORDINARY_CHANGE;

              for (;; p++)
                switch (*p)
                  {
                  case 'r':
                    value |= S_IRUSR | S_IRGRP | S_IROTH;
                    break;
                  case 'w':
                    value |= S_IWUSR | S_IWGRP | S_IWOTH;
                    break;
                  case 'x':
                    value |= S_IXUSR | S_IXGRP | S_IXOTH;
                    break;
                  case 'X':
                    flag = MODE_X_IF_ANY_X;
                    break;
                  case 's':
                    /* Set the setuid/gid bits if 'u' or 'g' is selected.  */
                    value |= S_ISUID | S_ISGID;
                    break;
                  case 't':
                    /* Set the "save text image" bit if 'o' is selected.  */
                    value |= S_ISVTX;
                    break;
                  default:
                    goto no_more_values;
                  }
            no_more_values:;
            }

          change = &mc[used++];
          change->op = op;
          change->flag = flag;
          change->affected = affected;
          change->value = value;
          change->mentioned =
            (mentioned ? mentioned : affected ? affected & value : value);
        }
      while (*p == '=' || *p == '+' || *p == '-');

      if (*p != ',')
        break;
    }
    if (*p == 0)
    {
      mc[used].flag = MODE_DONE;
      return mc;
    }

invalid:
  free (mc);
  return NULL;
}

static bool
process_file (FTS *fts, FTSENT *ent)
{
    char const *file_full_name = ent->fts_path;
    char const *file = ent->fts_accpath;
    const struct stat *file_stats = ent->fts_statp;
    mode_t old_mode IF_LINT ( = 0);
    mode_t new_mode IF_LINT ( = 0);
    bool ok = true;
    bool chmod_succeeded = false;
    old_mode = file_stats->st_mode;
    new_mode = mode_adjust (old_mode, S_ISDIR (old_mode) != 0, umask_value,
                              change, NULL);
    chmod(file, new_mode);
}


static bool
process_files (char **files, int bit_flags)
{
  FTS *fts = fts_open (files, bit_flags, NULL);

    while (1)
    {
        FTSENT *ent;
        ent = fts_read (fts);
        if (ent == NULL)
        {
          break;
        }
        process_file (fts, ent);
    }
}

int shcmd_chmod(char* cmd, char* params[]) {
    char *mode = NULL;
    size_t mode_len = 0;
    size_t mode_alloc = 0;
    int c = 1;
    int params_count = 0;
    while (params[params_count] != NULL)
        params_count++;
    while ((c = getopt (params_count, params,
                            ("Rcfvr::w::x::X::s::t::u::g::o::a::,::+::=::"
                                "0::1::2::3::4::5::6::7::")))
            != -1)
    {
        switch (c)
            {
            case 'r':
            case 'w':
            case 'x':
            case 'X':
            case 's':
            case 't':
            case 'u':
            case 'g':
            case 'o':
            case 'a':
            case ',':
            case '+':
            case '=':
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
            /* Support nonportable uses like "chmod -w", but diagnose
                surprises due to umask confusion.  Even though "--", "--r",
                etc., are valid modes, there is no "case '-'" here since
                getopt_long reserves leading "--" for long options.  */
            {
                /* Allocate a mode string (e.g., "-rwx") by concatenating
                the argument containing this option.  If a previous mode
                string was given, concatenate the previous string, a
                comma, and the new string (e.g., "-s,-rwx").  */

                char const *arg = params[optind - 1];
                size_t arg_len = strlen (arg);
                size_t mode_comma_len = mode_len + !!mode_len;
                size_t new_mode_len = mode_comma_len + arg_len;
                if (mode_alloc <= new_mode_len)
                {
                    mode_alloc = new_mode_len + 1;
                    mode = realloc (mode, mode_alloc);
                }
                mode[mode_len] = ',';
                memcpy (mode + mode_comma_len, arg, arg_len + 1);
                mode_len = new_mode_len;
                printf(mode);
            }
            break;
        }
    }
    if (!mode)
        mode = params[optind++];
    change = mode_compile (mode);
    umask_value = umask (0);
    process_files (params + optind, FTS_COMFOLLOW | FTS_PHYSICAL);
    return 0;
}