#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

#include "AM.h"
#include "bf.h"
#include "defn.h"

#include "myfunctions.h"


#include <math.h>// ceil function
//===================================================GLOBAL VARIABLES ================================================================
int leaf_header_size = sizeof(char) + sizeof(int) + sizeof(int);//leaf block header contains block type,no of entries,right neighbours number
int index_header_size = sizeof(char) + sizeof(int);             //index block header contains info on block type and no of entries
int overflow_header_size = sizeof(char) + sizeof(int);          //overflow block header contains info on block type and no of entries
//==========================================FUNCTION FOR TYPE AND LENGTH CHECKING=======================================================
int size_error(char type,int len){
	/*checks the validity of the type and the corresponding length of a field*/
	if(type == 'i' && len!=sizeof(int)){           //checking length matches type
		AM_errno = AM_INVALID_FIELD_SIZE;
		return -1;
	}
	if(type == 'f' && len!=sizeof(float)){
		AM_errno = AM_INVALID_FIELD_SIZE;
		return -1;
	}
	if(len < 1){                                 //checking for valid length
		AM_errno = AM_INVALID_FIELD_SIZE;
		return -1;
	}
	if(type!= 'c' && type!='f' && type!='i'){//checking for valid type
		AM_errno = AM_INVALID_TYPE;
		return -1;
	}
	return 0;
}

//===========================================FUNCTION FOR DESCENDING TREE====================================================
int descend_tree(int fileDesc,void *value1,PATH *path){
	/*stores a path of block numbers ,from root_block to the 
	result data/leaf block, inside path array.Returns an empty path 
	if the tree is empty.Returns -1 for error 
	0 for success.*/
	BF_Block *block;
	char* block_data;
	char block_type;
	int keys_no,next_block_no,right,left,i;
	void *key;
	int len = Open_files[fileDesc]->f_len1;
	if(Open_files[fileDesc]->root_block == -1) //no tree blocks allocated yet ,tree is empty,insert leaf will create a root leaf
		return 0;
	key = malloc(len);
	BF_Block_Init(&block);
	next_block_no = Open_files[fileDesc]->root_block;//first block to be scanned will be root
	PATH_Push(path,next_block_no);//number of root is first element to be pushed in path
	AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,next_block_no,block);
	if(AM_errno != BF_OK)
		return -1;
	block_data = BF_Block_GetData(block);
	block_type = get_block_type(block_data);//retrieving block type
	while(block_type == 'i'){//descend through all index_blocks and stop when a leaf 'l' is reached
		keys_no = get_update_entries(block_data,0);//retrieving current number of entries of the block that is examined
		for(i = 0 ; i<= keys_no - 1 ; i++){//for all entries on the index_block ,searching for the first key greater than value1
			get_key(fileDesc, block_data,i ,key);      //retrieving the next key/entry at i position
			get_right(fileDesc,block_data,i,&right);   //its right index
			get_left(fileDesc,block_data,i,&left);	   //and its left index
			if(compare_key(value1,key,Open_files[fileDesc]->f_type1) < 0)//comparing key and value1
				break;
		}
		if(i <= keys_no-1){//found key (at i)  greater than value1
			next_block_no = left; //descending left index of i key
		}
		else{//value1 greater than all keys
			next_block_no = right;//descending right most index of the block(right index of the last key that was retrieved)
		}
		PATH_Push(path,next_block_no); // next block to be scanned pushed in the path
		AM_errno = BF_UnpinBlock(block);//current block no longer needed in memory
		if(AM_errno != BF_OK)
			return -1;
		AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,next_block_no,block);//retrieving next block to be scanned 
		if(AM_errno != BF_OK)
			return -1;
		block_data = BF_Block_GetData(block);//retrieving data of the next block
		block_type = get_block_type(block_data);//retrieving type of the next block
	}
	AM_errno = BF_UnpinBlock(block);//last block was a leaf type,while stopped, we need to unpin it 
	if(AM_errno != BF_OK)
		return -1;	
	BF_Block_Destroy(&block);
	free(key);	
	return 0;
}




//============================================BLOCK ALLOCATION AND FORMATTING FUNCTIONS=======================================

int BlockCreate_Leaf(int fileDesc,BF_Block *block,int *block_no){
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        leaf type and returns its number descriptor in the variable block_no*/ 
	char *block_data;
	int entriesno=0;

	AM_errno = BF_AllocateBlock(Open_files[fileDesc]->fileDesc,block); //allocating new block
	if(AM_errno != BF_OK)
		return -1;
	AM_errno = BF_GetBlockCounter(Open_files[fileDesc]->fileDesc,block_no);// retrieving total number of blocks in file 
	if(AM_errno != BF_OK)
		return -1;
	*block_no -= 1; //number of this block is (number of blocks in file - 1) 
	block_data = BF_Block_GetData(block);//data of the buffer to write into
	
        //=============== initializing header part of the block ====================
	memcpy(block_data + sizeof(char),&entriesno,sizeof(int));//number of entries on the block initiallized to 0
	write_block_type(block_data,'l');//type of the block
	update_neighbour(block_data,-1);//number of the current neighbour of the block ,-1 for none
                //============end of initialization============
	BF_Block_SetDirty(block);
	return AME_OK;
}


