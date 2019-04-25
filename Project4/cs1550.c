/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

cs1550_root_directory root;
cs1550_directory_entry dir;
int flag = 0;
char bitmap[3 * BLOCK_SIZE * 8];

/*
 * Reads bitmap stored in .disk
 * Sets first block and last 2051 blocks to 1
 */
static void read_bitmap(void){ //Set up bitmap and read in from .disk
	
	char byte_val;
	FILE *f;
	f = fopen(".disk", "r");
	fseek(f, -3 * BLOCK_SIZE, SEEK_END);
	
	int i;
	for(i = 0; i < 10240 / 8; i++){		
		fread(&byte_val, 1, 1, f);
		int offset = 0;
		while(offset < 8){ //Doing bit manipulations
			if((byte_val & 0b10000000) == 0b10000000)
				bitmap[i * 8 + offset] = 1;
			else
				bitmap[i * 8 + offset] = 0;
			offset++;
			byte_val = byte_val << 1;
		}
	}
	fclose(f);

	for(i = 0; i < 24 * BLOCK_SIZE; i++){ //Setting last few bits to 1
		if(i == 0 || i >= 10237)
			bitmap[i] = 1;
	}
}

/*
 * Writes bitmap back to .disk
 *
 */
static void write_bitmap(void){ //Write bitmap back to .disk
	char byte_val;
	FILE *f;
	f = fopen(".disk", "r+");
	fseek(f, -3 * BLOCK_SIZE, SEEK_END);

	int i;
	for(i = 0; i < 10240 / 8; i++){
		byte_val = 0;
		int offset = 0;
		while(offset < 8){
			int value = bitmap[i * 8 + offset];
			byte_val = byte_val << 1;
			byte_val |= value;
			offset++;
		}
		fwrite(&byte_val, 1, 1, f);
	}
	fclose(f);
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";

	memset(stbuf, 0, sizeof(struct stat));
  	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	FILE *f;
	f = fopen(".disk", "r");
	fread(&root, BLOCK_SIZE, 1, f);
	fclose(f);

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		int i;
		res = -ENOENT;
		
		if(strlen(filename) == 0){ //Not a filename
			//Check if name is subdirectory
			for(i = 0; i < root.nDirectories; i++){
				struct cs1550_directory dir = root.directories[i];
				//Might want to return a structure with these fields
				if(strcmp(directory, dir.dname) == 0){
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					res = 0; //no error
				}
			}
		}
			
		else{ //Contains a file name
			for(i = 0; i < root.nDirectories; i++){
				struct cs1550_directory direct = root.directories[i]; //Get directory from root
				if(strcmp(directory, direct.dname) == 0){ //See if directory names match
					//Check if name is a regular file
					//regular file, probably want to be read and write
					int block = direct.nStartBlock; //Get block of directory
					f = fopen(".disk", "r");
					fseek(f, block * BLOCK_SIZE, SEEK_SET); //Read in that block of data into dir struct
					fread(&dir, BLOCK_SIZE, 1, f);
					fclose(f);
					int j;
					for(j = 0; j < dir.nFiles; j++){
						struct cs1550_file_directory file = dir.files[j];
						if(strcmp(filename, file.fname) == 0 && strcmp(extension, file.fext) ==0){
							stbuf->st_mode = S_IFREG | 0666; 
							stbuf->st_nlink = 1; //file links
							stbuf->st_size = file.fsize; 
							res = 0; // no error
						}
					}
				}
			}
		
		}
	}
	read_bitmap();
	write_bitmap();
 
	return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if(strcmp(path, "/") == 0){
		int i;
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		for(i = 0; i < root.nDirectories; i++){ //Listing all subdirs of root
			char * newpath;
			newpath = root.directories[i].dname;
			filler(buf, newpath, NULL, 0);
		}
		return 0;
	}

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (strlen(filename) != 0)
		return -ENOENT;
	
	FILE *f;
	f = fopen(".disk", "r");
	fread(&root, BLOCK_SIZE, 1, f);
	int i;

	struct cs1550_directory direct;
	for(i = 0; i < root.nDirectories; i++){ //Get subdirectory
		direct = root.directories[i];
		if(strcmp(directory, direct.dname) == 0)
			break;
	}
	if(i == root.nDirectories){ // This directory doesn't exist
		fclose(f);
		return -ENOENT;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
		
	int block = direct.nStartBlock;
	fseek(f, block * BLOCK_SIZE, SEEK_SET);
	fread(&dir, BLOCK_SIZE, 1, f);
	for(i = 0; i < dir.nFiles; i++){ //Listing all files of subdir
		char * newpath;
		newpath = dir.files[i].fname;
		strcat(newpath, ".");
		strcat(newpath, dir.files[i].fext);
		filler(buf, newpath, NULL, 0);
	}
	fclose(f);
	return 0;
}

