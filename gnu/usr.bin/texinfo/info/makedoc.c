/* makedoc.c -- Make doc.c and funs.h from input files.
   $Id: makedoc.c,v 1.2 1999/01/11 16:38:08 espie Exp $

   This file is part of GNU Info, a program for reading online documentation
   stored in Info format.

   Copyright (C) 1993, 97 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

/* This program grovels the contents of the source files passed as arguments
   and writes out a file of function pointers and documentation strings, and
   a header file which describes the contents.  This only does the functions
   declared with DECLARE_INFO_COMMAND. */

#include "info.h"

static void fatal_file_error ();

/* Name of the header file which receives the declarations of functions. */
static char *funs_filename = "funs.h";

/* Name of the documentation to function pointer file. */
static char *doc_filename = "doc.c";

static char *doc_header[] = {
  "/* doc.c -- Generated structure containing function names and doc strings.",
  "",
  "   This file was automatically made from various source files with the",
  "   command \"%s\".  DO NOT EDIT THIS FILE, only \"%s.c\".",
  (char *)NULL
};

static char *doc_header_1[] = {
  "   An entry in the array FUNCTION_DOC_ARRAY is made for each command",
  "   found in the above files; each entry consists of a function pointer,",
#if defined (NAMED_FUNCTIONS)
  "   a string which is the user-visible name of the function,",
#endif /* NAMED_FUNCTIONS */
  "   and a string which documents its purpose. */",
  "",
  "#include \"doc.h\"",
  "#include \"funs.h\"",
  "",
  "FUNCTION_DOC function_doc_array[] = {",
  "",
  (char *)NULL
};

/* How to remember the locations of the functions found so that Emacs
   can use the information in a tag table. */
typedef struct {
  char *name;                   /* Name of the tag. */
  int line;                     /* Line number at which it appears. */
  long char_offset;             /* Character offset at which it appears. */
} EMACS_TAG;

typedef struct {
  char *filename;               /* Name of the file containing entries. */
  long entrylen;                /* Total number of characters in tag block. */
  EMACS_TAG **entries;          /* Entries found in FILENAME. */
  int entries_index;
  int entries_slots;
} EMACS_TAG_BLOCK;

EMACS_TAG_BLOCK **emacs_tags = (EMACS_TAG_BLOCK **)NULL;
int emacs_tags_index = 0;
int emacs_tags_slots = 0;

#define DECLARATION_STRING "\nDECLARE_INFO_COMMAND"

static void process_one_file ();
static void maybe_dump_tags ();
static FILE *must_fopen ();

int
main (argc, argv)
     int argc;
     char **argv;
{
  register int i;
  int tags_only = 0;
  FILE *funs_stream, *doc_stream;

  for (i = 1; i < argc; i++)
    if (strcmp (argv[i], "-tags") == 0)
      {
        tags_only++;
        break;
      }

  if (tags_only)
    {
      funs_filename = "/dev/null";
      doc_filename = "/dev/null";
    }
  
  funs_stream = must_fopen (funs_filename, "w");
  doc_stream = must_fopen (doc_filename, "w");

  fprintf (funs_stream,
           "/* %s -- Generated declarations for Info commands. */\n",
           funs_filename);

  for (i = 0; doc_header[i]; i++)
    {
      fprintf (doc_stream, doc_header[i], argv[0], argv[0]);
      fprintf (doc_stream, "\n");
    }

  fprintf (doc_stream,
           _("   Source files groveled to make this file include:\n\n"));

  for (i = 1; i < argc; i++)
    fprintf (doc_stream, "\t%s\n", argv[i]);

  fprintf (doc_stream, "\n");

  for (i = 0; doc_header_1[i]; i++)
    fprintf (doc_stream, "%s\n", doc_header_1[i]);


  for (i = 1; i < argc; i++)
    {
      char *curfile;
      curfile = argv[i];

      if (*curfile == '-')
        continue;

      fprintf (doc_stream, "/* Commands found in \"%s\". */\n", curfile);
      fprintf (funs_stream, "\n/* Functions declared in \"%s\". */\n",
               curfile);

      process_one_file (curfile, doc_stream, funs_stream);
    }

  fprintf (doc_stream,
           "   { (VFunction *)NULL, (char *)NULL, (char *)NULL }\n};\n");

  fclose (funs_stream);
  fclose (doc_stream);

  if (tags_only)
    maybe_dump_tags (stdout);
  exit (0);
}