int BlockCreate_Index(int fileDesc,BF_Block *block,int *block_no){
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        index type and returns its number descriptor in the variable block_no*/ 
	char *block_data;
	int entriesno=0;

	AM_errno = BF_AllocateBlock(Open_files[fileDesc]->fileDesc,block); //allocating new block
	if(AM_errno != BF_OK)
		return -1;
	AM_errno = BF_GetBlockCounter(Open_files[fileDesc]->fileDesc,block_no);//checking number of blocks in file
	if(AM_errno != BF_OK)
		return -1;
	*block_no -= 1; //number descriptor of the block just created is the last block 
	block_data = BF_Block_GetData(block);//data of the buffer to write into
	
        //=============== initializing header part of the block ====================
	memcpy(block_data + sizeof(char),&entriesno,sizeof(int));
	write_block_type(block_data,'i');//type of the block
                //============end of initialization==============
	BF_Block_SetDirty(block);
	return AME_OK;
}

int BlockCreate_Overflow(int fileDesc,BF_Block *block,int *block_no){
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        overflow type and returns its number descriptor in the variable block_no*/ 
	char *block_data;
	int entriesno=0;

	AM_errno = BF_AllocateBlock(Open_files[fileDesc]->fileDesc,block); //allocating new block
	if(AM_errno != BF_OK)
		return -1;
	AM_errno = BF_GetBlockCounter(Open_files[fileDesc]->fileDesc,block_no);//checking number of blocks in file
	if(AM_errno != BF_OK)
		return -1;
	*block_no -= 1; //number descriptor of the block just created is the last block 
	block_data = BF_Block_GetData(block);//data of the buffer to write into
	
        //=============== initializing header part of the block ====================
	memcpy(block_data + sizeof(char),&entriesno,sizeof(int));
	write_block_type(block_data,'o');//type of the block
                //============end of initialization==============
	BF_Block_SetDirty(block);
	return AME_OK;
}

//====================================COMPARE FUNCTION (char,int,float)==============================================

float compare_key(void* value1,void* key,char type){
	/*compares value1 of the record to be inserted with the key of the 
	record that occupies the space on the block , returns 0 for equal values
	a positive integer if value1 is greater than key and a negative integer if value1 is less than key*/
	
	//integer compare
	if(type == 'i')
		return (float)(*(int*)value1 - *(int*)key);
	//string compare
	else if(type == 'c')
		return (float)strcmp((char*)value1,(char*)key);
	//float compare
	else//(type == 'f')
		return *(float*)value1 - *(float*)key;

}

//===================================GENERAL BLOCK FUNCTIONS=============================================
char get_block_type(char* data){
	/*returns the type of the block*/
	char type;
	memcpy(&type,data,sizeof(char));
	return type;
}
void write_block_type(char* data ,char type){
	/*stores the block type ,contained in type variable , in the header of the block*/
	memcpy(data,&type,sizeof(char));
}
int get_update_entries(char* data,int inc){
	/*increments the number of entries on the block by inc amount(can be 0).
	The value is stored in the header.Returns the number of entries.
	Can be called with 0 inc to get the current number of entries on the block*/
	int entriesno;
	memcpy(&entriesno,data + sizeof(char),sizeof(int));
	entriesno += inc;
	memcpy(data + sizeof(char),&entriesno,sizeof(int));
	return entriesno;
}

//==================================LEAF BLOCK SPECIFIC FUNCTIONS=================================================
void update_neighbour(char* data,int neighbour){
	/* updates the value corresponding to the number of the right neighbour*/
	memcpy(data + sizeof(char)+sizeof(int),&neighbour,sizeof(int));
}
int get_neighbour(char* data){
	/*retrieves the number of the current right neighbour of the block*/
	int neighbour;
	memcpy(&neighbour,data + sizeof(char) + sizeof(int),sizeof(int));
	return neighbour;
}

void set_overflow_index(int fileDesc,char* data,int position,int overflowno){
	/*sets the overflow index of a record stored sizeof(int) bytes left of it*/
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(data + leaf_header_size + position * (len1 + len2 + sizeof(int)),&overflowno,sizeof(int));
}

