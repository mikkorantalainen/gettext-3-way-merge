/* Performs 3-way merge of translation catalogs.
   Copyright (C) 2001-2007, 2009-2010 Free Software Foundation, Inc.
   Written by P. Christeas <p_christ@hol.gr>, 2010
   Based on msgcat by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "closeout.h"
#include "dir-list.h"
#include "str-list.h"
#include "file-list.h"
#include "error.h"
#include "error-progname.h"
#include "progname.h"
#include "relocatable.h"
#include "basename.h"
#include "message.h"
#include "read-catalog.h"
#include "read-po.h"
#include "read-properties.h"
#include "read-stringtable.h"
#include "write-catalog.h"
#include "write-po.h"
#include "write-properties.h"
#include "write-stringtable.h"
#include "color.h"
#include "msgl-3way.h"
#include "msgl-header.h"
#include "propername.h"
#include "gettext.h"

#define _(str) gettext (str)


/* Force output of PO file even if empty.  */
static int force_po;

/* Target encoding.  */
static const char *to_code;

/* Long options.  */
static const struct option long_options[] =
{
  { "color", optional_argument, NULL, CHAR_MAX + 5 },
  { "escape", no_argument, NULL, 'E' },
  { "force-po", no_argument, &force_po, 1 },
  { "help", no_argument, NULL, 'h' },
  { "indent", no_argument, NULL, 'i' },
  { "lang", required_argument, NULL, CHAR_MAX + 7 },
  { "no-escape", no_argument, NULL, 'e' },
  { "no-location", no_argument, &line_comment, 0 },
  { "no-wrap", no_argument, NULL, CHAR_MAX + 2 },
  { "output-file", required_argument, NULL, 'o' },
  { "properties-input", no_argument, NULL, 'P' },
  { "properties-output", no_argument, NULL, 'p' },
  { "sort-by-file", no_argument, NULL, 'F' },
  { "sort-output", no_argument, NULL, 's' },
  { "strict", no_argument, NULL, 'S' },
  { "stringtable-input", no_argument, NULL, CHAR_MAX + 3 },
  { "stringtable-output", no_argument, NULL, CHAR_MAX + 4 },
  { "style", required_argument, NULL, CHAR_MAX + 6 },
  { "to-code", required_argument, NULL, 't' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w', },
  { NULL, 0, NULL, 0 }
};


/* Forward declaration of local functions.  */
static void usage (int status)
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
        __attribute__ ((noreturn))
#endif
;


int
main (int argc, char **argv)
{
  int cnt;
  int optchar;
  bool do_help;
  bool do_version;
  char *output_file;
  msgdomain_list_ty *result;
  catalog_input_format_ty input_syntax = &input_format_po;
  catalog_output_format_ty output_syntax = &output_format_po;
  bool sort_by_msgid = false;
  bool sort_by_filepos = false;
  /* Language (ISO-639 code) and optional territory (ISO-3166 code).  */
  const char *catalogname = NULL;

  /* Set program name for messages.  */
  set_program_name (argv[0]);
  error_print_progname = maybe_print_progname;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, relocate (LOCALEDIR));
  bindtextdomain ("bison-runtime", relocate (BISON_LOCALEDIR));
  textdomain (PACKAGE);

  /* Ensure that write errors on stdout are detected.  */
  atexit (close_stdout);

  /* Set default values for variables.  */
  do_help = false;
  do_version = false;
  output_file = NULL;

  while ((optchar = getopt_long (argc, argv, "eEFhino:pPst:Vw:",
                                 long_options, NULL)) != EOF)
    switch (optchar)
      {
      case '\0':                /* Long option.  */
        break;

      case 'e':
        message_print_style_escape (false);
        break;

      case 'E':
        message_print_style_escape (true);
        break;

      case 'F':
        sort_by_filepos = true;
        break;

      case 'h':
        do_help = true;
        break;

      case 'i':
        message_print_style_indent ();
        break;

      case 'n':
        line_comment = 1;
        break;

      case 'o':
        output_file = optarg;
        break;

      case 'p':
        output_syntax = &output_format_properties;
        break;

      case 'P':
        input_syntax = &input_format_properties;
        break;

      case 's':
        sort_by_msgid = true;
        break;

      case 'S':
        message_print_style_uniforum ();
        break;

      case 't':
        to_code = optarg;
        break;

      case 'V':
        do_version = true;
        break;

      case 'w':
        {
          int value;
          char *endp;
          value = strtol (optarg, &endp, 10);
          if (endp != optarg)
            message_page_width_set (value);
        }
        break;

      case CHAR_MAX + 2: /* --no-wrap */
        message_page_width_ignore ();
        break;

      case CHAR_MAX + 3: /* --stringtable-input */
        input_syntax = &input_format_stringtable;
        break;

      case CHAR_MAX + 4: /* --stringtable-output */
        output_syntax = &output_format_stringtable;
        break;

      case CHAR_MAX + 5: /* --color */
        if (handle_color_option (optarg))
          usage (EXIT_FAILURE);
        break;

      case CHAR_MAX + 6: /* --style */
        handle_style_option (optarg);
        break;

      case CHAR_MAX + 7: /* --lang */
        catalogname = optarg;
        break;

      default:
        usage (EXIT_FAILURE);
        /* NOTREACHED */
      }

  /* Version information requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
"),
              "2001-2010");
      printf (_("Written by %s.\n"), proper_name ("Bruno Haible"));
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  if (color_test_mode)
    {
      print_color_test ();
      exit (EXIT_SUCCESS);
    }

  /* Verify selected options.  */
  if (!line_comment && sort_by_filepos)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
           "--no-location", "--sort-by-file");

  if (sort_by_msgid && sort_by_filepos)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
           "--sort-output", "--sort-by-file");

  if (argc - optind != 3)
      error (EXIT_FAILURE, 0, _("Must provide 3 files to work with: <A> <B> <origin>"));
  /* Append names from command line.  */

  /* Read input files, then filter, convert and merge messages.  */
  result = merge_3way_msgdomain_list(argv[optind], argv[optind+1], argv[optind+2],
                             input_syntax,
                             output_syntax->requires_utf8 ? "UTF-8" : to_code);

  /* Sorting the list of messages.  */
  if (sort_by_filepos)
    msgdomain_list_sort_by_filepos (result);
  else if (sort_by_msgid)
    msgdomain_list_sort_by_msgid (result);

  /* Set the Language field in the header.  */
  if (catalogname != NULL)
    msgdomain_list_set_header_field (result, "Language:", catalogname);

  /* Write the PO file.  */
  msgdomain_list_print (result, output_file, output_syntax, force_po, false);

  if (msg3way_has_merges)
      exit(EXIT_FAILURE);
  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION] [INPUTFILE]...\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Perform a 3-way merge of files A B that have common <origin>. In other\n\