/* Dumping out the contents of an Emacs tags table. */
static void
maybe_dump_tags (stream)
     FILE *stream;
{
  register int i;

  /* Print out the information for each block. */
  for (i = 0; i < emacs_tags_index; i++)
    {
      register int j;
      register EMACS_TAG_BLOCK *block;
      register EMACS_TAG *etag;
      long block_len;

      block_len = 0;
      block = emacs_tags[i];

      /* Calculate the length of the dumped block first. */
      for (j = 0; j < block->entries_index; j++)
        {
          char digits[30];
          etag = block->entries[j];
          block_len += 3 + strlen (etag->name);
          sprintf (digits, "%d,%ld", etag->line, etag->char_offset);
          block_len += strlen (digits);
        }

      /* Print out the defining line. */
      fprintf (stream, "\f\n%s,%ld\n", block->filename, block_len);

      /* Print out the individual tags. */
      for (j = 0; j < block->entries_index; j++)
        {
          etag = block->entries[j];

          fprintf (stream, "%s,\177%d,%ld\n",
                   etag->name, etag->line, etag->char_offset);
        }
    }
}

/* Keeping track of names, line numbers and character offsets of functions
   found in source files. */
static EMACS_TAG_BLOCK *
make_emacs_tag_block (filename)
     char *filename;
{
  EMACS_TAG_BLOCK *block;

  block = (EMACS_TAG_BLOCK *)xmalloc (sizeof (EMACS_TAG_BLOCK));
  block->filename = xstrdup (filename);
  block->entrylen = 0;
  block->entries = (EMACS_TAG **)NULL;
  block->entries_index = 0;
  block->entries_slots = 0;
  return (block);
}

static void
add_tag_to_block (block, name, line, char_offset)
     EMACS_TAG_BLOCK *block;
     char *name;
     int line;
     long char_offset;
{
  EMACS_TAG *tag;

  tag = (EMACS_TAG *)xmalloc (sizeof (EMACS_TAG));
  tag->name = name;
  tag->line = line;
  tag->char_offset = char_offset;
  add_pointer_to_array (tag, block->entries_index, block->entries,
                        block->entries_slots, 50, EMACS_TAG *);
}

/* Read the file represented by FILENAME into core, and search it for Info
   function declarations.  Output the declarations in various forms to the
   DOC_STREAM and FUNS_STREAM. */