int get_overflow_index(int fileDesc,char* data,int position){
	/*returns the overflow index of a record stored sizeof(int) bytes left of it*/
	int index;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(&index,data + leaf_header_size + position * (len1 + len2 + sizeof(int)),sizeof(int));
	return index;
}

void set_record(int fileDesc,char* data,int position,void* value1,void* value2){                    
	/*stores the record at position*/
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(data + leaf_header_size + sizeof(int) + position * (len1 + len2 + sizeof(int)),value1,len1);
	memcpy(data + leaf_header_size + sizeof(int) + len1 + position * (len1 + len2 + sizeof(int)),value2,len2);
}
void get_record(int fileDesc,char* data,int position,void* value1,void* value2){                       
	/*retrieves the record from position*/
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(value1,data + leaf_header_size + sizeof(int) + position * (len1 + len2 + sizeof(int)),len1);
	memcpy(value2,data + leaf_header_size + sizeof(int) + len1 + position * (len1 + len2 + sizeof(int)),len2);
}

int leaf_insert(int fileDesc,void* value1,void* value2,PATH* path,int *new_block_no,void *index_value){	
	int all_entries,err,block_no,root_block_no,split,old_neighbour;
	char *old_data,*new_data,*root_data;
	void *carried_value,*carried_value2;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	int max_records = Open_files[fileDesc]->max_records;
	BF_Block *block,*newblock,*rootblock;
	BF_Block_Init(&block);
	BF_Block_Init(&newblock);
	carried_value = malloc(len1);//is the value carried until we find a greater value
	carried_value2 = malloc(len2);//is the value carried until we find a greater value
	split = 0;
	if(PATH_NotEmpty(path)==0){//tree is empty -> creating root
		BF_Block_Init(&rootblock);
		err = BlockCreate_Leaf(fileDesc,rootblock,&root_block_no);// creating leaf root
		if(err == -1)	
			return -1;
		root_data = BF_Block_GetData(rootblock);//retrieve data of root block
		set_record(fileDesc,root_data,0,value1,value2);//first entry in root
		set_overflow_index(fileDesc,root_data,0,-1);//no overflow block yet 
		update_neighbour(root_data,-1);//root has no neighbour
		get_update_entries(root_data,1);             //accounting for first entry
		BF_Block_SetDirty(rootblock);//root block has been modified
		AM_errno = BF_UnpinBlock(rootblock);
		if(AM_errno != BF_OK)
				return -1;
		Open_files[fileDesc]->root_block = root_block_no;//updating metadata with first root number
		BF_Block_Destroy(&rootblock); 

	}
	else{//path has at least one block number
		block_no = PATH_Pop(path);	//retrieving leaf block number from path 
		AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,block_no,block);//retrieving leaf block
		if(AM_errno != BF_OK)
			return -1;
		old_data = BF_Block_GetData(block);//data of the block retrieved
		all_entries = get_update_entries(old_data,0);//the number of all the entries in the block before any modification
		if(check_duplicate(fileDesc,old_data,value1 ,value2) == 0){//if value1 is not inserted already as duplicate continue insert
			memcpy(carried_value,value1,len1);//value1 and value2 are carried values , carried values are inserted when
			memcpy(carried_value2,value2,len2);//a key greater than carried_value is found
			if(all_entries == max_records){//block is full ,  split
				err = BlockCreate_Leaf(fileDesc,newblock,new_block_no);// creating block,this will be the right child of the
				if(err == -1)								//parent index block
					return -1;
				new_data = BF_Block_GetData(newblock);//data of the block created from the split
				old_neighbour = get_neighbour(old_data);
				update_neighbour(new_data,old_neighbour);//new block's right neighbour was the neighbour of the original block
				update_neighbour(old_data,*new_block_no);//the new block is the new right neighbour of the original block
				split = 1;//split == true
				Leaf_split_push(fileDesc,old_data,new_data,carried_value,carried_value2,index_value);//split records and insert
			}
			else
				Leaf_push(fileDesc,old_data,carried_value,carried_value2);//insert
			if(split == 1){//if there was a split ,a newblock has been allocated and modified
				BF_Block_SetDirty(newblock);        
				AM_errno = BF_UnpinBlock(newblock);
				if(AM_errno != BF_OK)
					return -1;
			}
		
		}
		BF_Block_SetDirty(block);//original block might have been modified(in check duplicate ,in overflow push if ov.index was -1)
		AM_errno = BF_UnpinBlock(block);
		if(AM_errno != BF_OK)
			return -1;
	}
	BF_Block_Destroy(&block);
	BF_Block_Destroy(&newblock);
	free(carried_value);
	free(carried_value2);
	return split;
}	

