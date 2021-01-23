/*
    Name: David Trimino
    Student ID: 1001659459
*/

// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#define MAX_NUM_ARGUMENTS 4

char  BS_OEMName[8];
uint16_t BPB_BytesPerSec;
uint8_t  BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t  BPB_NumFat;
uint16_t BPB_RootEntCnt;
char    BS_ColLab[11];
uint32_t BPB_FATSz32;
uint32_t BPB_RootClus;

uint32_t RootDirSectors = 0;
uint32_t FirstDataSector= 0;
uint32_t FirstSectorofCluster = 0;

 FILE *fp;

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

 struct __attribute__((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];
/*
 parameter the current sector number that points at block of data
 Returns: The value of the address for that block of data
 Description: Find the starting address of a block of data given the sector number
 corresponding to that data block.
*/

int LBAToOffset(int32_t sector)
{
  return ((sector - 2 ) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFat * BPB_FATSz32 * BPB_BytesPerSec);
}


int matching (char* input)
{
  int location;
  int result = -100;
  int i;
  for( i = 0; i < 11; i++ )
  {
    input[i] = toupper( input[i] );
  }
  for (location  = 0; location < 16; location ++)
  {
    if(dir[location].DIR_Attr == 0x01 || dir[location].DIR_Attr == 0x10 || dir[location].DIR_Attr == 0x20)
    {
        char *string = strtok(input," ");
        char filename[12];
        strcpy(filename,dir[location].DIR_Name);
        char *token = strtok(filename," ");
        if(string)
        {
          if(strcmp(string,token)==0)
          {
            result = location;
          }
        }
        else
        {
          char *string = strtok(input," ");
          if(strcmp(string,token)==0)
          {
            result = location;
          }
        }

    }
  }
  return result;
}


int16_t NextLB(uint32_t sector)
{
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp ,FATAddress, SEEK_SET);
  fread (&val,2,1,fp);
  return val;
}



void getFile( char * fileName, char * newFileName )
{
  FILE * ofp;

  if( newFileName == NULL )
  {
    ofp = fopen( fileName, "w" );
    if( ofp == NULL )
    {
      printf( "Error: Could not open file %s\n", fileName );
    }
  }
  else
  {
    ofp = fopen( newFileName, "w" );
    if( ofp == NULL )
    {
      printf( "Error: Could not open file %s\n", newFileName );
    }
  }

  int i;
  int found = -100;
  char str[12];
  strncpy(str , fileName, strlen(fileName));
  char *string = strtok(str,".");
  found = matching (string);
  if(found == -100)
  {
    char str[16];
    char result2[16];
    strncpy(str , fileName , strlen(fileName));
    char *string1 = strtok(str, ".");
    char *string2 = strtok(NULL, "\n");;
    if(string2)
    {
      strcpy(result2,string1);
      strcat(result2,string2);
      found = matching(result2);
    }
  }
  if(found != -100)
  {
      int cluster = dir[found].DIR_FirstClusterLow;

      int bytesRemainingToRead = dir[found].DIR_FileSize;
      int offset = 0;
      unsigned char buffer[512];

      while( bytesRemainingToRead >= BPB_BytesPerSec )
      {
        offset = LBAToOffset( cluster );
        fseek( fp, offset, SEEK_SET );
        fread( buffer, 1, BPB_BytesPerSec, fp );

        fwrite( buffer, 1, 512, ofp );

        cluster = NextLB( cluster );

        bytesRemainingToRead -= BPB_BytesPerSec;
      }

      if( bytesRemainingToRead )
      {
        offset = LBAToOffset( cluster );
        fseek( fp, offset, SEEK_SET );
        fread( buffer, 1, bytesRemainingToRead, fp );

        fwrite( buffer, 1, bytesRemainingToRead, ofp );
      }

  }
  else
  {
    printf("NO FILE FOUND \n");
  }
  fclose( ofp );
}




