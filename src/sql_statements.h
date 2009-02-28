/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef VALHALLA_SQL_STATEMENTS_H
#define VALHALLA_SQL_STATEMENTS_H

/******************************************************************************/
/*                                                                            */
/*                                 Controls                                   */
/*                                                                            */
/******************************************************************************/

#define BEGIN_TRANSACTION \
 "BEGIN;"

#define END_TRANSACTION   \
 "COMMIT;"

/******************************************************************************/
/*                                                                            */
/*                              Create tables                                 */
/*                                                                            */
/******************************************************************************/

#define CREATE_TABLE_FILE                                 \
 "CREATE TABLE IF NOT EXISTS file ( "                     \
 "  file_id          INTEGER PRIMARY KEY AUTOINCREMENT, " \
 "  file_path        TEXT    NOT NULL UNIQUE, "           \
 "  file_mtime       INTEGER NOT NULL, "                  \
 "  file_meta_title  TEXT    NULL, "                      \
 "  file_meta_year   INTEGER NULL, "                      \
 "  file_meta_track  INTERGER NULL, "                     \
 "  checked__        INTEGER NOT NULL, "                  \
 "  _author_id       INTEGER NULL, "                      \
 "  _album_id        INTEGER NULL, "                      \
 "  _genre_id        INTEGER NULL "                       \
 ");"

