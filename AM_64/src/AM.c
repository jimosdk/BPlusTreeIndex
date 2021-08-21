#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

#include "AM.h"
#include "bf.h"
#include "defn.h"

#include "myfunctions.h"


#include <unistd.h>		//access() function
#include <linux/limits.h>	//PATH_MAX define


int AM_errno = AME_OK;

void AM_Init() {
	/*initiallizes the global arrays and the BF level*/
	int i ;
	AM_errno = BF_Init(LRU);
	if (AM_errno == AM_ACTIVE_ERROR)
        	printf("Το επίπεδο BF είναι ενεργό και δεν μπορεί να αρχικοποιηθεί\n");
	for(i = 0 ; i <= OPEN_FILES_SIZE - 1; i++)
		Open_files[i] = NULL; 			//initializing the array pointers to NULL since
	for(i = 0 ; i <= OPEN_SCANS_SIZE - 1; i++)	//there are no open files or running scans
		Open_scans[i] = NULL;				
}

int AM_CreateIndex(char *fileName, 
	               char attrType1, 
	               int attrLength1, 
	               char attrType2, 
	               int attrLength2) {
	/*creates a file named fileName , the file is then opened ,
	block 0 is allocated in order to store important information
	for the file.The file is then closed*/
	BF_Block* block;
	char* block_data;
	int fileDesc;
	int i = 0;
	int root = -1;
	int max_records_per_block;
	if(size_error(attrType1,attrLength1) == -1)
		return -1;
	if(size_error(attrType2,attrLength2) == -1)
		return -1;
	max_records_per_block = (BF_BLOCK_SIZE - leaf_header_size)/(sizeof(int) + attrLength1 + attrLength2);
	BF_Block_Init(&block);    				     //initializing block struct
	AM_errno = BF_CreateFile(fileName);                         //file created
	if(AM_errno != BF_OK)
		return -1;
	AM_errno = BF_OpenFile(fileName,&fileDesc);                 //opening file to create first block for storing metadata
	if(AM_errno != BF_OK)
		return AM_errno;
	AM_errno = BF_AllocateBlock(fileDesc,block);                //first block of the file contains metadata
	if(AM_errno != BF_OK)
		return -1;
	block_data = BF_Block_GetData(block);			    //getting the data of the block in order to modify it
	memcpy(block_data,&root,sizeof(int));                       //storing current root (-1 for none)
	i+= sizeof(int);
	memcpy(block_data + i,&attrType1,sizeof(char));             //storing type of field 1/key
	i += sizeof(char);					                 
	memcpy(block_data + i,&attrLength1,sizeof(int));            //storing length/size of field 1/key
	i+= sizeof(int);
	memcpy(block_data + i,&attrType2,sizeof(char));		    //storing type of field 2
	i+= sizeof(char);					    
	memcpy(block_data + i,&attrLength2,sizeof(int));            //storing length/size of field 2
	i+= sizeof(int);
	memcpy(block_data + i,&max_records_per_block,sizeof(int));  //storing value of max records / per block
	BF_Block_SetDirty(block);					//block has been modified
	AM_errno = BF_UnpinBlock(block);				//block not needed in memory anymore
	if(AM_errno != BF_OK)
		return -1;
	BF_Block_Destroy(&block);				//destroying block struct
	AM_errno = BF_CloseFile(fileDesc);                           //closing file
	if(AM_errno != BF_OK)
		return -1;	
	return AME_OK;
	
}

