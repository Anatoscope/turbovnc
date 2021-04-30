/*
 *  Copyright (C) 2021 AnatoScope SA.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 */

#ifndef __RFBPLAINJSON_H__
#define __RFBPLAINJSON_H__

/*
 * Parse from file a JSON object that has string key-value pairs.
 * Caller must free all the returned strings after use.
 *
 * @param fileName name of the JSON file to parse (must contain an object with string key-value pairs)
 * @param keys a NULL-terminated array of pointers to keys to extract
 * @return an array of pointers to the values (caller myst free the values and the array)
 */
char** rfbPlainJsonParseFile(const char *fileName, const char **keys);

#endif  /* __RFBPLAINJSON_H__ */
