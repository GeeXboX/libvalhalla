/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <check.h>

#include "vh_test.h"

typedef struct vh_test_case_s {
  const char  *name;
  void       (*add) (TCase *tc);
} vh_test_case_t;

static const vh_test_case_t vtc[] = {
  { "osdep",        vh_test_osdep },
  { "parser",       vh_test_parser },
};


static Suite *
test_suite (void)
{
  Suite *s;
  unsigned int i;

  s = suite_create ("Valhalla");
  if (!s)
    return NULL;

  for (i = 0; i < sizeof (vtc) / sizeof (*vtc); i++)
  {
    TCase *tc;

    tc = tcase_create (vtc[i].name);
    if (!tc)
      break;

    vtc[i].add (tc);

    suite_add_tcase (s, tc);
  }

  return s;
}

int
main (void)
{
  Suite *s;
  SRunner *sr;
  int failed;

  s = test_suite ();
  if (!s)
    return -1;

  sr = srunner_create (s);
  if (!sr)
    return -1;

  srunner_run_all (sr, CK_NORMAL);
  failed = srunner_ntests_failed (sr);
  srunner_free (sr);

  return failed;
}