int Leaf_push(int fileDesc,char* data,void* carried_value,void *carried_value2){
	/*Used only if the block in not full and record is not duplicate.Inserts carried record in the right position 
	of a block,and afterwards pushes the remaining records to the right ,if needed.
	returns -1 for error , else 0 for success  */
	int overflow,carried_overflow,t,all_entries,i,res;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	void *key,*temp,*value2,*temp2;
	all_entries = get_update_entries(data,0);
	key = malloc(len1);
	temp = malloc(len1);
	value2 = malloc(len2);
	temp2 = malloc(len2);
	carried_overflow = -1;//new entry has no overflow block
	for (i = 0; i<= all_entries - 1;i++){ //for all entries in the block,retrieving entry and storing the lesser of the two
		get_record(fileDesc,data,i,key,value2);//retrieve record from position(offset) i
		overflow = get_overflow_index(fileDesc,data,i);//retrieve respective overflow index
		res = compare_key(carried_value,key,Open_files[fileDesc]->f_type1);
		if(res < 0){ //key is greater than carried_value ,always swapping for lesser key (at i position)to keep the records sorted
			memcpy(temp,key,len1);
			memcpy(key,carried_value,len1);
			memcpy(carried_value,temp,len1);
			memcpy(temp2,value2,len2);                     //swapping secondary values of the records
			memcpy(value2,carried_value2,len2);
			memcpy(carried_value2,temp2,len2);
			t = overflow;                                  //swapping overflow index values 
			overflow = carried_overflow;
			carried_overflow = t;
		}
		set_record(fileDesc,data,i,key,value2);
		set_overflow_index(fileDesc,data,i,overflow);		       
	}
	set_record(fileDesc,data,i,carried_value,carried_value2);//max key in carried value , inserted last at the end of the block
	set_overflow_index(fileDesc,data,i,carried_overflow);
	get_update_entries(data,1);             //accounting for the entry that was inserted
	free(key);
	free(temp);
	free(value2);
	free(temp2);
	return AME_OK;	
}	
		
int Leaf_split_push(int fileDesc,char* old_data,char* new_data,void* carried_value,void *carried_value2,void *index_value){
	/*Used only if the original block is full and if the record is not duplicate.
	Inserts carried record  in the right position of the apropriate block,and afterwards pushes 
	the remaining records to the right ,meanwhile spliting all records of the original block.
	Returns -1 for error , else 0 for success.Stores 
	a value in index_value thats going to be inserted in the parent index block  */
	int t,all_entries,i,k,old_entries_incr,new_entries_incr,overflow,carried_overflow,res;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	int max_records = Open_files[fileDesc]->max_records;
	float b = max_records;
	void *key,*temp,*value2,*temp2;
	all_entries = get_update_entries(old_data,0);
	key = malloc(len1);
	temp = malloc(len1);
	value2 = malloc(len2);
	temp2 = malloc(len2);
	carried_overflow = -1;//new entry has no overflow block
	k = 0;
	old_entries_incr = 0;//this keeps count of the entries removed from original block 
	new_entries_incr = 0;//this keeps count of the entries added to new block added to the new block
	for (i = 0; i<= all_entries - 1;i++){ //for all entries in the block,the lesser between carried and key is stored 
		get_record(fileDesc,old_data,i,key,value2);
		overflow = get_overflow_index(fileDesc,old_data,i);
		res = compare_key(carried_value,key,Open_files[fileDesc]->f_type1);
		if(res < 0){ //key > carried_value swap carried value  and key
			memcpy(temp,key,len1);
			memcpy(key,carried_value,len1);
			memcpy(carried_value,temp,len1);
			memcpy(temp2,value2,len2);                     //swapping secondary values of the records
			memcpy(value2,carried_value2,len2);
			memcpy(carried_value2,temp2,len2);
			t = overflow;                                  //swapping overflow index values 
			overflow = carried_overflow;
			carried_overflow = t;
		}
                //Deciding which block to store the entry(below),while also searching for middle key to push up to parent block
		if(i == ceil(b/2.0)){//middle key pushed up within index_value
			memcpy(index_value,key,len1);//found index value(will be pushed up to parent block)
			old_entries_incr -=1; 
			new_entries_incr +=1;//middle record is first entry to be removed from old block and added to new one
			set_record(fileDesc,new_data,k,key,value2);
			set_overflow_index(fileDesc,new_data,k,overflow);
			k++;//offset for entries in new block increased for the next entry
		}
		else if(i < ceil(b/2.0)){//place key in original block at i position , as long as the block is not half full
			set_record(fileDesc,old_data,i,key,value2);
			set_overflow_index(fileDesc,old_data,i,overflow);
		}
		else if(i > ceil(b/2.0)){//place key in new block at k position ,after the original block is half full
			old_entries_incr -= 1;//one entry removed from old block
			new_entries_incr +=1;	//new block gets one entry from old			
			set_record(fileDesc,new_data,k,key,value2);
			set_overflow_index(fileDesc,new_data,k,overflow);
			k++;
		}
	}//the max element is  inserted at the end of the new block (after entries - 1 steps as the extra step from inserting a value)
	set_record(fileDesc,new_data,k,carried_value,carried_value2);
	set_overflow_index(fileDesc,new_data,k,carried_overflow);
	new_entries_incr += 1;         // +1 entry for the last element
	get_update_entries(new_data,new_entries_incr);//updating number of entries 
	get_update_entries(old_data,old_entries_incr);//for both blocks
	free(key);
	free(temp);
	free(value2);
	free(temp2);
	return AME_OK;
	
}