/*
 * From the bitmap, find the closest bit set to zero, represents closest free block
 * Return that block number and set returned block to 1
 */
static int block_number(void)
{
	int number = 0;
	for(number = 0; number < sizeof(bitmap); number++){
		if(bitmap[number] == 0){
			bitmap[number] = 1;
			return number;
		}
	}
	return -1;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) mode;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";
	
	if(path[0] != '/') //Check if under root directory
		return -EPERM;

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if(strlen(filename) != 0) //Check if trying to make a file rather than directory
		return -EPERM;
	if(strlen(directory) > MAX_FILENAME)
		return -ENAMETOOLONG;

	int i;
	for(i = 0; i < root.nDirectories; i++){ //Check if directory already exists
		struct cs1550_directory dir = root.directories[i];
		if(strcmp(directory, dir.dname) == 0)
			return -EEXIST;
	}

	
	FILE *f;
	f = fopen(".disk", "r+");
	fread(&root, BLOCK_SIZE, 1, f);
	if(root.nDirectories >= MAX_DIRS_IN_ROOT){
		fclose(f);
		return -ENOSPC;
	}

	fseek(f, 0, SEEK_SET);
	int index = root.nDirectories;
	struct cs1550_directory direct = root.directories[index]; //Update directory struct within root
	strcpy(direct.dname, directory); //Update name
	int block = block_number(); //Update start block
	if(block == -1){
		fclose(f);
		return -ENOSPC;
	}
	direct.nStartBlock = block;
	root.directories[index] = direct;
	root.nDirectories += 1; //Increment number of directories

	fwrite(&root, BLOCK_SIZE, 1, f); //write to disk
	fclose(f);	
	write_bitmap();
	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	
	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if(strlen(filename) > 8 || strlen(extension) > 3)
		return -ENAMETOOLONG;	
	if(strlen(filename) == 0)	
		return -EPERM;
	FILE *f;
	f = fopen(".disk", "r+");
	fread(&root, BLOCK_SIZE, 1, f);
	
	int i;
	for(i = 0; i < root.nDirectories; i++){
		struct cs1550_directory direct = root.directories[i];
		if(strcmp(direct.dname, directory) == 0){ //Check to see if right directory
			int block = direct.nStartBlock;
			fseek(f, block * BLOCK_SIZE, SEEK_SET);
			fread(&dir, BLOCK_SIZE, 1, f);

			if(dir.nFiles == MAX_FILES_IN_DIR)
				return -ENOSPC; //Return this error if exceeding max files in dir
			int j;
			for(j = 0; j < dir.nFiles; j++){
				struct cs1550_file_directory file_dir = dir.files[j];
				if(strcmp(file_dir.fname, filename) == 0 && strcmp(file_dir.fext, extension) == 0)
					return -EEXIST; //If file already exists in directory, return this error
			}

			int file_block = block_number(); //Get next block for file
			struct cs1550_file_directory new_file = dir.files[dir.nFiles];
			strcpy(new_file.fname, filename);
			strcpy(new_file.fext, extension);
			new_file.nStartBlock = file_block;
			dir.files[dir.nFiles] = new_file;
			dir.nFiles += 1;

			fseek(f, block * BLOCK_SIZE, SEEK_SET);
			fwrite(&dir, BLOCK_SIZE, 1, f);
			fclose(f);
			break;
		}
	}
	if(i == root.nDirectories) //No directory found
		return -ENOENT;
	write_bitmap();
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (strlen(filename) == 0) //Trying to read from directory, not allowed
		return -EISDIR;
	
	struct cs1550_file_directory file_entry;
	int dirBlock = -1;
	int fileBlock = -1;
	FILE *f;
	f = fopen(".disk", "r");
	fread(&root, BLOCK_SIZE, 1, f);
	
	//Find directory and file, check path exists
	int i;
	int j = -1;
	for(i = 0; i < root.nDirectories; i++){
		struct cs1550_directory direct = root.directories[i];
		if(strcmp(direct.dname, directory) == 0){
			dirBlock = direct.nStartBlock;
			fseek(f, dirBlock * BLOCK_SIZE, SEEK_SET);
			fread(&dir, BLOCK_SIZE, 1, f);
			
			for(j = 0; j < dir.nFiles; j++){
				file_entry = dir.files[j];
				if(strcmp(file_entry.fname, filename) == 0 && strcmp(file_entry.fext, extension) == 0){
					fileBlock = file_entry.nStartBlock;
					i = root.nDirectories + 1; //Force loop to break
					break;					
				}
			}			
		}
	}

	if(size == 0){
		fclose(f);
		return 0;
	}
	if(i == root.nDirectories){
		fclose(f);
		return -ENOENT;
	}
	if(offset > file_entry.fsize){
		fclose(f);
		return -EFBIG;
	}

	while(offset > 512){ //Get to right block
		fileBlock += 1;
		offset -= 512;
	}

	if (size + offset > file_entry.fsize)
		size = file_entry.fsize;
	struct cs1550_disk_block disk;
	int counter = 1;
	int bytesRemaining = size;
	while(bytesRemaining > 512){
		fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
		fread(&disk, BLOCK_SIZE, 1, f);
		for(i = offset; i < 512; i++)
			*(buf + i + (counter - 1) * 512) = disk.data[i];
		fileBlock++;
		bytesRemaining -= 512;
		counter++;
		offset = 0;		
	}
	fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
	fread(&disk, BLOCK_SIZE, 1, f);
	fclose(f);
	for(i = 0; i < bytesRemaining; i++)
		*(buf + i + (counter - 1) * 512) = disk.data[i];

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{

	(void) fi;
	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char extension[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (strlen(filename) == 0) //Trying to write into directory, not allowed
		return -EPERM;
	
	struct cs1550_file_directory file_entry;
	int dirBlock = -1;
	int fileBlock = -1;
	int index = -1;
	FILE *f;
	f = fopen(".disk", "r+");
	fread(&root, BLOCK_SIZE, 1, f);
	
	//Find directory and file, check path exists
	int i;
	int j = -1;
	for(i = 0; i < root.nDirectories; i++){
		struct cs1550_directory direct = root.directories[i];
		if(strcmp(direct.dname, directory) == 0){
			dirBlock = direct.nStartBlock;
			fseek(f, dirBlock * BLOCK_SIZE, SEEK_SET);
			fread(&dir, BLOCK_SIZE, 1, f);
			
			for(j = 0; j < dir.nFiles; j++){
				file_entry = dir.files[j];
				if(strcmp(file_entry.fname, filename) == 0 && strcmp(file_entry.fext, extension) == 0){
					fileBlock = file_entry.nStartBlock;
					index = j;
					i = root.nDirectories + 1; //Force loop to break
					break;					
				}
			}			
		}
	}

	if(size == 0){
		fclose(f);
		return 0;
	}
	if(i == root.nDirectories){
		fclose(f);
		return -ENOENT;
	}
	if(offset > file_entry.fsize){
		fclose(f);
		return -EFBIG;
	}

	int blocksWrite = (offset + size) / 512 + 1; //Total number of blocks allocated to the file if buffer written
	int curBlocks = file_entry.fsize / 512 + 1; //Total number of blocks file is currently using
	
	if(blocksWrite > curBlocks && bitmap[fileBlock + blocksWrite - 1] == 1){
		fclose(f);
		return -ENOSPC; //Return error if writing extra blocks to file, but extra blocks not allocated
	}
	if((offset + size) > file_entry.fsize) //Adjust size if appending
		file_entry.fsize = offset + size;

	dir.files[index] = file_entry;	
	fseek(f, dirBlock * BLOCK_SIZE, SEEK_SET); //Write new directory info to disk
	fwrite(&dir, BLOCK_SIZE, 1, f);

	struct cs1550_disk_block disk;
	fileBlock += offset / 512; //Go to correct block based off of offset
	fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
	fread(&disk, BLOCK_SIZE, 1, f);
	
	int counter = 1;
	while(size > 512){
		for(i = offset % 512; i < BLOCK_SIZE; i++) //Writing data to disk struct
			disk.data[i] = *(buf + i + (counter - 1) * 512);
		bitmap[fileBlock] = 1; //Set bitmap
		fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&disk, BLOCK_SIZE, 1, f); //Write this info to file
		fread(&disk, BLOCK_SIZE, 1, f);
		fileBlock++; //Update file block
		size -= 512;
		offset = 0; //After initial iteration, set offset to 0
		counter++;
	}
	int bytesRemaining = size;
	for(i = offset % 512; i < 512; i++){ //Write remaining data to disk struct
		disk.data[i] = *(buf  + (i - offset % 512) + (counter - 1) * 512);
		bytesRemaining--;
	}
	if(bytesRemaining > 0){
		bitmap[fileBlock] = 1;
		fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&disk, BLOCK_SIZE, 1, f);
		fread(&disk, BLOCK_SIZE, 1, f);
		for(i = 0; i < bytesRemaining; i++)
			disk.data[i] = *(buf + (size - bytesRemaining) + i + (counter - 1) * 512);
		fseek(f, (fileBlock + 1) * BLOCK_SIZE, SEEK_SET);
		fwrite(&disk, BLOCK_SIZE, 1, f);
	}
	else{
		bitmap[fileBlock] = 1;
		fseek(f, fileBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&disk, BLOCK_SIZE, 1, f);
	}
	fclose(f);
	
	write_bitmap();	
	//write data
	//set size (should be same as input) and return, or error

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