static void
process_one_file (filename, doc_stream, funs_stream)
     char *filename;
     FILE *doc_stream, *funs_stream;
{
  int descriptor, decl_len;
  char *buffer, *decl_str;
  struct stat finfo;
  long offset;
  long file_size;
  EMACS_TAG_BLOCK *block;

  if (stat (filename, &finfo) == -1)
    fatal_file_error (filename);

  descriptor = open (filename, O_RDONLY, 0666);

  if (descriptor == -1)
    fatal_file_error (filename);

  file_size = (long) finfo.st_size;
  buffer = (char *)xmalloc (1 + file_size);
  read (descriptor, buffer, file_size);
  close (descriptor);

  offset = 0;
  decl_str = DECLARATION_STRING;
  decl_len = strlen (decl_str);

  block = make_emacs_tag_block (filename);

  while (1)
    {
      long point = 0;
      long line_start = 0;
      int line_number = 0;

      char *func, *doc;
#if defined (NAMED_FUNCTIONS)
      char *func_name;
#endif /* NAMED_FUNCTIONS */

      for (; offset < (file_size - decl_len); offset++)
        {
          if (buffer[offset] == '\n')
            {
              line_number++;
              line_start = offset + 1;
            }

          if (strncmp (buffer + offset, decl_str, decl_len) == 0)
            {
              offset += decl_len;
              point = offset;
              break;
            }
        }

      if (!point)
        break;

      /* Skip forward until we find the open paren. */
      while (point < file_size)
        {
          if (buffer[point] == '\n')
            {
              line_number++;
              line_start = point + 1;
            }
          else if (buffer[point] == '(')
            break;

          point++;
        }

      while (point++ < file_size)
        {
          if (!whitespace_or_newline (buffer[point]))
            break;
          else if (buffer[point] == '\n')
            {
              line_number++;
              line_start = point + 1;
            }
        }

      if (point >= file_size)
        break;

      /* Now looking at name of function.  Get it. */
      for (offset = point; buffer[offset] != ','; offset++);
      func = (char *)xmalloc (1 + (offset - point));
      strncpy (func, buffer + point, offset - point);
      func[offset - point] = '\0';

      /* Remember this tag in the current block. */
      {
        char *tag_name;

        tag_name = (char *)xmalloc (1 + (offset - line_start));
        strncpy (tag_name, buffer + line_start, offset - line_start);
        tag_name[offset - line_start] = '\0';
        add_tag_to_block (block, tag_name, line_number, point);
      }

#if defined (NAMED_FUNCTIONS)
      /* Generate the user-visible function name from the function's name. */
      {
        register int i;
        char *name_start;

        name_start = func;

        if (strncmp (name_start, "info_", 5) == 0)
          name_start += 5;

        func_name = xstrdup (name_start);

        /* Fix up "ea" commands. */
        if (strncmp (func_name, "ea_", 3) == 0)
          {
            char *temp_func_name;

            temp_func_name = (char *)xmalloc (10 + strlen (func_name));
            strcpy (temp_func_name, "echo_area_");
            strcat (temp_func_name, func_name + 3);
            free (func_name);
            func_name = temp_func_name;
          }

        for (i = 0; func_name[i]; i++)
          if (func_name[i] == '_')
            func_name[i] = '-';
      }
#endif /* NAMED_FUNCTIONS */

      /* Find doc string. */
      point = offset + 1;

      while (point < file_size)
        {
          if (buffer[point] == '\n')
            {
              line_number++;
              line_start = point + 1;
            }

          if (buffer[point] == '"')
            break;
          else
            point++;
        }

      offset = point + 1;

      while (offset < file_size)
        {
          if (buffer[offset] == '\n')
            {
              line_number++;
              line_start = offset + 1;
            }

          if (buffer[offset] == '\\')
            offset += 2;
          else if (buffer[offset] == '"')
            break;
          else
            offset++;
        }

      offset++;
      if (offset >= file_size)
        break;

      doc = (char *)xmalloc (1 + (offset - point));
      strncpy (doc, buffer + point, offset - point);
      doc[offset - point] = '\0';

#if defined (NAMED_FUNCTIONS)
      fprintf (doc_stream, "   { %s, \"%s\", %s },\n", func, func_name, doc);
      free (func_name);
#else /* !NAMED_FUNCTIONS */
      fprintf (doc_stream, "   { %s, %s },\n", func, doc);
#endif /* !NAMED_FUNCTIONS */

      fprintf (funs_stream, "extern void %s ();\n", func);
      free (func);
      free (doc);
    }
  free (buffer);

  /* If we created any tags, remember this file on our global list.  Otherwise,
     free the memory already allocated to it. */
  if (block->entries)
    add_pointer_to_array (block, emacs_tags_index, emacs_tags,
                          emacs_tags_slots, 10, EMACS_TAG_BLOCK *);
  else
    {
      free (block->filename);
      free (block);
    }
}

static void
fatal_file_error (filename)
     char *filename;
{
  fprintf (stderr, _("Couldn't manipulate the file %s.\n"), filename);
  exit (2);
}

static FILE *
must_fopen (filename, mode)
     char *filename, *mode;
{
  FILE *stream;

  stream = fopen (filename, mode);
  if (!stream)
    fatal_file_error (filename);

  return (stream);
}

