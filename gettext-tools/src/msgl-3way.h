/* Message list concatenation and duplicate handling.
   Copyright (C) 2001-2003, 2006-2010 Free Software Foundation, Inc.
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

#ifndef _MSGL_3WAY_H
#define _MSGL_3WAY_H

#include <stdbool.h>

#include "message.h"
#include "str-list.h"
#include "read-catalog-abstract.h"


#ifdef __cplusplus
extern "C" {
#endif

/* If true, omit the header entry.
   If false, keep the header entry present in the input.  */
extern DLL_VARIABLE bool msg3way_has_merges;

extern msgdomain_list_ty *
       merge_3way_msgdomain_list (const char *a_file, const char* b_file,
                                  const char *origin_file,
                                catalog_input_format_ty input_syntax,
                                const char *to_code);


#ifdef __cplusplus
}
#endif


#endif /* _MSGL_3WAY_H */