int AM_DestroyIndex(char *fileName) {
	/*searches for an active registration.If it finds one it checks for 
	a fileName match , if there is a fileName match ,error ,else it deletes the file */
	int res,i;
	char actual_path[PATH_MAX + 1];
	char *ptr;
	if( access( fileName, F_OK ) == -1 ) {// file doesn't exist
		AM_errno = AM_INVALID_FILENAME;
		return -1;
	}
	for(i=0; i<=OPEN_FILES_SIZE - 1;i++){ //scanning the array for an active registration
		if(Open_files[i] != NULL){//if there is an active registration on the array
			if(!strcmp(Open_files[i]->fileName,fileName)){ //check to see if it is a registration of the file for deletion 
				AM_errno = AM_FILE_OPEN; //fileName match,error code  unable to destroy an open file
				return -1;
			}
		}
	}
	ptr = realpath(fileName,actual_path);//retrieving the full path of the file
	res = remove(ptr);//removing the file
	if(res!=0){
		AM_errno = AM_REMOVE_FAILED;  //error code remove failed to delete file specified
		return -1;
	}	
	return AME_OK;
}

int AM_OpenIndex (char *fileName) {
	/*Opens file specified and creates its registration on
	Open files array.Error if the array is full*/
	int fileDesc;
	BF_Block* block;
	char* block_data;
	int i = 0;
	if( access( fileName, F_OK ) == -1 ) {// file doesn't exist
		AM_errno = AM_INVALID_FILENAME;
		return -1;
	}
	BF_Block_Init(&block);					//initializing block struct
	AM_errno =  BF_OpenFile(fileName,&fileDesc);            //opening requested file and retrieving real fileDesc
	if(AM_errno != BF_OK)
		return -1;
	AM_errno = BF_GetBlock(fileDesc,0,block);               //getting the first block created in CreateIndex in order to read the metadata
	if(AM_errno != BF_OK)
		return -1;
	block_data = BF_Block_GetData(block);                   //contains metadata to be copied on the Open files array registration
	for(i = 0 ; i<= OPEN_FILES_SIZE - 1; i ++)    //searching NULL pointer in Open files array for registration
		if(Open_files[i] == NULL)            
			break;                     
	if(i == OPEN_FILES_SIZE){
		AM_errno = AM_FILES_ARRAY_LIMIT;                      //max registrations on Open files array error code 
		return -1;
	}	
	INDEX_DATA_init(&Open_files[i],fileDesc,fileName,block_data);//creating a registration on Open_files array 
	AM_errno = BF_UnpinBlock(block);                                 //block not needed in memory anymore
	if(AM_errno != BF_OK)
		return -1;
	BF_Block_Destroy(&block);                             //destroying block struct                                        
	return i;                                          //the fileDesc is the corresponding index of the registration on Open files array
}

int AM_CloseIndex (int fileDesc) {
	/*removes the Open files array registration specified,unless it doesn't exist 
	or there is a scan running on it*/
	int real_fileDesc;
	char* block_data;
	BF_Block *block;
	if(Open_files[fileDesc] == NULL || fileDesc > OPEN_FILES_SIZE -1 || fileDesc < 0){
		AM_errno = AM_INVALID_FILEDESC;					//invalid fileDesc
		return -1;
	}
	if(Open_files[fileDesc]->open_scans > 0){               //are there any scans running on the file still?
		AM_errno = AM_SCAN_RUNNING;                                 // error code unable to close file while scan is running
		return -1;                                         
	}
	BF_Block_Init(&block);                                 
	real_fileDesc = Open_files[fileDesc]->fileDesc;		//retrieving the actual fileDesc returned from BF_Openfile
	AM_errno = BF_GetBlock(real_fileDesc,0,block);          //retrieving block 0 in order to update the root value stored in it
	if(AM_errno != BF_OK)
		return -1;
	block_data = BF_Block_GetData(block);                   //will update the root value stored in the block's data to the current value
	INDEX_DATA_destroy(&Open_files[fileDesc],block_data);       //destroying the struct of the registration, updating block 0
	BF_Block_SetDirty(block);                               //block updated
	AM_errno = BF_UnpinBlock(block);			//block not needed
	if(AM_errno != BF_OK)
		return -1;
	BF_Block_Destroy(&block); 
	AM_errno = BF_CloseFile(real_fileDesc);                                    //Closing the file
	if(AM_errno != BF_OK)
		return -1;
	return AME_OK;
}

