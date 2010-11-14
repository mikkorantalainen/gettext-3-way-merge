/* Message list concatenation and duplicate handling.
   Copyright (C) 2001-2003, 2005-2010 Free Software Foundation, Inc.
   Hacked by P. Christeas <p_christ@hol.gr>, 2010
   Original by Bruno Haible <haible@clisp.cons.org>, 2001.

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
#include <alloca.h>

/* Specification.  */
#include "msgl-3way.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "xerror.h"
#include "xvasprintf.h"
#include "message.h"
#include "read-catalog.h"
#include "po-charset.h"
#include "msgl-ascii.h"
#include "msgl-equal.h"
#include "msgl-iconv.h"
#include "xalloc.h"
#include "xmalloca.h"
#include "c-strstr.h"
#include "basename.h"
#include "gettext.h"

#define _(str) gettext (str)

bool msg3way_has_merges = false;

static bool msg3way_headers(message_ty* fin, const message_ty* remote)
{
    char * final;
    const char *rev_datea, *rev_dateb;
    const char *por_hdr = "PO-Revision-Date:";
    const size_t por_hdrlen = strlen(por_hdr);
    bool local_last = true;
    size_t n;
    
    /* as a special step, be sure to include copyright headers from
       both sources
    */
    if (remote->comment)
        for(n=0; n < remote->comment->nitems; n++)
            if (strcasestr(remote->comment->item[n], "copyright") &&
                !string_list_member(fin->comment,remote->comment->item[n]))
              string_list_append(fin->comment,remote->comment->item[n]);
        
    if (!remote->msgstr_len)
        return true;
    else if (!fin->msgstr_len){
        fin->msgstr = remote->msgstr;
        fin->msgstr_len = remote->msgstr_len;
        return true;
    }
    
    rev_datea = c_strstr(fin->msgstr, por_hdr)+por_hdrlen;
    rev_dateb = c_strstr(remote->msgstr, por_hdr)+por_hdrlen;
    
    for(;rev_datea && rev_dateb; rev_datea++, rev_dateb++){
        /* we only want to compare the string part of fin, remote
           after the revision-date header, to see which one is
           more recent
        */
        if (*rev_datea == '\n' || *rev_dateb == '\n')
            break;
        if ((rev_datea >= fin->msgstr + fin->msgstr_len) ||
              (rev_dateb >= remote->msgstr + remote->msgstr_len))
            break;
        if ( *rev_dateb > *rev_datea){
            local_last = false;
        }
    }

    if (!local_last){
        fin->msgstr = remote->msgstr;
        fin->msgstr_len = remote->msgstr_len;
    }

    return true;
}

