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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "rfb.h"

enum { BLOCK_SIZE = 1024 };

typedef struct {
  char *pRead; /* points to the char after last block read */
  char *pBegin; /* points to the current char */
  char *pEnd; /* points to the char after the extracted word */
  size_t processedLen; /* number of chars that went through before the last read */
  char buff[BLOCK_SIZE];
} rfbPlainJsonParser;

static void rfbPlainJsonParserInit(rfbPlainJsonParser *parser)
{
  parser->pRead = parser->buff;
  parser->pBegin = parser->buff;
  parser->pEnd = parser->buff;
  parser->processedLen = 0;
}

static size_t rfbPlainJsonParserCurrPos(rfbPlainJsonParser *parser)
{
  return parser->pBegin - parser->buff + parser->processedLen;
}

static int rfbPlainJsonParserSkipWhitespace(rfbPlainJsonParser *parser, FILE *f, const char *fileName)
{
  int eof = 0;

  while (1) {
    for (; parser->pBegin < parser->pRead && isspace(*parser->pBegin); ++parser->pBegin)
      ;

    if (parser->pBegin != parser->pRead)
      return 1;

    if (eof)
      break;

    parser->processedLen += parser->pRead - parser->buff;
    const size_t len = fread(parser->buff, 1, BLOCK_SIZE, f);
    if (len != BLOCK_SIZE) {
      if (ferror(f) != 0) {
        return 0;
      } else {
        eof = feof(f);
      }
    }
    parser->pRead = parser->buff + len;
    parser->pBegin = parser->buff;
  }

  rfbLog("WARNING: JSON expected a non-whitespace, file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(parser));
  return 0;
}

static int rfbPlainJsonParserExtractUntil(rfbPlainJsonParser *parser, FILE *f, char c)
{
  parser->pEnd = parser->pBegin;

  int eof = 0;

  while (1) {
    for (; parser->pEnd < parser->pRead && *parser->pEnd != c; ++parser->pEnd)
      ;

    if (parser->pEnd != parser->pRead)
      return 1;

    if (eof)
      break;

    const size_t shiftLen = parser->pBegin - parser->buff;
    const size_t toReadLen = BLOCK_SIZE - (parser->pEnd - parser->pBegin);

    if (!toReadLen)
      return 0;

    /* Shift useful contents to the beginning. */
    if (shiftLen) {
      for (size_t i = 0; i != shiftLen; ++i)
        parser->buff[i] = parser->buff[i + shiftLen];
      parser->pRead -= shiftLen;
      parser->pBegin -= shiftLen;
      parser->pEnd -= shiftLen;
    }

    parser->processedLen += shiftLen;

    const size_t len = fread(parser->pRead, 1, toReadLen, f);
    if (len != toReadLen) {
      if (ferror(f) != 0) {
        return 0;
      } else {
        eof = feof(f);
      }
    }
    parser->pRead += len;
  }

  return 0;
}

char** rfbPlainJsonParseFile(const char *fileName, const char **keys)
{
  assert(fileName);
  assert(keys);

  FILE *f = fopen(fileName, "r");
  if (!f) {
    rfbLog("WARNING: Unable to open file '%s'\n", fileName);
    return NULL;
  }

  const char **keysPtr = keys;
  while (*keysPtr)
    ++keysPtr;
  const size_t keysCount = keysPtr - keys;

  char **result = calloc(keysCount + 1, sizeof(*keys));

  do { /* do {...} while(0) - single iteration loop */

    if (!result) {
      rfbLogPerror("Parsing JSON keys, out of memory");
      break;
    }

    rfbPlainJsonParser parser;
    rfbPlainJsonParserInit(&parser);

    if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
      break;

    if (*parser.pBegin++ != '{') {
      rfbLog("WARNING: JSON expected '{', file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
      break;
    }

    if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
      break;

    if (*parser.pBegin == '}') {
      fclose(f);
      return result;
    }

    while (1) {
      if (*parser.pBegin++ != '"') {
        rfbLog("WARNING: JSON expected '\"', file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      if (!rfbPlainJsonParserExtractUntil(&parser, f, '"')) {
        rfbLog("WARNING: JSON expected keystring, file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      size_t keyIndex = 0;

      {
        const size_t keyLen = parser.pEnd - parser.pBegin;
        while (keyIndex != keysCount &&
            (strncmp(keys[keyIndex], parser.pBegin, keyLen) != 0 || keys[keyIndex][keyLen]))
          ++keyIndex;
      }

      parser.pBegin = parser.pEnd + 1;

      if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
        break;

      if (*parser.pBegin++ != ':') {
        rfbLog("WARNING: JSON expected ':', file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
        break;

      if (*parser.pBegin++ != '"') {
        rfbLog("WARNING: JSON expected '\"', file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      if (!rfbPlainJsonParserExtractUntil(&parser, f, '"')) {
        rfbLog("WARNING: JSON expected valuestring, file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      if (keyIndex != keysCount) {
        if (result[keyIndex]) {
          rfbLog("WARNING: JSON duplicate key, file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
          break;
        }

        const size_t valLen = parser.pEnd - parser.pBegin;
        result[keyIndex] = malloc(valLen + 1);

        if (!result[keyIndex]) {
          rfbLogPerror("Parsing JSON keys, out of memory");
          break;
        }

        memcpy(result[keyIndex], parser.pBegin, valLen);
        result[keyIndex][valLen] = '\0';
      }

      parser.pBegin = parser.pEnd + 1;

      if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
        break;

      if (*parser.pBegin == '}') {
        fclose(f);
        return result;
      }

      if (*parser.pBegin++ != ',') {
        rfbLog("WARNING: JSON expected ',', file '%s', position %zu\n", fileName, rfbPlainJsonParserCurrPos(&parser));
        break;
      }

      if (!rfbPlainJsonParserSkipWhitespace(&parser, f, fileName))
        break;
    }
  } while(0);

  if (result)
    for (size_t i = 0; i != keysCount; ++i)
      free(result[i]);
  free(result);
  fclose(f);

  return NULL;
}