int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
	/*stores a path of block numbers from root to data block.Calls insert leaf 
	for the top value of path (it's always a leaf block number).If the leaf is split
	,calls index insert (on the remaining index block numbers in path) 
	in order to modify the upper part of the tree accordingly,starting with inserting
	index value(middle key from split) and linking the new block to a parent block*/
	PATH* path;//stack of block numbers
	int newno;//carries the value of the block ,created from split ,in order to link it to a parent block
	int split,err;
	void* index_value;//carries the value of the key that is going to be inserted on a higher tree level(if there was a split)
	PATH_Init(&path);//allocating empty stack for storing path from root to a data block
	if(Open_files[fileDesc] == NULL || fileDesc > OPEN_FILES_SIZE -1 || fileDesc < 0){
		AM_errno = AM_INVALID_FILEDESC;					//invalid fileDesc
		return -1;
	}
	index_value = malloc(Open_files[fileDesc]->f_len1);//this will store the middle value to be pushed up after a split
	err = descend_tree(fileDesc,value1,path); //descending to a data block and storing block numbers in path
	if(err == -1)
		return -1;
	split = leaf_insert(fileDesc,value1,value2,path,&newno,index_value);// split = 1 for true ,0 for false,-1 for error
	if(split == -1)
		return -1;
	if(split == 1){//if the data block in path was split push index_value up and modify index_blocks accordingly
		err = index_insert(fileDesc,index_value,path,newno);//applying insert to the remaining index blocks of path
		if(err == -1)
			return -1;
	}
	PATH_Destroy(&path);
	free(index_value);
  	return AME_OK;
}

int AM_OpenIndexScan(int fileDesc, int op, void *value) {
	/*if the tree is not empty and the Open scans array is not full
	it finds the correct leaf block and makes a registration for a new scan*/
	PATH *path;
	int start_block,err,i;
	if(Open_files[fileDesc] == NULL || fileDesc > OPEN_FILES_SIZE -1 || fileDesc < 0){
		AM_errno = AM_INVALID_FILEDESC;					//invalid fileDesc
		return -1;
	}
	if(Open_files[fileDesc]->root_block == -1){
		AM_errno = AM_TREE_EMPTY;                        //tree is empty
		return -1;
	}
	if(op == 1 || op == 4 || op == 6){// using descend tree to find the correct start block
		PATH_Init(&path);//initialliazing empty stack path,its going to store all the block numbers while descending from root to leaf
		err = descend_tree(fileDesc,value,path);
		if(err == -1)
			return -1;
		start_block = PATH_Pop(path);
		PATH_Destroy(&path);	
	}
	else if(op == 2 || op == 3 || op == 5)//start block will be the left most,immediate descend only to the left
		start_block = 1;//the leftmost leaf is always the first block of the tree (block number 0 reserved for metadata)
	for(i = 0 ; i<= OPEN_SCANS_SIZE - 1; i ++)//scanning for empty spot -> Null pointer
		if(Open_scans[i] == NULL)            
			break;
	if(i == OPEN_SCANS_SIZE){
		AM_errno = AM_SCANS_ARRAY_LIMIT;                      //max registrations on Open scans array error code 
		return -1;
	}	
	SCAN_DATA_init(&Open_scans[i],fileDesc,op,value,start_block);//registering scan on Open scans array
	(Open_files[fileDesc]->open_scans)++;
  	return i;//scanDesc is the position of the registration on the Open scans array 
}

