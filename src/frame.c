#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>

#include "ds_str.h"
#include "frm.h"

/* **********************************************************
 * Command line handling:
 *    prog [options] <command> [options] <subcommand> ...
 *
 * Essentially, options can come before commands, after commands
 * or both before and after commands, and multiple subcommands
 * may be present, with options once again appearing anywhere.
 */

// All options, separated by 0xfe
static char *g_options = NULL;
static void cline_parse_options (int argc, char **argv)
{
   for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-' && argv[i][1] == '-') {
         char *option = &argv[i][2];
         ds_str_append (&g_options, "\xfe", option, NULL);
      }
   }
   ds_str_append (&g_options, "\xfe", NULL);
}

static char *cline_option_get (const char *name)
{
   char *sterm = ds_str_cat ("\xfe", name, NULL);
   if (!sterm) {
      fprintf (stderr, "OOM error creating option search term\n");
      return NULL;
   }
   char *position = strstr (g_options, sterm);
   // Option not found
   if (!position) {
      free (sterm);
      return NULL;
   }

   position += strlen (sterm);

   // Option found, no value found
   if (*position != '=') {
      free (sterm);
      return ds_str_dup ("");
   }

   // Option found, value found
   position++;
   char *end = strchr (position, '\xfe');
   if (!end) {
      fprintf (stderr, "Internal error, no option delimiter xfe found\n");
      free (sterm);
      return NULL;
   }

   size_t val_len = (end - position) + 1;
   char *value = calloc (val_len, 1);
   if (!value) {
      fprintf (stderr, "OOM error creating option value\n");
      free (sterm);
      return NULL;
   }

   memcpy (value, position, val_len - 1);
   free (sterm);
   return value;
}


// All commands, separated by 0xfe
static char *g_commands = NULL;
static void cline_parse_commands (int argc, char **argv)
{
   for (int i=1; i<argc; i++) {
      if (argv[i][0] != '-' && argv[i][1] != '-') {
         ds_str_append (&g_commands, argv[i], "\xfe", NULL);
      }
   }
   ds_str_append (&g_commands, "\xfe", NULL);
}

static char *cline_command_get (size_t index)
{
   char *cmd = g_commands;
   for (size_t i=0; i<index; i++) {
      cmd = strchr (cmd, '\xfe');
      if (!cmd)
         break;
   }
   if (!cmd) {
      fprintf (stderr, "Request for command [%zu] failed\n", index);
      return NULL;
   }

   if (*cmd == '\xfe')
      cmd++;

   char *end = strchr (cmd, '\xfe');
   if (!end) {
      fprintf (stderr, "Internal error, no command delimiter xfe found\n");
      return NULL;
   }

   size_t cmd_len = (end - cmd) + 1;
   char *ret = calloc (cmd_len, 1);
   if (!ret) {
      fprintf (stderr, "OOM error returning command\n");
      return NULL;
   }

   memcpy (ret, cmd, cmd_len - 1);
   return ret;
}

static char edlin[1024 * 1024];
static char *run_editor (void)
{
   char *message = NULL;
   char *editor = getenv ("EDITOR");
   if (!editor || !editor[0]) {
      FRM_ERROR ("Warning: no $EDITOR specified.\n");
      printf ("Enter the message, ending with a single period "
               "on a line by itself\n");
      while ((fgets (edlin, sizeof edlin -1, stdin))!=NULL) {
         if (edlin[0] == '.')
            break;
         if (!(ds_str_append (&message, edlin, NULL))) {
            FRM_ERROR ("OOM reading edlin input\n");
            free (message);
            return NULL;
         }
      }
   } else {
      char fname[] = "frame-tmpfile-XXXXXX";
      int fd = mkstemp (fname);
      if (fd < 0) {
         FRM_ERROR ("Failed to create temporary file: %m\n");
         return NULL;
      }
      close (fd);
      char *shcmd = ds_str_cat (editor, " ", fname, NULL);
      printf ("Waiting for [%s] to return\n", shcmd);
      int exitcode = system (shcmd);
      free (shcmd);
      if (exitcode != 0) {
         FRM_ERROR ("Editor aborted, aborting: %m\n");
         return NULL;
      }
      if (!(frm_writefile (fname,
                  "\n",
                  "Replace this content with your message.",
                  "\n",
                  "There is no limit on the length of messages\n",
                  NULL))) {
         FRM_ERROR ("Failed to edit temporary file [%s]: %m\n", fname);
         return NULL;
      }
      message = frm_readfile (fname);
      if (!message) {
         FRM_ERROR ("Failed to read editor output, aborting\n");
         return NULL;
      }
   }
   return message;
}

