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

#include "json_utils.c"


static void *
foreach (int *data, const item_t *item_data)
{
  static const char *expected[] = {
    "foo", "bar", "arr", "tux"
  };

  fail_unless (!strcmp (item_data->item, expected[*data]),
               "string \"%s\" should be \"%s\"",
               item_data->item, expected[*data]);
  if (*data == 2)
  {
    fail_unless (item_data->is_array, "\"arr\" must be an array");
    fail_unless (item_data->index == 2,
                 "index must be 2, but %d received", item_data->index);
  }

  ++*data;
  return data;
}

START_TEST (test_json_utils_tokenize)
{
  int cnt = 0;
  list_t *list = tokenize ("foo.bar.arr[2].tux");

  vh_list_foreach (list, &cnt, (void *) foreach);
  vh_list_free (list);
  fail_unless (cnt == 4,
               "expected 4 loops, but %d received", cnt);
}
END_TEST

START_TEST (test_json_utils_json_get)
{
  int i;
  const char *s;
  json_object *tmp;
  json_object *json = json_tokener_parse (" \
  {                    \
    \"a\": 3,          \
    \"b\": \"two\",    \
    \"c\": {           \
      \"tux\": 42,     \
      \"arr\": [       \
        \"walkyries\", \
        \"thor\", {    \
          \"id\": 8    \
        }              \
      ]                \
    }                  \
  }                    \
  ");

  tmp = vh_json_get (json, "a");
  i = json_object_get_int (tmp);
  fail_unless (i == 3, "expected 3 but received %d", i);

  tmp = vh_json_get (json, "b");
  s = json_object_get_string (tmp);
  fail_unless (!strcmp (s, "two"), "expected \"two\" but received %s", s);

  tmp = vh_json_get (json, "c.tux");
  i = json_object_get_int (tmp);
  fail_unless (i == 42, "expected 42 but received %d", i);

  tmp = vh_json_get (json, "c.arr[1]");
  s = json_object_get_string (tmp);
  fail_unless (!strcmp (s, "thor"), "expected \"thor\" but received %s", s);

  tmp = vh_json_get (json, "c.arr[2].id");
  i = json_object_get_int (tmp);
  fail_unless (i == 8, "expected 8 but received %d", i);

  char *sv = vh_json_get_str (json, "c.arr[0]");
  fail_unless (!strcmp (sv, "walkyries"),
               "expected \"walkyries\" but received %s", sv);
  free (sv);

  i = vh_json_get_int (json, "c.arr[2].id");
  fail_unless (i == 8, "expected 8 but received %d", i);

  json_object_put (json);
}
END_TEST

void
vh_test_json_utils (TCase *tc)
{
  tcase_add_test (tc, test_json_utils_tokenize);
  tcase_add_test (tc, test_json_utils_json_get);
}