int check_duplicate(int fileDesc,char *data,void *value1 ,void *value2){
	/*scans data buffer for a key that has value equal to value1
	if it finds one ,the record is pushed in an overflow block*/
	int i ,entries_no;
	void *key,*key_value2;
	key = malloc(Open_files[fileDesc]->f_len1);
	key_value2 = malloc(Open_files[fileDesc]->f_len2);
	entries_no = get_update_entries(data,0);//retrieving number of entries of the block
	for(i = 0; i <= entries_no - 1 ; i++){//scan all entries for a duplicate key
		get_record(fileDesc,data,i,key,key_value2);//get next record on data
		if(compare_key(value1,key,Open_files[fileDesc]->f_type1) == 0){//there is a duplicate key
			overflow_push(fileDesc,data,i,value1,value2);//insert value at the corresponding overlflow block
			free(key);
			free(key_value2);
			return 1;//1 indicates duplicate has been found and record has been inserted
		}
	}
	free(key);
	free(key_value2);
	return 0;//duplicate has not been found
}
		
void *find_next_leaf_entry(int scanDesc){
	/*continuously scans blocks until there is a record match or until 
	there are no more data blocks.If match is found sets info needed for next scan
	and returns the requested value,else returns NULL*/
	int fileDesc = Open_scans[scanDesc]->fileDesc;
	int entries_no,i,flag,res;
	int op = Open_scans[scanDesc]->op;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;	
	int next_block = Open_scans[scanDesc]->next_block;
	int next_record = Open_scans[scanDesc]->next_record;
	BF_Block *block;
	char* block_data;
	void *value,*v1,*v2;
	BF_Block_Init(&block);
	v1 = malloc(len1);
	v2 = malloc(len2);
	value = Open_scans[scanDesc]->value;
	flag = 0;
	while (next_block != -1){//block to be opened exists
		AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,next_block,block);//get the block
		if(AM_errno != BF_OK)
			return NULL;
		block_data = BF_Block_GetData(block);
		entries_no = get_update_entries(block_data,0);//get the number of total entries
		for(i = next_record ; i<= entries_no -1;i++){//scan through all of its entries or until match is found
			get_record(fileDesc,block_data,i,v1,v2);//retrieving record to check if it matches the op's requirements
			res = compare_key(v1,value,Open_files[fileDesc]->f_type1);//compare value with the key of the entry
			if(res == 0 && (op == 1 || op == 5 || op == 6)){// EQUAL , res fullfills requirements of the operator
				flag = 1;//return the corresponding value 2
				break;
			}
			else if( res < 0 && (op == 2 || op == 3 || op == 5)){//LESS OR NOT EQUAL
				flag = 1;
				break;
			}
			else if(res > 0 && (op == 2 || op == 4 || op == 6)){ // GREATER OR NOT EQUAL
				flag = 1;
				break;
			}
		}//requirements fullfilled or there are no more entries in block
		if(i >= entries_no - 1){ //if all records are scanned jump to neighbouring block's first record(i reached last entry)
			next_block = get_neighbour(block_data);//neighbouring block will be scanned next,if -1 there is no next block,scan ends
			Open_scans[scanDesc]->next_block = next_block;
			next_record = 0;	//position of the first record in neighbour block
			Open_scans[scanDesc]->next_record = next_record;
		}
		else if(i <entries_no - 1){//there are more records to be scanned after i,in the same block(i didn't reach last entry)
			next_record = i + 1; //then go to next record
			Open_scans[scanDesc]->next_record = i + 1;
		}
		AM_errno = BF_UnpinBlock(block);
		if(AM_errno != BF_OK)
			return NULL;
		if(flag == 1){//if op match found return v2 and exit function
			Open_scans[scanDesc]->overflow_block=get_overflow_index(fileDesc,block_data,i);//next call on overflow block if exists 
			BF_Block_Destroy(&block);
			free(v1);
			return v2;
		}
	}
	free(v1);
	free(v2);
	BF_Block_Destroy(&block);
	AM_errno = AME_EOF;
	return NULL;
}
		
		
	
	
	 
//========================================INDEX BLOCK SPECIFIC FUNCTIONS=========================================