msgdomain_list_ty *
       merge_3way_msgdomain_list (const char *a_file, const char* b_file,
                                  const char *origin_file,
                                catalog_input_format_ty input_syntax,
                                const char *to_code)
{
  msgdomain_list_ty *mdlps[3];
  msgdomain_list_ty *mdl_a, *mdl_b, *mdl_org;
  const char ***canon_charsets;
  const char ***identifications;
  msgdomain_list_ty *final_mdlp;
  const char *canon_to_code;
  size_t n, j;

  /* Read input files.  */
  mdlps[0] = mdl_a = read_catalog_file (a_file, input_syntax);
  mdlps[1] = mdl_b = read_catalog_file (b_file, input_syntax);
  mdlps[2] = mdl_org = read_catalog_file (origin_file, input_syntax);

  /* Determine the canonical name of each input file's encoding.  */
  canon_charsets = XNMALLOC (3, const char **);
  for (n = 0; n < 3; n++)
    {
      msgdomain_list_ty *mdlp = mdlps[n];
      size_t k;
      const char *filename;
      switch (n){
        case 0:
            filename = a_file;
            break;
        case 1:
            filename = b_file;
            break;
        case 2:
            filename = origin_file;
            break;
      }
      /* Will iterate over files + domains: 2-dim array  */
      canon_charsets[n] = XNMALLOC (mdlp->nitems, const char *);
      for (k = 0; k < mdlp->nitems; k++)
        {
          message_list_ty *mlp = mdlp->item[k]->messages;
          const char *canon_from_code = NULL;

          if (mlp->nitems > 0)
            {
              for (j = 0; j < mlp->nitems; j++)
                if (is_header (mlp->item[j]) && !mlp->item[j]->obsolete)
                  {
                    const char *header = mlp->item[j]->msgstr;

                    if (header != NULL)
                      {
                        const char *charsetstr = c_strstr (header, "charset=");

                        if (charsetstr != NULL)
                          {
                            size_t len;
                            char *charset;
                            const char *canon_charset;

                            charsetstr += strlen ("charset=");
                            len = strcspn (charsetstr, " \t\n");
                            charset = (char *) xmalloca (len + 1);
                            memcpy (charset, charsetstr, len);
                            charset[len] = '\0';

                            canon_charset = po_charset_canonicalize (charset);
                            if (canon_charset == NULL)
                              {
                                /* Don't give an error for POT files, because
                                   POT files usually contain only ASCII
                                   msgids.  */
                                
                                size_t filenamelen = strlen (filename);

                                if (filenamelen >= 4
                                    && memcmp (filename + filenamelen - 4,
                                               ".pot", 4) == 0
                                    && strcmp (charset, "CHARSET") == 0)
                                  canon_charset = po_charset_ascii;
                                else
                                  error (EXIT_FAILURE, 0,
                                         _("\
present charset \"%s\" is not a portable encoding name"),
                                         charset);
                              }

                            freea (charset);

                            if (canon_from_code == NULL)
                              canon_from_code = canon_charset;
                            else if (canon_from_code != canon_charset)
                              error (EXIT_FAILURE, 0,
                                     _("\
two different charsets \"%s\" and \"%s\" in input file"),
                                     canon_from_code, canon_charset);
                          }
                      }
                  }
              if (canon_from_code == NULL)
                {
                  if (is_ascii_message_list (mlp))
                    canon_from_code = po_charset_ascii;
                  else if (mdlp->encoding != NULL)
                    canon_from_code = mdlp->encoding;
                  else
                    {
                      if (k == 0)
                        error (EXIT_FAILURE, 0, _("\
input file `%s' doesn't contain a header entry with a charset specification"),
                               filename);
                      else
                        error (EXIT_FAILURE, 0, _("\
domain \"%s\" in input file `%s' doesn't contain a header entry with a charset specification"),
                               mdlp->item[k]->domain, filename);
                    }
                }
            }
          canon_charsets[n][k] = canon_from_code;
        }
    }

  /* Determine textual identifications of each file/domain combination.  */
  identifications = XNMALLOC (2, const char **);
  for (n = 0; n < 2; n++)
    {
      const char *filename = basename ( (n==0)? a_file: b_file);
      msgdomain_list_ty *mdlp = mdlps[n];
      size_t k;

      identifications[n] = XNMALLOC (mdlp->nitems, const char *);
      for (k = 0; k < mdlp->nitems; k++)
        {
          const char *domain = mdlp->item[k]->domain;
          message_list_ty *mlp = mdlp->item[k]->messages;
          char *project_id = NULL;

          for (j = 0; j < mlp->nitems; j++)
            if (is_header (mlp->item[j]) && !mlp->item[j]->obsolete)
              {
                const char *header = mlp->item[j]->msgstr;

                if (header != NULL)
                  {
                    const char *cp = c_strstr (header, "Project-Id-Version:");

                    if (cp != NULL)
                      {
                        const char *endp;

                        cp += sizeof ("Project-Id-Version:") - 1;

                        endp = strchr (cp, '\n');
                        if (endp == NULL)
                          endp = cp + strlen (cp);

                        while (cp < endp && *cp == ' ')
                          cp++;

                        if (cp < endp)
                          {
                            size_t len = endp - cp;
                            project_id = XNMALLOC (len + 1, char);
                            memcpy (project_id, cp, len);
                            project_id[len] = '\0';
                          }
                        break;
                      }
                  }
              }

          identifications[n][k] =
            (project_id != NULL
             ? (k > 0 ? xasprintf ("%s:%s (%s)", filename, domain, project_id)
                      : xasprintf ("%s (%s)", filename, project_id))
             : (k > 0 ? xasprintf ("%s:%s", filename, domain)
                      : xasprintf ("%s", filename)));
        }
    }

  /* Determine the target encoding for the messages.  */
  if (to_code != NULL)
    {
      /* Canonicalize target encoding.  */
      canon_to_code = po_charset_canonicalize (to_code);
      if (canon_to_code == NULL)
        error (EXIT_FAILURE, 0,
               _("target charset \"%s\" is not a portable encoding name."),
               to_code);
    }
  else
    {
      /* No target encoding was specified.  Test whether the messages are
         all in a single encoding.  If so, conversion is not needed.  */
      const char *first = NULL;
      const char *second = NULL;
      bool with_ASCII = false;
      bool with_UTF8 = false;
      bool all_ASCII_compatible = true;

      size_t k;

      for (k = 0; k < mdl_a->nitems; k++)
        if (canon_charsets[0][k] != NULL)
          {
            if (canon_charsets[0][k] == po_charset_ascii)
              with_ASCII = true;
            else
              {
                if (first == NULL)
                  first = canon_charsets[0][k];
                else if (canon_charsets[0][k] != first && second == NULL)
                  second = canon_charsets[0][k];

                if (strcmp (canon_charsets[0][k], "UTF-8") == 0)
                  with_UTF8 = true;

                if (!po_charset_ascii_compatible (canon_charsets[0][k]))
                  all_ASCII_compatible = false;
              }
          }

      if (with_ASCII && !all_ASCII_compatible)
        {
          /* assert (first != NULL); */
          if (second == NULL)
            second = po_charset_ascii;
        }

      if (second != NULL)
        {
          /* A conversion is needed.  Warn the user since he hasn't asked
             for it and might be surprised.  */
          if (with_UTF8)
            multiline_warning (xasprintf (_("warning: ")),
                               xasprintf (_("\
Input files contain messages in different encodings, UTF-8 among others.\n\
Converting the output to UTF-8.\n\
")));
          else
            multiline_warning (xasprintf (_("warning: ")),
                               xasprintf (_("\
Input files contain messages in different encodings, %s and %s among others.\n\
Converting the output to UTF-8.\n\
To select a different output encoding, use the --to-code option.\n\
"), first, second));
          canon_to_code = po_charset_utf8;
        }
      else if (first != NULL && with_ASCII && all_ASCII_compatible)
        {
          /* The conversion is a no-op conversion.  Don't warn the user,
             but still perform the conversion, in order to check that the
             input was really ASCII.  */
          canon_to_code = first;
        }
      else
        {
          /* No conversion needed.  */
          canon_to_code = NULL;
        }
    }

  /* Now convert the remaining messages to to_code.  */
  if (canon_to_code != NULL)
    for (n = 0; n < 3; n++)
      {
        msgdomain_list_ty *mdlp = mdlps[n];
        size_t k;
        const char *filename;
        switch (n){
          case 0:
              filename = a_file;
              break;
          case 1:
              filename = b_file;
              break;
          case 2:
              filename = origin_file;
              break;
        }

        for (k = 0; k < mdlp->nitems; k++)
          if (canon_charsets[n][k] != NULL)
            /* If the user hasn't given a to_code, don't bother doing a noop
               conversion that would only replace the charset name in the
               header entry with its canonical equivalent.  */
            if (!(to_code == NULL && canon_charsets[n][k] == canon_to_code))
              if (iconv_message_list (mdlp->item[k]->messages,
                                      canon_charsets[n][k], canon_to_code,
                                      filename))
                {
                  multiline_error (xstrdup (""),
                                   xasprintf (_("\
Conversion of file %s from %s encoding to %s encoding\n\
changes some msgids or msgctxts.\n\
Either change all msgids and msgctxts to be pure ASCII, or ensure they are\n\
UTF-8 encoded from the beginning, i.e. already in your source code files.\n"),
                                              filename, canon_charsets[n][k],
                                              canon_to_code));
                  exit (EXIT_FAILURE);
                }
      }

    /* Iterate over the messages in file A, see if they appear in the diff
       of  B - Origin, else copy
     */
  final_mdlp = msgdomain_list_alloc (true);
  {
    size_t k;
    for (k = 0; k < mdl_a->nitems; k++)
      {
        /*-* todo: encode the domain(s) */
        const char *domain = mdl_a->item[k]->domain;
        message_list_ty *mlp = mdl_a->item[k]->messages;
        message_list_ty *mlp_dom = NULL; /* the target list */
        message_list_ty *mlp_bdom = NULL; /* Corresponding list in B */
        message_list_ty *mlp_ordom = NULL; /* Corresponding list in Orig */
        

        mlp_dom = msgdomain_list_sublist (final_mdlp, domain, true);
        mlp_bdom = msgdomain_list_sublist (mdl_b, domain, false);
        mlp_ordom = msgdomain_list_sublist (mdl_org, domain, false);

        for (j = 0; j < mlp->nitems; j++)
          {
            message_ty *mp = mlp->item[j];
            message_ty *tmp, *mpb = NULL, *mpor = NULL;
            size_t i;

            tmp = message_list_search (mlp_dom, mp->msgctxt, mp->msgid);
            if (tmp == NULL)
              {
                tmp = message_copy (mp);
                tmp->obsolete = mp->obsolete;
                message_list_append (mlp_dom, tmp);
              }

            if ((!is_header (mp) && mp->is_fuzzy)
                    || mp->msgstr[0] == '\0')
              /* Weak translation.  Counted as negative tmp->used.  */
              {
                if (tmp->used <= 0)
                  tmp->used--;
              }
            else
              /* Good translation.  Counted as positive tmp->used.  */
              {
                if (tmp->used < 0)
                  tmp->used = 0;
                tmp->used++;
              }
            mp->tmp = tmp;
            
            if (mlp_bdom)
              mpb = message_list_search (mlp_bdom, mp->msgctxt, mp->msgid);
            if (mlp_ordom)
              mpor = message_list_search (mlp_ordom, mp->msgctxt, mp->msgid);
            
            if (mpb)
                mpb->used++;
            if (mpb && mpb->msgstr[0] == '\0')
                mpb = NULL;
            if (mpor && mpor->msgstr[0] == '\0')
                mpor = NULL;
            
            /* Here is the core 3-way algorithm: */
            if (is_header(mp) && (mpb) && msg3way_headers(tmp, mpb))
                continue ;
            if (mpb){
                /* if msg has been added/changed at B - Orig, add to A */
                if (!mpor || !message_str_equal(mpb, mpor, true)) {
                    
                    char *new_msgstr;
                    new_msgstr = XNMALLOC(mpb->msgstr_len + 1, char);
                    memcpy(new_msgstr, mpb->msgstr, mpb->msgstr_len);
                    new_msgstr[mpb->msgstr_len] = '\0';
                
                    if ((tmp->msgstr_len == 0) ||
                            (tmp->is_fuzzy && !mpb->is_fuzzy) ||
                            (message_str_equal(mp, mpor, true))){
                        tmp->msgstr = new_msgstr;
                        tmp->msgstr_len = mpb->msgstr_len;
                        tmp->is_fuzzy = mpb->is_fuzzy;
                    }
                    else {
                        /* put as alternate */
                        size_t nbytes;
                        i = tmp->alternative_count;
                        nbytes = (i + 2) * sizeof (struct altstr);
                        tmp->alternative = xrealloc (tmp->alternative, nbytes);

                        { /* one from A */
                            tmp->alternative[i].id= xasprintf ("#-#-#-#-#  %s  #-#-#-#-#",
                                                    identifications[0][k]);
                            tmp->alternative[i].msgstr = tmp->msgstr;
                            tmp->alternative[i].msgstr_len = tmp->msgstr_len;
                            tmp->alternative[i].msgstr_end = tmp->msgstr + tmp->msgstr_len;
                            tmp->alternative[i].comment = tmp->comment;
                            tmp->alternative[i].comment_dot = tmp->comment_dot;
                            /* must zero the tmp, especially the lists */
                            tmp->msgstr = NULL;
                            tmp->msgstr_len = 0L;
                            tmp->comment = NULL;
                            tmp->comment_dot = NULL;
                            i++;
                        }
                        { /* and one from B */
                            tmp->alternative[i].id= xasprintf ("#-#-#-#-#  %s  #-#-#-#-#",
                                                    identifications[1][k]);
                            tmp->alternative[i].msgstr = new_msgstr;
                            tmp->alternative[i].msgstr_len = mpb->msgstr_len;
                            tmp->alternative[i].msgstr_end = new_msgstr + mpb->msgstr_len;
                            tmp->alternative[i].comment = mpb->comment;
                            tmp->alternative[i].comment_dot = mpb->comment_dot;
                        }
                        tmp->alternative_count = i + 1;
                    }
                }
            }
            else if (mpor){
                /* mpb removed from B - Orig. If A has the same one,
                   remove too.
                */
                if (message_str_equal(tmp, mpor, false)){
                    tmp->msgstr = "";
                    tmp->msgstr_len = 0;
                    tmp->used = 0;
                }
            }
        
          }

      }

      /* second iteration: We try to find strings that have not been 
         processed from (B - Origin)
      */
      for (k = 0; k < mdl_b->nitems; k++)
      {
        /*-* todo: encode the mpb domain... */
        const char *domain = mdl_b->item[k]->domain;
        message_list_ty *mlp_dom = NULL; /* the target list */
        message_list_ty *mlp_bdom = mdl_b->item[k]->messages;
        message_list_ty *mlp_ordom = NULL; /* Corresponding list in Orig */
        

        mlp_dom = msgdomain_list_sublist (final_mdlp, domain, true);
        mlp_ordom = msgdomain_list_sublist (mdl_org, domain, false);
        
        for (j = 0; j < mlp_bdom->nitems; j++)
          {
            message_ty *mp = mlp_bdom->item[j];
            message_ty *tmp, *mpor = NULL;
            size_t i;
            
            if ((mp->used > 0) || (!mp->msgstr_len))
                continue;

            tmp = message_list_search (mlp_dom, mp->msgctxt, mp->msgid);
            if (tmp){
                error (EXIT_FAILURE, 0, _("Algorithm error: B message \"%s\":\"%s\" reappeared"),
                        tmp->msgid, tmp->msgstr);
                break;
            }
            mpor = message_list_search (mlp_ordom, mp->msgctxt, mp->msgid);
            if (!mpor || !message_str_equal(mp, mpor, true)) {
                tmp = message_copy (mp);
                tmp->obsolete = mp->obsolete;
                message_list_append (mlp_dom, tmp);
            }
          }
      }
  }


  /* Determine the common known a-priori encoding, if any.  */
  /*if (nfiles > 0)
    {
      bool all_same_encoding = true;

      for (n = 1; n < nfiles; n++)
        if (mdlps[n]->encoding != mdlps[0]->encoding)
          {
            all_same_encoding = false;
            break;
          }

      if (all_same_encoding)
        total_mdlp->encoding = mdlps[0]->encoding;
    }*/



  {
    size_t k;

    for (k = 0; k < final_mdlp->nitems; k++)
      {
        message_list_ty *mlp = final_mdlp->item[k]->messages;

        for (j = 0; j < mlp->nitems; j++)
          {
            message_ty *tmp = mlp->item[j];

            if (tmp->alternative_count > 0)
              {
                /* Test whether all alternative translations are equal.  */
                struct altstr *first = &tmp->alternative[0];
                size_t i;

                for (i = 0; i < tmp->alternative_count; i++)
                  if (!(tmp->alternative[i].msgstr_len == first->msgstr_len
                        && memcmp (tmp->alternative[i].msgstr, first->msgstr,
                                   first->msgstr_len) == 0))
                    break;

                if (i == tmp->alternative_count)
                  {
                    /* All alternatives are equal.  */
                    tmp->msgstr = first->msgstr;
                    tmp->msgstr_len = first->msgstr_len;
                  }
                else
                  {
                    /* Concatenate the alternative msgstrs into a single one,
                       separated by markers.  */
                    msg3way_has_merges = true;
                    size_t len;
                    const char *p;
                    const char *p_end;
                    char *new_msgstr;
                    char *np;

                    len = 0;
                    for (i = 0; i < tmp->alternative_count; i++)
                      {
                        size_t id_len = strlen (tmp->alternative[i].id);

                        len += tmp->alternative[i].msgstr_len;

                        p = tmp->alternative[i].msgstr;
                        p_end = tmp->alternative[i].msgstr_end;
                        for (; p < p_end; p += strlen (p) + 1)
                          len += id_len + 2;
                      }

                    new_msgstr = XNMALLOC (len, char);
                    np = new_msgstr;
                    for (;;)
                      {
                        /* Test whether there's one more plural form to
                           process.  */
                        for (i = 0; i < tmp->alternative_count; i++)
                          if (tmp->alternative[i].msgstr
                              < tmp->alternative[i].msgstr_end)
                            break;
                        if (i == tmp->alternative_count)
                          break;

                        /* Process next plural form.  */
                        for (i = 0; i < tmp->alternative_count; i++)
                          if (tmp->alternative[i].msgstr
                              < tmp->alternative[i].msgstr_end)
                            {
                              if (np > new_msgstr && np[-1] != '\0'
                                  && np[-1] != '\n')
                                *np++ = '\n';

                              len = strlen (tmp->alternative[i].id);
                              memcpy (np, tmp->alternative[i].id, len);
                              np += len;
                              *np++ = '\n';

                              len = strlen (tmp->alternative[i].msgstr);
                              memcpy (np, tmp->alternative[i].msgstr, len);
                              np += len;
                              tmp->alternative[i].msgstr += len + 1;
                            }

                        /* Plural forms are separated by NUL bytes.  */
                        *np++ = '\0';
                      }
                    tmp->msgstr = new_msgstr;
                    tmp->msgstr_len = np - new_msgstr;

                    tmp->is_fuzzy = true;
                  }

                /* Test whether all alternative comments are equal.  */
                for (i = 0; i < tmp->alternative_count; i++)
                  if (tmp->alternative[i].comment == NULL
                      || !string_list_equal (tmp->alternative[i].comment,
                                             first->comment))
                    break;

                if (i == tmp->alternative_count)
                  /* All alternatives are equal.  */
                  tmp->comment = first->comment;
                else
                  /* Concatenate the alternative comments into a single one,
                     separated by markers.  */
                  msg3way_has_merges = true;
                  for (i = 0; i < tmp->alternative_count; i++)
                    {
                      string_list_ty *slp = tmp->alternative[i].comment;

                      if (slp != NULL)
                        {
                          size_t l;

                          message_comment_append (tmp, tmp->alternative[i].id);
                          for (l = 0; l < slp->nitems; l++)
                            message_comment_append (tmp, slp->item[l]);
                        }
                    }

                /* Test whether all alternative dot comments are equal.  */
                for (i = 0; i < tmp->alternative_count; i++)
                  if (tmp->alternative[i].comment_dot == NULL
                      || !string_list_equal (tmp->alternative[i].comment_dot,
                                             first->comment_dot))
                    break;

                if (i == tmp->alternative_count)
                  /* All alternatives are equal.  */
                  tmp->comment_dot = first->comment_dot;
                else {
                  msg3way_has_merges = true;
                  /* Concatenate the alternative dot comments into a single one,
                     separated by markers.  */
                  for (i = 0; i < tmp->alternative_count; i++)
                    {
                      string_list_ty *slp = tmp->alternative[i].comment_dot;

                      if (slp != NULL)
                        {
                          size_t l;

                          message_comment_dot_append (tmp,
                                                      tmp->alternative[i].id);
                          for (l = 0; l < slp->nitems; l++)
                            message_comment_dot_append (tmp, slp->item[l]);
                        }
                    }
                }
              }
          }
      }
  }

  return final_mdlp;
}
