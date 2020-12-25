#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
  buffer_cache_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_flush_all ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  int name_length = strlen(name);
  char directory[name_length];
  char file_name[name_length];

  split_path(name, directory, file_name);

  struct dir *dir = dir_open_path (directory);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  int name_length = strlen(name);
  if (name_length == 0)
  {
    return NULL;
  }
  char directory[name_length + 1];
  char file_name[name_length + 1];

  split_path(name, directory, file_name);
  struct dir *dir = dir_open_path (directory);
  struct inode *inode = NULL;

  if (dir != NULL)
  {
    if (strlen(file_name) > 0)
    {
      dir_lookup (dir, file_name, &inode);
      dir_close (dir);
    }
    else
    {
      inode = dir_get_inode (dir);
    }
  }

  if (inode == NULL || inode_is_removed(inode))
  {
    return NULL;
  }

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  int name_length = strlen(name);
  char directory[name_length];
  char file_name[name_length];

  split_path(name, directory, file_name);
  struct dir *dir = dir_open_path (directory);
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* path spliting */
void
split_path(const char* path, char *dir, char *name)
{
  int path_length = strlen(path);
  char* path_copy = (char*)malloc(sizeof(char) * (path_length + 1));
  memcpy (path_copy, path, sizeof(char) * (path_length + 1));

  char* dir_iter = dir;
  if (dir_iter && path_copy[0] == '/')
  {
    memcpy (dir_iter, "/", sizeof(char));
    dir_iter ++;
  }

  char *token, *save_ptr;
  char *last_token = "";

  for (token = strtok_r (path_copy, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
    {
      int last_length = strlen(last_token);
      if (dir_iter && last_length > 0)
      {
        memcpy (dir_iter, last_token, sizeof(char) * last_length);
        dir_iter += last_length;
        memcpy (dir_iter, "/", sizeof(char));
        dir_iter ++;
      }

      last_token = token;
    }

  if (dir_iter)
  {
    memcpy (dir_iter, "\0", sizeof(char));
  }

  memcpy(name, last_token, sizeof(char) * (strlen(last_token) + 1));
}