void get_key(int fileDesc, char* data,int position ,void* key){
	/*returns the key value stored at position*/
	int len = Open_files[fileDesc]->f_len1;
	memcpy(key,data + index_header_size + sizeof(int) + (len + sizeof(int)) * position,len);
}
void get_right(int fileDesc,char* data,int position,int *right){
	/*returns the value of the right index of the key located at position*/
	int key_len = Open_files[fileDesc]->f_len1;
	memcpy(right,data + index_header_size + (key_len + sizeof(int)) * (position + 1),sizeof(int));
}
void get_left(int fileDesc,char* data,int position,int *left){
	/*returns the value of the left index of the key located at position*/
	int key_len = Open_files[fileDesc]->f_len1;
	memcpy(left,data + index_header_size + (key_len + sizeof(int)) * position,sizeof(int));
}
void set_key(int fileDesc,char* data,int position ,void* key){
	/*sets the key value at position*/
	int len = Open_files[fileDesc]->f_len1;
	memcpy(data + index_header_size + sizeof(int) + (len + sizeof(int)) * position,key,len);
}

void set_right(int fileDesc,char* data,int position,int right){
	/*sets the value of te right index of the key located at position*/
	int key_len = Open_files[fileDesc]->f_len1;
	memcpy(data + index_header_size + (key_len + sizeof(int)) * (position + 1),&right,sizeof(int));
}
void set_left(int fileDesc,char* data,int position,int left){
	/*sets the value of the left index of the key located at position*/
	int key_len = Open_files[fileDesc]->f_len1;
	memcpy(data + index_header_size + (key_len + sizeof(int)) * position,&left,sizeof(int));
}

int Index_push(int fileDesc,char* data,void* carried_value,int carried_right){
	/*Used only if the block in not full.Inserts carried value and carried right index in
	the right position of a block,and afterwards pushes the remaining records to the right ,if needed.
	returns -1 for error , else 0 for success  */
	int right,t,all_entries,i;
	int len = Open_files[fileDesc]->f_len1;
	void *key,*temp;
	all_entries = get_update_entries(data,0);
	key = malloc(len);
	temp = malloc(len);
	for (i = 0; i<= all_entries - 1;i++){ //for all entries in the block,retrieving entry and storing the lesser of the two
		get_key(fileDesc,data,i,key);
		get_right(fileDesc,data,i,&right);
		if(compare_key(carried_value,key,Open_files[fileDesc]->f_type1)<0){ //key > carried_value swap carried and key
			memcpy(temp,key,len);
			memcpy(key,carried_value,len);
			memcpy(carried_value,temp,len);
			t = right;                                  //swapping right index values ,left indeces never change
			right = carried_right;
			carried_right = t;
		}
		set_key(fileDesc,data,i,key); //place key at i position
		set_right(fileDesc,data,i,right);		       
	}
	set_key(fileDesc,data,i ,carried_value);//max key in carried value , inserted last at the end of the block
	set_right(fileDesc,data,i,carried_right);
	get_update_entries(data,1);             //accounting for the entry that was inserted
	free(key);
	free(temp);
	return AME_OK;
}

int Index_split_push(int fileDesc,char* old_data,char* new_data,void* carried_value,int carried_right,void *index_value){
	/*Used only if the original block is full.Inserts carried value and carried right index
	in the right position of the apropriate block,and afterwards pushes the remaining records 
	to the right ,if needed.Mean while moves half of the original entries to the new block.
	Returns -1 for error,else 0 for success.Stores a value in index_value
	that's going to be inserted on the parent index block  */
	int right,t,all_entries,i,k,old_entries_incr,new_entries_incr;
	int len = Open_files[fileDesc]->f_len1;
	int max_records = Open_files[fileDesc]->max_records;
	float b = max_records;//number of pointers per index block equals the number of max records per leaf block
	void *key,*temp;
	all_entries = get_update_entries(old_data,0);
	key = malloc(len);
	temp = malloc(len);
	k = 0;
	old_entries_incr = 0;
	new_entries_incr = 0;
	for (i = 0; i<= all_entries - 1;i++){ //for all entries in the block,the lesser between carried and key is stored 
		get_key(fileDesc, old_data,i ,key);
 		get_right(fileDesc,old_data,i,&right);
		if(compare_key(carried_value,key,Open_files[fileDesc]->f_type1)<0){ //key > carried_value swap values
			memcpy(temp,key,len);
			memcpy(key,carried_value,len);
			memcpy(carried_value,temp,len);
			t = right;                                                 //swapping right index values too
			right = carried_right;
			carried_right = t;
		}
		if(i == ceil(b/2.0) - 1){//middle key deleted and pushed up within index_value
			memcpy(index_value,key,len);
			old_entries_incr -=1; //middle key removed 
			set_left(fileDesc,new_data,0,right);//first index of new block==right index of middle key
		}
		else if(i < ceil(b/2.0) - 1){//place key in original block at i position , as long as the block is not half full
			set_key(fileDesc,old_data,i ,key);
			set_right(fileDesc,old_data,i,right);
		}
		else if(i > ceil(b/2.0) - 1){//place key in new block at k position ,after the original block is half full
			old_entries_incr -= 1;//one entry removed from old block
			new_entries_incr +=1;	//new block gets one entry from old			
			set_key(fileDesc,new_data,k ,key);
			set_right(fileDesc,new_data,k,right);
			k++;
		}
	}//the max element is  inserted at the end of the new block (after entries - 1 steps as the extra step from inserting a value)
	set_key(fileDesc,new_data,k ,carried_value);
	set_right(fileDesc,new_data,k,carried_right);
	new_entries_incr += 1;         // +1 entry for the last element
	get_update_entries(new_data,new_entries_incr);//updating number of entries 
	get_update_entries(old_data,old_entries_incr);//for both blocks
	free(key);
	free(temp);
	return AME_OK;
	
}
	