void *AM_FindNextEntry(int scanDesc) {
	/*if scanDesc corresponds to an active registration of a running scan 
	on Open scans array ,it scans for the next entry that fullfills the requirements*/
	if(Open_scans[scanDesc] == NULL || scanDesc > OPEN_SCANS_SIZE || scanDesc < 0){
		AM_errno = AM_INVALID_SCANDESC;  //invalid scanDesc
		return NULL;
	}
	if(Open_scans[scanDesc]->overflow_block >= 0)//last eligible entry found had an overflow index > -1(existing overflow block)
		return find_next_overflow_entry(scanDesc);//return the next value2 in the overflow block
	if(Open_scans[scanDesc]->next_block >= 0)//if next_block = -1 ,next block doesn't exist -> scanned through all eligible leaf blocks
		return find_next_leaf_entry(scanDesc);//returns NULL if no entry is found and scan ends
	AM_errno = AME_EOF;
	return NULL;// no more blocks to be scanned
			
	
}

int AM_CloseIndexScan(int scanDesc) {
	/*removes the Open scan array registration specified 
	unless it doesn't exist*/
	if(Open_scans[scanDesc] == NULL || scanDesc > OPEN_SCANS_SIZE || scanDesc < 0){
		AM_errno = AM_INVALID_SCANDESC;  //invalid scanDesc
		return -1;
	}
	(Open_files[Open_scans[scanDesc]->fileDesc]->open_scans)--; //decreasing the number of open scans on the file by one
	SCAN_DATA_destroy(&Open_scans[scanDesc]);                   //destroying the registration of this scan on Open scans array
  	return AME_OK;
}

void AM_PrintError(char *errString) {
	printf("\n %s \n\n error code -->>%d<<-- \n\n", errString,AM_errno);                //printing errString,and error code
	if(AM_errno == AM_OPEN_FILES_LIMIT_ERROR)	
		printf("Υπάρχουν ήδη BF_MAX_OPEN_FILES αρχεία ανοικτά\n\n");
	else if (AM_errno == AM_INVALID_FILE_ERROR)
          	printf("Ο αναγνωριστικός αριθμός αρχείου δεν αντιστιχεί σε κάποιο ανοιχτό αρχείο\n\n");
  	else if (AM_errno == AM_FILE_ALREADY_EXISTS) 
       		printf("Το αρχείο δεν μπορεί να δημιουργιθεί γιατι υπάρχει ήδη\n\n");
  	else if (AM_errno == AM_FULL_MEMORY_ERROR)          
		printf("Η μνήμη έχει γεμίσει με ενεργά block\n\n");
	else if (AM_errno == AM_INVALID_BLOCK_NUMBER_ERROR) 
		printf("Το block που ζητήθηκε δεν υπάρχει στο αρχείο\n\n");
  	else if( AM_errno == AM_FILE_OPEN)
                printf("File is open and it can not be deleted\n\n");
  	else if (AM_errno == AM_REMOVE_FAILED)	
		printf("Remove failed to delete file\n\n");
  	else if (AM_errno == AM_FILES_ARRAY_LIMIT)
	        printf("Max registration on open files array\n\n");
  	else if (AM_errno == AM_INVALID_FILENAME)
	        printf("Invalid filename given\n\n");
  	else if (AM_errno == AM_SCAN_RUNNING)
                printf("unable to close file ,scan is running\n\n");
  	else if (AM_errno == AM_INVALID_FILEDESC)
	        printf("Invalid file descriptor\n\n");
  	else if (AM_errno == AM_INVALID_SCANDESC)     
	        printf("Invalid scan descriptor\n\n");
  	else if (AM_errno == AM_TREE_EMPTY)
                printf("Tree is empty ,unable to scan\n\n");
	else if (AM_errno == AM_SCANS_ARRAY_LIMIT)
                printf("Max registrations on open scans array\n\n");
  	else if (AM_errno == AM_INVALID_FIELD_SIZE)
	        printf("AttrLength does not match AttrType\n\n");
 	else if (AM_errno == AM_INVALID_TYPE)
                printf("Invalid attrType\n\n");
	else if (AM_errno == AME_EOF)
                printf("Scan reached end of file\n\n");
  
}

void AM_Close() {
	BF_Close();  
}
