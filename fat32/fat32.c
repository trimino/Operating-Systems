
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
#include <stdint.h>

#define MAX_NUM_ARGUMENTS 3

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size


// BPB: BIOS Parmeter Block  - located in the first sector of the reserved sectors
char    BS_OEMName[8];
char    BS_VolLab[11];
int8_t  BPB_SecPerClus;
int8_t  BPB_NumFATs;

int16_t BPB_RootEntCnt;
int16_t BPB_BytesPerSec;
int16_t BPB_RsvdSecCnt;

int32_t BPB_FATSz32;
int32_t BPB_RootClus;
int32_t RootDirSectors = 0;
int32_t FirstDataSector = 0;
int32_t FirstSectorofCluster = 0;

FILE *fp;

// Each record can be represented by the struct below
struct __attribute__((__packed__)) DirectoryEntry{
    char        DIR_Name[11];
    uint8_t     DIR_Attr;
    uint8_t     Unused1[8];
    uint8_t     Unused2[4];
    uint16_t    DIR_FirstClusterHigh;
    uint16_t    DOR_FirstClusterLow;
    uint32_t    DIR_FileSize;
};
struct DirectoryEntry dir[16];


/*
*Function       : LBAToOffset
*Parameters     : The current sector number that points to a block of data
*Returns        : The value of the address for that block of data
*Description    : Finds the starting address of  a block of data given the sector number corresponding to that data block  
*/
int LBAToOffset(int32_t sector)
{
    return ( (sector - 2) * BPB_BytesPerSec ) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}


/*
Name: NextLB
Parameters: Given a logical block address, look up into the first FAT and return the logical block address of the block in the file. If there is no futher 
blocks then return -1;
*/
int16_t NextLB(uint32_t sector)
{
    uint32_t    FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t     val;
    
    fseek( fp, FATAddress, SEEK_SET);
    fread( &val, 2, 1, fp);
    return val;
}


int matching (char* input)
{
    int location;
    int result = -100;
    int i;

    // Convert all characters to uppercase
    for( i = 0; i < 11; i++ )
    {
        input[i] = toupper( input[i] );
    }

    for (location  = 0; location < 16; location ++)
    {
        if(dir[location].DIR_Attr == 0x01 || dir[location].DIR_Attr == 0x10 || dir[location].DIR_Attr == 0x20)
        {
            char filename[12];
            char *string = strtok(input," ");
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



int main()
{

    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

    while( 1 )
    {
        // Print out the mfs prompt
        printf ("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

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

        // Tokenize the input stringswith whitespace used as the delimiter
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

        // CODE TO HELP WITH DEUBG - prints tokenized input
        // int token_index  = 0;
        // for( token_index = 0; token_index < token_count; token_index ++ ) 
        // {
        //   printf("token[%d] = %s\n", token_index, token[token_index] );  
        // }
        // free( working_root );


        if (strcmp( token[0], "open" ) == 0)
        {
            // Condition if the user opens a file that is already opened
            if (fp)
            {
                printf("The file is already opened\n");
            }
            else
            {
                // Open the fat32.img
                fp = fopen( token[1], "r");

                if (fp)
                {
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
                    fread(&BPB_NumFATs,1,1,fp);

                    fseek(fp,36,SEEK_SET);
                    fread(&BPB_FATSz32,4,1,fp);

                    RootDirSectors = (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec) +(BPB_RsvdSecCnt * BPB_BytesPerSec);

                    fseek(fp,RootDirSectors,SEEK_SET);
                    fread(dir, 16, sizeof(struct DirectoryEntry), fp);
                }
                else
                {
                    printf("ERROR: file system not found\n");
                }
            }

        }


        else if (strcmp( token[0], "stat" ) == 0)
        {
            // Code for stat command
        }


        else if(strcmp( token[0], "cd" ) == 0)
        {
            // Code for cd command       
        }


        else if (strcmp(token[0], "ls" ) == 0)
        {
            // Program requirements to support '.' and '..'
            if (token[0] || strcmp( token[1], "." ) == 0 || strcmp( token[1], ".." ) == 0){
                // Check to see if the file is open to view the contents
                if (fp)
                {
                    int i ;
                    for(i = 0; i < 16; i++)
                    {
                        char filename[12];
                        strncpy(filename, dir[i].DIR_Name, 11);
                        // Do not list deleted files or system volme names
                        if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 ) && filename[0] != 0xffffffe5)
                        {
                            printf("%s\n",filename);
                        }
                    }
                }
            }
        }


        else if (strcmp( token[0], "info" ) == 0)
        {
            // Code for info command
        }


        else if (strcmp( token[0], "get" ) == 0)
        {
            // Code for get command
        }


        else if (strcmp( token[0], "read" ) == 0)
        {
            // Code for read command
        }


        else if (strcmp( token[0], "close" ) == 0)
        {
            // Code for close command
        }


        else if (strcmp( token[0], "bpb" ) == 0){
            printf("\nBPB_BytesPerSec\n");
            printf("hexadecimal : %x\nbase 10     : %d\n\n", BPB_BytesPerSec, BPB_BytesPerSec );

            printf("BPB_SecPerClus \n");
            printf("hexadecimal : %x\nbase 10     : %d\n\n", BPB_SecPerClus,BPB_SecPerClus );

            printf("BPB_RsvdSecCnt\n");
            printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_RsvdSecCnt, BPB_RsvdSecCnt );

            printf("BPB_NumFATS\n");
            printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_NumFATs,BPB_NumFATs);

            printf("BPB_FATSz32\n");
            printf("hexadecimal : %x\nbase 10     : %d\n\n",BPB_FATSz32,BPB_FATSz32);
        }


        else if (strcmp( token[0], "quit" ) == 0)
        {
            return 0;
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