#define CREATE_TABLE_AUTHOR                               \
 "CREATE TABLE IF NOT EXISTS author ( "                   \
 "  author_id        INTEGER PRIMARY KEY AUTOINCREMENT, " \
 "  author_name      TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_ALBUM                                \
 "CREATE TABLE IF NOT EXISTS album ( "                    \
 "  album_id         INTEGER PRIMARY KEY AUTOINCREMENT, " \
 "  album_name       TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_GENRE                                \
 "CREATE TABLE IF NOT EXISTS genre ( "                    \
 "  genre_id         INTEGER PRIMARY KEY AUTOINCREMENT, " \
 "  genre_name       TEXT    NOT NULL UNIQUE "            \
 ");"

#define CREATE_TABLE_ALLOC_AUTHOR_ALBUM                   \
 "CREATE TABLE IF NOT EXISTS alloc_author_album ( "       \
 "  author_id        INTEGER NOT NULL, "                  \
 "  album_id         INTEGER NOT NULL, "                  \
 "  PRIMARY KEY (author_id, album_id) "                   \
 ");"

#define CREATE_TABLE_ALLOC_GENRE_ALBUM                    \
 "CREATE TABLE IF NOT EXISTS alloc_genre_album ( "        \
 "  genre_id         INTEGER NOT NULL, "                  \
 "  album_id         INTEGER NOT NULL, "                  \
 "  PRIMARY KEY (genre_id, album_id) "                    \
 ");"

/******************************************************************************/
/*                                                                            */
/*                              Create indexes                                */
/*                                                                            */
/******************************************************************************/

#define CREATE_INDEX_FILE_PATH        \
 "CREATE UNIQUE INDEX IF NOT EXISTS " \
 "file_path_idx ON file (file_path);"

#define CREATE_INDEX_AUTHOR_NAME      \
 "CREATE UNIQUE INDEX IF NOT EXISTS " \
 "author_name_idx ON author (author_name);"

#define CREATE_INDEX_ALBUM_NAME       \
 "CREATE UNIQUE INDEX IF NOT EXISTS " \
 "album_name_idx ON album (album_name);"

#define CREATE_INDEX_GENRE_NAME       \
 "CREATE UNIQUE INDEX IF NOT EXISTS " \
 "genre_name_idx ON genre (genre_name);"

/******************************************************************************/
/*                                                                            */
/*                                  Select                                    */
/*                                                                            */
/******************************************************************************/

#define SELECT_FILE_FROM                  "SELECT file_id, file_path, "       \
                                          "file_meta_title, file_meta_year, " \
                                          "file_meta_track "                  \
                                          "FROM file "
#define SELECT_FILE_WHERE_                "WHERE "
#define SELECT_FILE_AND_                  "AND "
#define SELECT_FILE_WHERE_AUTHOR_NULL     "_author_id IS NULL "
#define SELECT_FILE_WHERE_ALBUM_NULL      "_album_id IS NULL "
#define SELECT_FILE_WHERE_GENRE_NULL      "_genre_id IS NULL "
#define SELECT_FILE_WHERE_AUTHOR_EQUAL    "_author_id = ? "
#define SELECT_FILE_WHERE_ALBUM_EQUAL     "_album_id = ? "
#define SELECT_FILE_WHERE_GENRE_EQUAL     "_genre_id = ? "
#define SELECT_FILE_WHERE_AUTHOR_NOTEQUAL "(_author_id != ? OR _author_id IS NULL) "
#define SELECT_FILE_WHERE_ALBUM_NOTEQUAL  "(_album_id != ? OR _album_id IS NULL) "
#define SELECT_FILE_WHERE_GENRE_NOTEQUAL  "(_genre_id != ? OR _genre_id IS NULL) "
#define SELECT_FILE_ORDER                 "ORDER BY file_meta_track, " \
                                          "file_meta_title;"


#define SELECT_AUTHOR \
  SELECT_AUTHOR_FROM  \
  SELECT_AUTHOR_ORDER

#define SELECT_AUTHOR_BY_ALBUM \
  SELECT_AUTHOR_FROM           \
  SELECT_AUTHOR_JOIN           \
  SELECT_AUTHOR_WHERE          \
  SELECT_AUTHOR_ORDER

#define SELECT_AUTHOR_FROM  "SELECT author.author_id, author_name FROM author "
#define SELECT_AUTHOR_JOIN  "INNER JOIN alloc_author_album " \
                            "ON author.author_id = alloc_author_album.author_id "
#define SELECT_AUTHOR_WHERE "WHERE alloc_author_album.album_id = ? "
#define SELECT_AUTHOR_ORDER "ORDER BY author_name;"


#define SELECT_ALBUM \
  SELECT_ALBUM_FROM  \
  SELECT_ALBUM_ORDER

#define SELECT_ALBUM_BY_AUTHOR \
  SELECT_ALBUM_FROM            \
  SELECT_ALBUM_JOIN_AUTHOR     \
  SELECT_ALBUM_WHERE_AUTHOR    \
  SELECT_ALBUM_ORDER

#define SELECT_ALBUM_BY_GENRE  \
  SELECT_ALBUM_FROM            \
  SELECT_ALBUM_JOIN_GENRE      \
  SELECT_ALBUM_WHERE_GENRE     \
  SELECT_ALBUM_ORDER

#define SELECT_ALBUM_FROM         "SELECT album.album_id, album_name FROM album "
#define SELECT_ALBUM_JOIN_AUTHOR  "INNER JOIN alloc_author_album " \
                                  "ON album.album_id = alloc_author_album.album_id "
#define SELECT_ALBUM_JOIN_GENRE   "INNER JOIN alloc_genre_album " \
                                  "ON album.album_id = alloc_genre_album.album_id "
#define SELECT_ALBUM_WHERE_AUTHOR "WHERE alloc_author_album.author_id = ? "
#define SELECT_ALBUM_WHERE_GENRE  "WHERE alloc_genre_album.genre_id = ? "
#define SELECT_ALBUM_ORDER        "ORDER BY album_name;"


#define SELECT_GENRE \
  SELECT_GENRE_FROM  \
  SELECT_GENRE_ORDER

#define SELECT_GENRE_FROM   "SELECT genre_id, genre_name FROM genre "
#define SELECT_GENRE_ORDER  "ORDER BY genre_name;"

/* Internal */

#define SELECT_FILE_MTIME \
 "SELECT file_mtime "     \
 "FROM file "             \
 "WHERE file_path = ?;"

#define SELECT_AUTHOR_ID \
 "SELECT author_id "     \
 "FROM author "          \
 "WHERE author_name = ?;"

#define SELECT_ALBUM_ID  \
 "SELECT album_id "      \
 "FROM album "           \
 "WHERE album_name = ?;"

#define SELECT_GENRE_ID  \
 "SELECT genre_id "      \
 "FROM genre "           \
 "WHERE genre_name = ?;"

#define SELECT_FILE_CHECKED_CLEAR \
 "SELECT file_path "              \
 "FROM file "                     \
 "WHERE checked__ = 0;"

/******************************************************************************/
/*                                                                            */
/*                                  Insert                                    */
/*                                                                            */
/******************************************************************************/

#define INSERT_FILE             \
 "INSERT "                      \
 "INTO file (file_path, "       \
 "           file_mtime, "      \
 "           file_meta_title, " \
 "           file_meta_year, "  \
 "           file_meta_track, " \
 "           checked__, "       \
 "           _author_id, "      \
 "           _album_id, "       \
 "           _genre_id) "       \
 "VALUES (?, ?, ?, ?, ?, 1, ?, ?, ?);"

#define INSERT_AUTHOR         \
 "INSERT "                    \
 "INTO author (author_name) " \
 "VALUES (?);"

#define INSERT_ALBUM        \
 "INSERT "                  \
 "INTO album (album_name) " \
 "VALUES (?);"

#define INSERT_ALLOC_AUTHOR_ALBUM                 \
 "INSERT "                                        \
 "INTO alloc_author_album (author_id, album_id) " \
 "VALUES (?, ?);"

#define INSERT_GENRE        \
 "INSERT "                  \
 "INTO genre (genre_name) " \
 "VALUES (?);"

#define INSERT_ALLOC_GENRE_ALBUM                \
 "INSERT "                                      \
 "INTO alloc_genre_album (genre_id, album_id) " \
 "VALUES (?, ?);"

/******************************************************************************/
/*                                                                            */
/*                                  Update                                    */
/*                                                                            */
/******************************************************************************/

#define UPDATE_FILE          \
 "UPDATE file "              \
 "SET file_mtime      = ?, " \
 "    file_meta_title = ?, " \
 "    file_meta_year  = ?, " \
 "    file_meta_track = ?, " \
 "    checked__       = 1, " \
 "    _author_id      = ?, " \
 "    _album_id       = ?, " \
 "    _genre_id       = ?  " \
 "WHERE file_path = ?;"

#define UPDATE_FILE_CHECKED_CLEAR \
 "UPDATE file "                   \
 "SET checked__ = 0;"

/******************************************************************************/
/*                                                                            */
/*                                  Delete                                    */
/*                                                                            */
/******************************************************************************/

#define DELETE_FILE  \
 "DELETE FROM file " \
 "WHERE file_path = ?;"

/* Cleanup */

#define CLEANUP_GENRE             \
 "DELETE FROM genre "             \
 "WHERE genre_id IN ( "           \
 "  SELECT genre_id "             \
 "  FROM genre "                  \
 "  LEFT OUTER JOIN file "        \
 "    ON genre_id = _genre_id "   \
 "  WHERE _genre_id IS NULL "     \
 ");"

#define CLEANUP_AUTHOR            \
 "DELETE FROM author "            \
 "WHERE author_id IN ( "          \
 "  SELECT author_id "            \
 "  FROM author "                 \
 "  LEFT OUTER JOIN file "        \
 "    ON author_id = _author_id " \
 "  WHERE _author_id IS NULL "    \
 ");"

#define CLEANUP_ALBUM             \
 "DELETE FROM album "             \
 "WHERE album_id IN ( "           \
 "  SELECT album_id "             \
 "  FROM album "                  \
 "  LEFT OUTER JOIN file "        \
 "    ON album_id = _album_id "   \
 "  WHERE _album_id IS NULL "     \
 ");"

#define CLEANUP_ALLOC_AUTHOR_ALBUM            \
 "DELETE FROM alloc_author_album "            \
 "WHERE author_id IN ( "                      \
 "  SELECT alloc.author_id "                  \
 "  FROM alloc_author_album AS alloc "        \
 "  LEFT OUTER JOIN author "                  \
 "    ON alloc.author_id = author.author_id " \
 "  WHERE author.author_id IS NULL "          \
 ") "                                         \
 "OR album_id IN ( "                          \
 "  SELECT alloc.album_id "                   \
 "  FROM alloc_author_album AS alloc "        \
 "  LEFT OUTER JOIN album "                   \
 "    ON alloc.album_id = album.album_id "    \
 "  WHERE album.album_id IS NULL "            \
 ");"

#define CLEANUP_ALLOC_GENRE_ALBUM             \
 "DELETE FROM alloc_genre_album "             \
 "WHERE genre_id IN ( "                       \
 "  SELECT alloc.genre_id "                   \
 "  FROM alloc_genre_album AS alloc "         \
 "  LEFT OUTER JOIN genre "                   \
 "    ON alloc.genre_id = genre.genre_id "    \
 "  WHERE genre.genre_id IS NULL "            \
 ") "                                         \
 "OR album_id IN ( "                          \
 "  SELECT alloc.album_id "                   \
 "  FROM alloc_genre_album AS alloc "         \
 "  LEFT OUTER JOIN album "                   \
 "    ON alloc.album_id = album.album_id "    \
 "  WHERE album.album_id IS NULL "            \
 ");"

#endif /* VALHALLA_SQL_STATEMENTS_H */
