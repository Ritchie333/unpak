// unpak.c - program to decompress a Quake .PAK file and write out its contents

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct pak_header_s
{
  char id[4];
  int offset;
  int size;
} pak_header_t;

typedef struct pak_file_s
{
  char name[56];
  int offset;
  int size;
} pak_file_t;

void pak_enum(const char *pak_filename, int (* callback)( const char*, const char*, const int ))
{
  FILE* fp = fopen( pak_filename, "rb" );
  if( fp ) {
    pak_header_t pak_header;
    if( fread( &pak_header, sizeof( pak_header ), 1, fp ) ) {
      if( !memcmp( pak_header.id, "PACK", 4 ) ) {
        const int num_files = pak_header.size / sizeof( pak_file_t );
        if (!fseek(fp, pak_header.offset, SEEK_SET) ) {
          for( int i = 0; i < num_files; i++ ) {
            pak_file_t pak_file;
            if( fread( &pak_file, sizeof( pak_file_t ), 1, fp ) ) {
              callback( pak_filename, pak_file.name, pak_file.size );
            }
          }
        } 
      }
    }
    fclose( fp );
  }
}

/* pak_filename : the os filename of the .pak file */
/* filename     : the name of the file you're trying to load from the .pak file (remember to use forward slashes for the path) */
/* out_filesize : if not null, the loaded file's size will be returned here */
/* returns a malloced buffer containing the file contents (remember to free it later), or NULL if any error occurred */
void *pak_load_file(const char *pak_filename, const char *filename, int *out_filesize)
{
  FILE *fp;
  pak_header_t pak_header;
  int num_files;
  int i;
  pak_file_t pak_file;
  void *buffer;

  fp = fopen(pak_filename, "rb");
  if (!fp) 
    return NULL;

  if (!fread(&pak_header, sizeof(pak_header), 1, fp))
    goto pak_error;
  if (memcmp(pak_header.id, "PACK", 4) != 0)
    goto pak_error;

  num_files = pak_header.size / sizeof(pak_file_t);

  if (fseek(fp, pak_header.offset, SEEK_SET) != 0)
    goto pak_error;

  for (i = 0; i < num_files; i++)
  {
    if (!fread(&pak_file, sizeof(pak_file_t), 1, fp))
      goto pak_error;

    if (!strcmp(pak_file.name, filename))
    {
      if (fseek(fp, pak_file.offset, SEEK_SET) != 0)
        goto pak_error;

      buffer = malloc(pak_file.size);
      if (!buffer)
        goto pak_error;

      if (!fread(buffer, pak_file.size, 1, fp))
      {
        free(buffer);
        goto pak_error;
      }

      if (out_filesize)
        *out_filesize = pak_file.size;
      fclose( fp );
      return buffer;
    }
  }

pak_error:
  fclose(fp);
  return NULL;
}

void dirname( const char* path, char* result, size_t size )
{
  const char* ch = path + strlen( path );
  for( ; ch > path; ch-- ) {
    if( '/' == *ch ) {
      break;
    } 
  }
  size_t i = 0;
  for( const char* dest = path; i + 1 < size && dest < ch; dest++ ) {
    result[ i++ ] = *dest;
  }
  result[ i ] = 0;
}

int on_enum_pak( const char* pak_filename, const char* filename, const int size )
{
  void* buf = pak_load_file( pak_filename, filename, NULL );
  if( buf )  {
    char pak_dir[ 256 ];
    dirname( pak_filename, pak_dir, 256 );
    if ( !*pak_dir ) {
      strcpy( pak_dir, "." );
    }

    char sub_dir[ 256 ];
    strcpy( sub_dir, pak_dir );
    strcat( sub_dir, "/" );
    
    char file_dir[ 256 ];
    dirname( filename, file_dir, 256 );
    strcat( sub_dir, file_dir );
    DIR* dir = opendir( sub_dir );
    if( dir ) {
      closedir( dir );
    } else if (ENOENT == errno ) {
      printf( "mkdir %s\n", sub_dir );
      mkdir( sub_dir, S_IRWXU );
    }

    char path[ 256 ];
    strcpy( path, pak_dir );
    strcat( path, "/" );
    strcat( path, filename );
    printf( "%s\n", path );
    FILE* fp = fopen( path, "wb" );
    if( fp ) {
      fwrite( buf, size, 1, fp );
      fclose( fp );
    } else {
      fprintf( stderr, "Unable to write to %s : %d\n", path, errno );
    }
    free( buf );
  }
  return 1;
}

int main(int argc, char* argv[])
{
  if( argc < 2 ) {
    printf( "Usage : unpak [pak_filename]\n" );
    return 0;
  }
  const char* pak_filename = argv[ 1 ];
  pak_enum( pak_filename, on_enum_pak );  
  return 0;
}