int Index_make_root(int fileDesc,void* index_value,int new_block_no){
	/*creates an index block and makes it the root of the tree.Returns
	-1 for error, 0 for success*/
	char* root_data;
	BF_Block *root_block;
	int root_block_no,err;
	BF_Block_Init(&root_block);
	err = BlockCreate_Index(fileDesc,root_block,&root_block_no);
	if(err == -1)
		return -1;
	root_data = BF_Block_GetData(root_block);
	set_left(fileDesc,root_data,0,Open_files[fileDesc]->root_block);//left index of root block is the previous root 
	set_key(fileDesc,root_data,0,index_value);//first key of the block
	set_right(fileDesc,root_data,0,new_block_no);//right index is number of block created from last split
	get_update_entries(root_data,1);//increment number of entries by one , first entry
	Open_files[fileDesc]->root_block = root_block_no;//this is the new root block of the file
	BF_Block_SetDirty(root_block);//block modified
	AM_errno = BF_UnpinBlock(root_block);
	if(AM_errno != BF_OK)
			return -1;
	BF_Block_Destroy(&root_block);
	return AME_OK;
}

int index_insert(int fileDesc,void *index_value,PATH *path,int new_block_no){
	/*Called only after a leaf block has been split. Inserts index value and new block no in the appropriate block.If 
	a block is full ,a new one is made and the records are split between the two ,while the middle value becomes the index value for the 		higher level.If there is no index block in the tree or the index root has been split up it makes a new root.Returns -1 for error,
	0 for success.*/
	int all_entries,err,block_no,split,carried_right;
	char *old_data,*new_data;
	void *carried_value;
	int len = Open_files[fileDesc]->f_len1;
	int max_records = Open_files[fileDesc]->max_records;
	BF_Block *block,*newblock;
	BF_Block_Init(&block);
	BF_Block_Init(&newblock);
	carried_value = malloc(len);//is the value carried until we find a greater value
	split = 1;//leaf block was split
	//if a leaf root has been split ,while is not entered ,because there was no other element in path other than the poped leaf number
	while(PATH_NotEmpty(path) && split == 1){//if there is an index block in path and there has been a split in the level below
		block_no = PATH_Pop(path);				//insert index_value and new_block_no in the new level
		memcpy(carried_value,index_value,len);//value to be inserted is carried
		carried_right = new_block_no;	//right index of value to be inserted
		split = 0;			//indicates whether a split has happened on a block
		AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,block_no,block);//this is the block before the split
		if(AM_errno != BF_OK)
			return -1;
		old_data = BF_Block_GetData(block);//data of the block before the split
		all_entries = get_update_entries(old_data,0);//the number of all the entries in the block before any modification
		if(all_entries == max_records - 1){//limit of stored keys reached(#pointers == max_records)
			err = BlockCreate_Index(fileDesc,newblock,&new_block_no);// creating new block,new_block_no will be pushed up for index
			if(err == -1)								//in parent block
				return -1;
			new_data = BF_Block_GetData(newblock);//data of the block created from the split	
			split = 1;//split == true
			Index_split_push(fileDesc,old_data,new_data,carried_value,carried_right,index_value);//inserting value,split == true
		}
		else
			Index_push(fileDesc,old_data,carried_value,carried_right);//inserting value in original block,split == false
		if(split == 1){//if there was a split newblock has been allocated and modified
			BF_Block_SetDirty(newblock);        
			AM_errno = BF_UnpinBlock(newblock);
			if(AM_errno != BF_OK)
				return -1;
		}
		BF_Block_SetDirty(block);//original block has been modified
		AM_errno = BF_UnpinBlock(block);
		if(AM_errno != BF_OK)
				return -1;
	}
	if(split == 1){//either an index root(during the while loop) or a leaf root(meaning path was empty before the loop) has been split up 
		err = Index_make_root(fileDesc,index_value,new_block_no);   //make an index block that is root
		if(err == -1)
			return -1;
	}
	BF_Block_Destroy(&block);
	BF_Block_Destroy(&newblock);
	free(carried_value);
	return AME_OK;
}