static void print_helpmsg (void)
{
   fprintf (stderr, "TODO: The help message\n");
}


int main (int argc, char **argv)
{
   int ret = EXIT_SUCCESS;
   cline_parse_options (argc, argv);
   cline_parse_commands (argc, argv);

   // TODO: At some point maybe verify that the options specified are applicable
   // to the command.
   char *command = cline_command_get (0);
   char *help = cline_option_get ("help");
   char *dbpath = cline_option_get ("dbpath");
   char *message = cline_option_get ("message");
   frm_t *frm = NULL;

   if (!command || !command[0]) {
      print_helpmsg ();
      ret = EXIT_FAILURE;
      goto cleanup;
   }

   if (!dbpath) {
      // TODO: Windows compatibility
      const char *home = getenv("HOME");
      if (!home || !home[0]) {
         fprintf (stderr, "No --dbpath specified and $HOME is not set\n");
         ret = EXIT_FAILURE;
         goto cleanup;
      }
      dbpath = ds_str_cat (home, "/.framedb", NULL);
      if (!dbpath) {
         fprintf (stderr, "OOM error copying $HOME\n");
         ret = EXIT_FAILURE;
         goto cleanup;
      }
   }

   // Check for each command in turn. Could be done in an array, but I don't care
   // enough to do it.
   if ((strcmp (command, "init"))==0) {
      if ((frm = frm_create (dbpath))) {
         printf ("Initialised framedb at [%s]\n", dbpath);
         ret = EXIT_SUCCESS;
      } else {
         fprintf (stderr, "Failed to initialise framedb at [%s]: %m\n", dbpath);
         ret = EXIT_FAILURE;
      }
      goto cleanup;
   }

   if (!(frm = frm_init (dbpath))) {
      fprintf (stderr, "Failed to load db from [%s]\n", dbpath);
      ret = EXIT_FAILURE;
      goto cleanup;
   }

   if ((strcmp (command, "history"))==0) {
      char *history = frm_history (frm, 10);
      char *sptr = NULL;
      char *tok = strtok_r (history, "\n", &sptr);
      size_t i=0;
      printf ("Frame history\n");
      char indicator = '*';
      do {
         printf ("%c  %5zu: %s\n", indicator, i, tok);
         indicator = ' ';
      } while ((tok = strtok_r (NULL, "\n", &sptr)));
      printf ("\n");
      free (history);
      goto cleanup;
   }

   if ((strcmp (command, "status"))==0) {
      char *current = frm_current (frm);
      char *payload = frm_payload (frm);
      char *mtime = frm_date_str (frm);

      printf ("Current frame\n   %s\n", current);
      printf ("\nNotes (%s)\n", mtime);
      char *sptr = NULL;
      char *tok = strtok_r (payload, "\n", &sptr);
      do {
         printf ("   %s\n", tok);
      } while ((tok = strtok_r (NULL, "\n", &sptr)));
      printf ("\n");
      free (current);
      free (mtime);
      free (payload);
      goto cleanup;
   }

   if ((strcmp (command, "push"))==0) {
      char *name = cline_command_get(1);
      if (!name || !name[0]) {
         fprintf (stderr, "Must specify a name for the new frame being pushed\n");
         free (name);
         ret = EXIT_FAILURE;
         goto cleanup;
      }
      char *message = cline_option_get ("message");
      if (!message) {
         message = run_editor ();
      }

      if (!(frm_push (frm, name, message))) {
         fprintf (stderr, "Failed to create new frame\n");
         free (name);
         free (message);
         ret = EXIT_FAILURE;
         goto cleanup;
      }
      free (name);
      free (message);
      name = frm_current (frm);
      printf ("Created new frame [%s]\n", name);
      free (name);
      goto cleanup;
   }

   // The default, with no arguments, is to print out the help message.
   // If we got to this point we have a command but it is unrecognised.
   fprintf (stderr, "Unrecognised command [%s]\n", command);
   ret = EXIT_FAILURE;

cleanup:
   frm_close (frm);
   free (command);
   free (help);
   free (dbpath);
   free (message);
   free (g_options);
   free (g_commands);
   return ret;
}

