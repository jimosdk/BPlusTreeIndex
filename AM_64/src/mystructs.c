#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

#include "AM.h"
#include "bf.h"
#include "defn.h"

#include "mystructs.h"

//================================================GLOBAL ARRAYS============================================================================
INDEX_DATA *Open_files[OPEN_FILES_SIZE];			//array of pointers to INDEX_DATA for the currently open files
SCAN_DATA *Open_scans[OPEN_SCANS_SIZE];                     //array of pointers to SCAN_DATA for the currently running scans
//=============================================INDEX_DATA MEMBER FUNCTIONS=====================================================
void INDEX_DATA_init(INDEX_DATA** index_data,int fileDesc,char* fileName,char* metadata){
	/* initializes struct,retrieving the info stored in the 1st block of the file
	*index_data(Open_files array pointer) points to the allocated space */
	int i = 0;
	*index_data = (INDEX_DATA*)malloc(sizeof(INDEX_DATA));			//allocating space for the struct
	(*index_data)->fileName = (char*)malloc(sizeof(char)*(strlen(fileName)+1));//allocating member to have the length the of fileName given
	(*index_data)->fileDesc = fileDesc;
	(*index_data)->open_scans = 0;
	strcpy((*index_data)->fileName,fileName);              			//copying the name of the file 
	memcpy(&(*index_data)->root_block,metadata,sizeof(int));         		//retrieving the root block of the file
	i+=sizeof(int);
	memcpy(&(*index_data)->f_type1,metadata + i,sizeof(char));        	//retrieving the type of field 1
	i+=sizeof(char);
	memcpy(&(*index_data)->f_len1,metadata + i,sizeof(int));          	//retrieving the length/size of field 1
	i+=sizeof(int);
	memcpy(&(*index_data)->f_type2,metadata + i,sizeof(char));		//retrieving the type of field 2
	i+=sizeof(char);
	memcpy(&(*index_data)->f_len2,metadata + i,sizeof(int));			//retrieving the length/size of field 2
	i+=sizeof(int);
	memcpy(&(*index_data)->max_records,metadata + i,sizeof(int));		//retrieving the number of max records per block
}

void INDEX_DATA_destroy(INDEX_DATA** index_data,char* metadata){
/*frees the space pointed at by *index_data(Open files array pointer) updates the root value stored in block 0 of the file*/
	memcpy(metadata,&(*index_data)->root_block,sizeof(int));//updating root value of block 0 of the file
	free((*index_data)->fileName);//freeing allocated space for fileName
	free(*index_data);//freeing allocated space for struct
	*index_data = NULL;//registration deleted ,space in Open files array is now empty/open
}
//===========================================SCAN_DATA MEMBER FUNCTIONS=========================================================
void SCAN_DATA_init(SCAN_DATA** scan_data,int fileDesc,int op,void *value ,int start_block){
	/*allocates space for and inititializes the registration for a 
	scan on the Open scans array*/
	*scan_data = (SCAN_DATA*)malloc(sizeof(SCAN_DATA));
	(*scan_data)->fileDesc = fileDesc;
	(*scan_data)->next_block = start_block;
	(*scan_data)->next_record = 0;
	(*scan_data)->overflow_block = -1;
	(*scan_data)->next_overflow_record = 0;
	(*scan_data)->op = op;
	(*scan_data)->value = malloc(Open_files[fileDesc]->f_len1);
	memcpy((*scan_data)->value,value,Open_files[fileDesc]->f_len1);
	
}

void SCAN_DATA_destroy(SCAN_DATA** scan_data){
	/*frees the space allocated for a registration on
	Open scans array*/
	free((*scan_data)->value);
	free(*scan_data);
	*scan_data = NULL;
}
//===========================================BLOCK_NUM MEMBER FUNCTIONS===================================================

void BLOCK_NUM_Init(BLOCK_NUM **block_num, int no, BLOCK_NUM *previous_top_num){
	/*allocates space for a node and initializes its members.Member no is initialized 
	by the number of the block pushed in the stack and the pointer is initialized 
	with the address of the previous block that was on the top of the stack.The first
	node pushed in the stack, which is also the bottom node , will have next_block_num
	pointing at NULL*/
	*block_num = (BLOCK_NUM*)malloc(sizeof(BLOCK_NUM));
	(*block_num)->no = no;
	(*block_num)->next_block_num = previous_top_num;
}

void BLOCK_NUM_Destroy(BLOCK_NUM **block_num){
	/*frees allocated space pointed at by *block_num*/
	free(*block_num);
}

int BLOCK_NUM_Getno(BLOCK_NUM *block_num){
	/*returns the number of the block stored in block_num*/
	return block_num->no;
}

BLOCK_NUM* BLOCK_NUM_Getnext(BLOCK_NUM *block_num){
	/*returns the address of the next node pointed at by next_block_num*/
	return block_num->next_block_num;
}

//============================================PATH MEMBER FUNCTIONS =======================================================

void PATH_Init(PATH **path){
	/*allocates space for the header and initializes its members 
	to represent an empty stack*/
	*path = (PATH*)malloc(sizeof(PATH));
	(*path)->block_total = 0;
	(*path)->top_block_num = NULL;
}

int PATH_Pop(PATH *path){
	/*retrieves and returns the block number stored in the top node of the stack
	deletes that node and stores the address of the new top node that 
	has surfaced.Decreases the number of nodes by 1*/
	int no;
	BLOCK_NUM *temp;
	no = BLOCK_NUM_Getno(path->top_block_num);
	temp = BLOCK_NUM_Getnext(path->top_block_num);
	BLOCK_NUM_Destroy(&path->top_block_num);
	(path->block_total)--;
	path->top_block_num = temp;
	return no;
}

void PATH_Push(PATH *path,int no){
	/*allocates a new node at the top of the stack to store the value of no.
	The new node becomes the top node and points at the previous top node.Also increments/updates
	the number of nodes in the stack by 1*/
	BLOCK_NUM *block_num;
	BLOCK_NUM_Init(&block_num,no,path->top_block_num);
	path->top_block_num = block_num;
	(path->block_total)++;
}

int PATH_NotEmpty(PATH *path){
	/*checks if the stack is empty.Stack is empty if there are no
	nodes in it*/
	return path->block_total;
}

void PATH_Destroy(PATH **path){
	/*destroys the stack by destroying(using pop) all the nodes from top
	to bottom and then the header*/ 
	while(PATH_NotEmpty(*path) != 0)
		PATH_Pop(*path);
	free(*path);
}



