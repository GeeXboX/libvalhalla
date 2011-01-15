/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2011 Mathieu Schroeter <mathieu@schroetersa.ch>
 *
 * This file is part of libvalhalla.
 *
 * libvalhalla is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libvalhalla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libvalhalla; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <check.h>

#include "vh_test.h"

#include "parser.c"

/*
 * The function parser_decrap_pattern() is case sensitive. Look at the
 * function parser_decrapify() for the complete decrapifier.
 */
START_TEST (test_parser_decrap_pattern)
{
  unsigned int i;
  const struct {
    const char *str;  /* input string */
    const char *bl;   /* black word */
    const char *res;  /* result string */
    unsigned int se;
    unsigned int ep;
  } list[] = {
    { "xxx",                  "SExEP",    "xxx",                  0, 0 },
    { "",                     "SExEP",     "",                    0, 0 },
    { "The Valkyries s01e02", "sSEeEP",   "The Valkyries       ", 1, 2 },
    { "The Valkyries S01E02", "sSEeEP",   "The Valkyries S01E02", 0, 0 },
    { "The Valkyries s01e02", "sNUMeNUM", "The Valkyries       ", 0, 0 },
    { "2 or 3 Valkyries",     "NUM",      "  or 3 Valkyries",     0, 0 },
    { "The Valkyries S4",     "SSE",      "The Valkyries   ",     4, 0 },
    { "The Valkyries E4",     "EEP",      "The Valkyries   ",     0, 4 },
    { "The Valkyries E4 E5",  "EEP",      "The Valkyries    E5",  0, 4 },
  };

  for (i = 0; i < sizeof (list) / sizeof (*list); i++)
  {
    unsigned int se = 0, ep = 0;
    char str[256];

    snprintf (str, sizeof (str), "%s", list[i].str);
    parser_decrap_pattern (str, list[i].bl, &se, &ep);
    fail_unless (!strcmp (str, list[i].res),
                 "badly decrapified with %s (expected : %s, found : %s)",
                 list[i].bl, list[i].res, str);
    fail_if (list[i].se != se,
             "the season number is not correct : %i, found : %i)",
             list[i].se, se);
    fail_if (list[i].ep != ep,
             "the episode number is not correct : %i, found : %i)",
             list[i].ep, ep);
  }
}
END_TEST

void
vh_test_parser (TCase *tc)
{
  tcase_add_test (tc, test_parser_decrap_pattern);
}