//=======================================================OVERFLOW SPECIFIC FUNCTIONS============================================
void set_overflow_record(int fileDesc,char* data,int position,void* value1,void* value2){                    
	/*stores the record at position*/
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(data + overflow_header_size + position * (len1 + len2),value1,len1);
	memcpy(data + overflow_header_size + len1 + position * (len1 + len2),value2,len2);
}
void get_overflow_record(int fileDesc,char* data,int position,void* value1,void* value2){                       
	/*retrieves the record from position*/
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	memcpy(value1,data + overflow_header_size + position * (len1 + len2),len1);
	memcpy(value2,data + overflow_header_size + len1 + position * (len1 + len2),len2);
}
int overflow_push(int fileDesc,char* leaf_data,int position,void* value1,void* value2){
	/* inserts a duplicate record at the corresponding overflow block.If there isn't an overflow block (-1 overflow index)
	it gets created.Returns -1 for error , 0 for success*/
	BF_Block *block;
	char* block_data;
	int entries,err,block_no;					
	BF_Block_Init(&block);                                               //initializing struct for block
	block_no = get_overflow_index(fileDesc,leaf_data,position);            //retrieving overflow index from the buffer of the leaf block
	if(block_no == -1){							//if there is no overflow block pointed at
		err = BlockCreate_Overflow(fileDesc,block,&block_no);		//create a new overflow block
		if(err == -1)
			return -1;
		set_overflow_index(fileDesc,leaf_data,position,block_no);//and set the index to point at this block
		//BF_Block_SetDirty(leaf_block);//leaf block's index to overflow block has been modified//ekswterika<========sthn insert leaf
		
	}
	else{                                                                    //else if there is an overflow block pointed at
		AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,block_no,block);		//bring it to memory
		if(AM_errno != BF_OK)
			return -1;
	}	
	block_data = BF_Block_GetData(block);                                    
	entries = get_update_entries(block_data,0);                                //get the number of entries
	set_overflow_record(fileDesc,block_data,entries,value1,value2);//insert duplicate record(e.g. 0 entries -> 0 offset)
	get_update_entries(block_data,1);                                         //update the number of entries
	BF_Block_SetDirty(block);						//block has been modified
	AM_errno = BF_UnpinBlock(block);					//done working on this block
	if(AM_errno != BF_OK)
		return -1;
	BF_Block_Destroy(&block);                                               //destroy the struct
	return AME_OK;
}

void *find_next_overflow_entry(int scanDesc){
	/*retrieves the second value of the next duplicate record stored
	in the overflow block , modifies overflow block and next overflow record
	accordingly(for the next scan) and returns the value*/
	int next_overflow_record = Open_scans[scanDesc]->next_overflow_record;
	int entries_no;
	int fileDesc = Open_scans[scanDesc]->fileDesc;
	int len1 = Open_files[fileDesc]->f_len1;
	int len2 = Open_files[fileDesc]->f_len2;
	char* data;
	void *value1,*value2;
	BF_Block *block;
	BF_Block_Init(&block);
	value1 = malloc(len1);
	value2 = malloc(len2);//allocating space for storing the requested value
	AM_errno = BF_GetBlock(Open_files[fileDesc]->fileDesc,Open_scans[scanDesc]->overflow_block,block);//retrieve the overflow block
	if(AM_errno != BF_OK)
		return NULL;
	data = BF_Block_GetData(block);
	get_overflow_record(fileDesc,data,Open_scans[scanDesc]->next_overflow_record,value1,value2);//retrieve current records value2
	entries_no = get_update_entries(data,0);//retrieve the number of total entries on this block
	if(next_overflow_record == entries_no - 1){//reached last entry for this block
		Open_scans[scanDesc]->overflow_block = -1;// next scan continues searching on leaf blocks
		Open_scans[scanDesc]->next_overflow_record = 0;//initiallizing record number for the next time an overflow block is found
	}
	else//haven't reached last entry yet
		(Open_scans[scanDesc]->next_overflow_record)++;//move to the next entry of this block(info needed for the next scan)
	AM_errno = BF_UnpinBlock(block);//block no longer needed in memory
	if(AM_errno != BF_OK)
		return NULL;
	free(value1);
	BF_Block_Destroy(&block);
	return value2;//returned retrieved value2
}