int main(int argc,char ** argv)
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  char string [100];

  int count = 0;
  char input[100];
  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");
    int count = 0;
    int i;
    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
    if(strcmp(cmd_str,"\n") == 0)
    {
        // keep looping until its not an empty string anymore
        while (1)
        {
          printf ("msh> ");
          fgets (cmd_str, MAX_COMMAND_SIZE, stdin);
          if(strcmp(cmd_str,"\n")!=0)
          {
            break;
          }
        }
    }

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input strings with whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your FAT32 functionality

    /*int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ )
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );
    }

    free( working_root );*/

     if(strcmp(token[0],"open")==0 )
     {
       fp = fopen(token[1],"r");

       if (strcmp(token[1], string) == 0)
       {
         printf("The file is already opened\n");
       }
       else if(fp)
       {
          strcpy(string,token[1]);
          // seek set move from the beginning of the file
          // seek cur move from where we currently are
          // seek end move from the end of the file toward the head of the file
          fseek(fp,11,SEEK_SET);
          fread(&BPB_BytesPerSec,2,1,fp);

          fseek(fp,13,SEEK_SET);
          fread(&BPB_SecPerClus,1,1,fp);

          fseek(fp,14,SEEK_SET);
          fread(&BPB_RsvdSecCnt,2,1,fp);

          fseek(fp,16,SEEK_SET);
          fread(&BPB_NumFat,1,1,fp);

          fseek(fp,36,SEEK_SET);
          fread(&BPB_FATSz32,4,1,fp);

          RootDirSectors = (BPB_NumFat * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);

          fseek(fp,RootDirSectors,SEEK_SET);
          fread(dir, 16, sizeof(struct DirectoryEntry), fp);
        }
        else
        {
          printf("ERROR: file system not found\n");
        }
      }

     else if(strcmp(token[0],"stat") == 0)
     {
        int result = -100;
        int found = -100;
        if(fp)
        {
          char input[11];
          memset(input,0,11);
          strncpy(input, token[1] , strlen(token[1]));
          char *string = strtok(input, ".");
          result = matching(string);
          if (result != -100)
          {
            printf("Attribute    Size    Starting Cluster Number \n");
            printf("%4d %12d %7d\n",dir[result].DIR_Attr,dir[result].DIR_FileSize,dir[result].DIR_FirstClusterLow);
          }
          else
          {
            char str[16];
            char result2[16];
            strncpy(str , token[1] , strlen(token[1]));
            char *string1 = strtok(str, ".");
            char *string2 = strtok(NULL, "\n");;
            if(string2)
            {
              strcpy(result2,string1);
              strcat(result2,string2);
              found = matching(result2);
              if(found != -100)
              {
                printf("Attribute    Size    Starting Cluster Number \n");
                printf("%4d %12d %7d\n",dir[found].DIR_Attr,dir[found].DIR_FileSize,dir[found].DIR_FirstClusterLow);
              }
              else
              {
                printf("Error: No directory found !\n");
              }
            }

          }
        }
        else if(!fp)
        {
          printf("Error: First system image must be opened first\n");
        }

     }
     else if ( strcmp(token[0],"cd") == 0 )
     {
       if(fp)
       {
         int result = -100;
         string[1] = '\0';
         char *string = strtok(token[1]," ");
         result = matching(token[1]);
         if(result == -100)
         {
            printf("Error: Directory not found !\n");
         }
         else
         {
            result = matching(token[1]);
            int clus = dir[result].DIR_FirstClusterLow;
            if(clus == 0)
            {
              clus = 2;
            }
            int offset = LBAToOffset(clus);
            fseek(fp,offset,SEEK_SET);
            fread(dir,sizeof(struct DirectoryEntry),16,fp);
         }

       }
       else
       {
         printf("Error: no file found\n");
       }

     }
     else if ( strcmp(token[0],"ls") == 0 )
     {
        if(fp)
        {
           if((token[0]) || strcmp(token[1],".") == 0 || strcmp(token[1],"..") == 0 )
           {
              for(i = 0; i < 16; i++)
              {
                    char filename[12];
                    strncpy(filename, dir[i].DIR_Name,11);
                    if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && filename[0] != 0xffffffe5)
                    {
                        printf("%s\n",filename);
                    }
              }
           }
        }
        else
        {
          printf("Error: First system image must be opened first\n");
        }

     }
     else if(strcmp(token[0],"read") == 0)
     {
       int i;

       if(fp)
       {
         int result = -100;
         string[1] = '\0';
         char *string = strtok(token[1],".");
         result = matching(token[1]);

         if(result == -100)
         {
            printf("Error: File not found !\n");
         }
         else
         {
            result = matching(token[1]);
            int clus = dir[result].DIR_FirstClusterLow;
            int size = atoi(token[2]);
            int requestedOffset = atoi(token[2]);
            int requestedBytes = atoi(token[3]);

            int bytesremain = requestedBytes;

            printf("size\n");
            while(size >= BPB_BytesPerSec)
            {
              clus = NextLB( clus );
              size = size - BPB_BytesPerSec;
              printf("%d ",size);
            }
            int offset  = LBAToOffset( clus );
            int byteOffset = (requestedOffset % BPB_BytesPerSec);

            fseek(fp , offset + byteOffset, SEEK_SET);
            unsigned char buffer[BPB_BytesPerSec];
      ///////////////////////////////////////////////////////////////////
            int firstBlocksize = BPB_BytesPerSec - requestedOffset;
            fread(buffer , 1 , firstBlocksize , fp);

            for(i = 0; i < firstBlocksize; i++)
            {
              printf("%x ",buffer[i]);
            }

            bytesremain = bytesremain - firstBlocksize;

            while(bytesremain >= 512)
            {
              clus = NextLB(clus);
              offset = LBAToOffset(clus);
              fseek(fp, offset, SEEK_SET);
              fread(buffer,1,BPB_BytesPerSec,fp);
              for(i = 0; i < firstBlocksize; i++)
              {
                printf("%x ",buffer[i]);
              }
              bytesremain = bytesremain - BPB_BytesPerSec;
            }
            //////////////////////////////////////////
            if(bytesremain)
            {
              clus = NextLB(clus);
              offset = LBAToOffset(clus);
              fseek ( fp, offset, SEEK_SET);
              fread(buffer , 1 , bytesremain , fp);
              for(i = 0; i < bytesremain; i ++)
              {
                 printf("%x ",buffer[i]);
              }
            }
         }
         printf("\n");
       }
       else
       {
         printf("Error: First system image must be opened first\n");
       }

     }
     else if(strcmp(token[0],"close")==0 )
     {
       if (strcmp (string,"/0") != 0 )
       {
         fclose(fp);
         fp = NULL;
         strcpy(string,"/0");
       }
       else
       {
         printf("Error: First system image must be opened first\n");
       }
     }


     else if(strcmp(token[0],"bpb") == 0)
     {
        printf("\nBPB_BytesPerSec\n");
        printf("hexadecimal : %x\nbase 10     : %d\n\n", BPB_BytesPerSec, BPB_BytesPerSec );

        printf("BPB_SecPerClus \n");
        printf("hexadecimal : %x\nbase 10     : %d\n\n", BPB_SecPerClus,BPB_SecPerClus );

        printf("BPB_RsvdSecCnt\n");
        printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_RsvdSecCnt, BPB_RsvdSecCnt );

        printf("BPB_NumFATS\n");
        printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_NumFat,BPB_NumFat);

        printf("BPB_FATSz32\n");
        printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_FATSz32,BPB_FATSz32);
     }
     else if(strcmp(token[0],"quit") == 0)
     {
        return 0;
     }
     else if( strcmp( token[0], "get" ) == 0 )
     {
        if( fp )
        {
          getFile( token[1], token[2] );
        }
        else
        {
          printf( "Error: File system image must be opened first.\n" );
        }
    }
     else
     {
        if(fp)
          printf("Command not found ! \n");
        else
          printf("Error: First system image must be opened first\n");

     }

  }
  return 0;
}