words, file A is being merged with the forward diff of (B - origin) \
"));
      printf ("\n");
      printf (_("\
Mandatory arguments to long options are mandatory for short options too.\n"));
      printf ("\n");
      printf (_("\
Input file location:\n"));
      printf (_("\
  A-INPUT B-INPUT ORIGIN      input files\n"));
      printf (_("\
Output file location:\n"));
      printf (_("\
  -o, --output-file=FILE      write output to specified file\n"));
      printf (_("\
The results are written to standard output if no output file is specified\n\
or if it is -.\n"));
      printf ("\n");
      printf (_("\
Input file syntax:\n"));
      printf (_("\
  -P, --properties-input      input files are in Java .properties syntax\n"));
      printf (_("\
      --stringtable-input     input files are in NeXTstep/GNUstep .strings\n\
                              syntax\n"));
      printf ("\n");
      printf (_("\
Output details:\n"));
      printf (_("\
  -t, --to-code=NAME          encoding for output\n"));
      printf (_("\
      --lang=CATALOGNAME      set 'Language' field in the header entry\n"));
      printf (_("\
      --color                 use colors and other text attributes always\n\
      --color=WHEN            use colors and other text attributes if WHEN.\n\
                              WHEN may be 'always', 'never', 'auto', or 'html'.\n"));
      printf (_("\
      --style=STYLEFILE       specify CSS style rule file for --color\n"));
      printf (_("\
  -e, --no-escape             do not use C escapes in output (default)\n"));
      printf (_("\
  -E, --escape                use C escapes in output, no extended chars\n"));
      printf (_("\
      --force-po              write PO file even if empty\n"));
      printf (_("\
  -i, --indent                write the .po file using indented style\n"));
      printf (_("\
      --no-location           do not write '#: filename:line' lines\n"));
      printf (_("\
  -n, --add-location          generate '#: filename:line' lines (default)\n"));
      printf (_("\
      --strict                write out strict Uniforum conforming .po file\n"));
      printf (_("\
  -p, --properties-output     write out a Java .properties file\n"));
      printf (_("\
      --stringtable-output    write out a NeXTstep/GNUstep .strings file\n"));
      printf (_("\
  -w, --width=NUMBER          set output page width\n"));
      printf (_("\
      --no-wrap               do not break long message lines, longer than\n\
                              the output page width, into several lines\n"));
      printf (_("\
  -s, --sort-output           generate sorted output\n"));
      printf (_("\
  -F, --sort-by-file          sort output by file location\n"));
      printf ("\n");
      printf (_("\
Informative output:\n"));
      printf (_("\
  -h, --help                  display this help and exit\n"));
      printf (_("\
  -V, --version               output version information and exit\n"));
      printf ("\n");
      /* TRANSLATORS: The placeholder indicates the bug-reporting address
         for this package.  Please add _another line_ saying
         "Report translation bugs to <...>\n" with the address for translation
         bugs (typically your translation team's web or email address).  */
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"),
             stdout);
    }

  exit (status);
}