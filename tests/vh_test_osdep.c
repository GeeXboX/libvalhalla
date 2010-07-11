/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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

#include <stdlib.h>
#include <string.h>

#include <check.h>

#include "osdep.h"
#include "vh_test.h"


START_TEST (test_osdep_strndup)
{
  unsigned int i;
  char *string;
  static const struct {
    const char *str_in;
    const char *str_out;
    size_t in;
    size_t out;
  } list[] = {
    { "",       "",       0, 0  },
    { "",       "",       1, 0  },
    { "foobar", "",       0, 0  },
    { "foobar", "f",      1, 1  },
    { "foobar", "fooba",  5, 5  },
    { "foobar", "foobar", 6, 6  },
    { "foobar", "foobar", 7, 6  },
  };

  for (i = 0; i < sizeof (list) / sizeof (*list); i++)
  {
    char copy[16];

    string = vh_strndup (list[i].str_in, list[i].in);
    fail_if (!string, "malloc error");
    strcpy (copy, string);
    free (string);

    fail_unless (strlen (copy) == list[i].out,
                 "strlen was %i instead of %i for \"%s\"",
                 strlen (list[i].str_in), list[i].out, list[i].str_in);

    fail_unless (!strcmp (copy, list[i].str_out),
                 "string \"%s\" should be \"%s\"", copy, list[i].str_out);
  }
}
END_TEST

START_TEST (test_osdep_strcasestr)
{
  unsigned int i;
  const char *haystack = "Chosen by Odin, half of those that die in combat "
                         "travel to Valhalla upon death, led by valkyries, "
                         "while the other half go to the goddess Freyja's "
                         "field Fólkvangr.";
  struct {
    const char *needle;
    const char *pos;
  } list[] = {
    { "odin",       haystack + 10 },
    { "VALHALLA",   haystack + 59 },
    { "Valkyries",  haystack + 87 },
  };

  for (i = 0; i < sizeof (list) / sizeof (*list); i++)
  {
    const char *it;

    it = vh_strcasestr (haystack, list[i].needle);
    fail_if (it != list[i].pos,
             "needle \"%s\" not found at the right position "
             "(expected : %i, found : %i)",
             list[i].needle, list[i].pos - haystack, it - haystack);
  }
}
END_TEST

START_TEST (test_osdep_strtok_r)
{
  unsigned int i;
  char *item, *buffer = NULL;
  char str[4096] = { 0 };
  const char *delim = "|";
  const char *aesir[] = { "Baldr", "Bragi", "Forseti", "Dellingr", "Freyr",
                          "Heimdallr", "Hermóðr", "Höðr", "Hœnir", "Lóðurr",
                          "Loki", "Meili", "Mímir", "Móði and Magni",
                          "Njörðr", "Odin", "Óðr", "Thor", "Týr", "Ullr",
                          "Váli", "Víðarr", "Vili and Vé" };

  for (i = 0; i < sizeof (aesir) / sizeof (*aesir); i++)
  {
    strcat (str, aesir[i]);
    if (i != sizeof (aesir) / sizeof (*aesir) - 1)
      strcat (str, delim);
  }

  i = 0;
  item = strtok_r (str, delim, &buffer);
  while (item)
  {
    fail_unless (!strcmp (item, aesir[i]),
                 "expected \"%s\" but item was \"%s\"", aesir[i], item);
    item = vh_strtok_r (NULL, delim, &buffer);
    i++;
  }
}
END_TEST

void
vh_test_osdep (TCase *tc)
{
  tcase_add_test (tc, test_osdep_strndup);
  tcase_add_test (tc, test_osdep_strcasestr);
  tcase_add_test (tc, test_osdep_strtok_r);
}
