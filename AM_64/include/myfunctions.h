#include "mystructs.h"
//====================================================GLOBAL VARIABLES =================================================================
extern int leaf_header_size;//leaf block header contains block type,no of entries,right neighbours number
extern int index_header_size;             //index block header contains info on block type and no of entries
extern int overflow_header_size;          //overflow block header contains info on block type and no of entries
//==========================================FUNCTION FOR TYPE AND LENGTH CHECKING=======================================================
int size_error(char type,int len);
//=======================================FUNCTION FOR DESCENDING TREE====================================================
int descend_tree(int fileDesc,void *value1,PATH *path);
	/*stores a path of block numbers ,from root_block to the 
	result data/leaf block, inside path array.Returns an empty path 
	if the tree is empty.Returns -1 for error 
	0 for success.*/
//============================================BLOCK ALLOCATION AND FORMATTING FUNCTIONS=======================================

int BlockCreate_Leaf(int fileDesc,BF_Block *block,int *block_no);
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        leaf type and returns its number descriptor in the variable block_no*/ 
int BlockCreate_Index(int fileDesc,BF_Block *block,int *block_no);
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        index type and returns its number descriptor in the variable block_no*/ 
int BlockCreate_Overflow(int fileDesc,BF_Block *block,int *block_no);
	/*takes fileDesc,an initialized block struct, and a variable for returning 
	the created blocks number descriptor.Allocates and initializes an empty new block of 
        overflow type and returns its number descriptor in the variable block_no*/ 
//====================================COMPARE FUNCTION (char,int,float)==============================================
float compare_key(void* value1,void* key,char type);
	/*compares value1 of the record to be inserted with the key of the 
	record that occupies the space on the block , returns 0 for equal values
	a positive integer if value1 is greater than key and a negative integer if value1 is less than key*/
//===================================GENERAL BLOCK FUNCTIONS=============================================
char get_block_type(char* data);
	/*returns the type of the block*/	
void write_block_type(char* data ,char type);
	/*stores the block type ,contained in type variable , in the header of the block*/	
int get_update_entries(char* data,int inc);
	/*increments the number of entries on the block by inc amount(can be 0).
	The value is stored in the header.Returns the number of entries.
	Can be called with 0 inc to get the current number of entries on the block*/
//==================================LEAF BLOCK SPECIFIC FUNCTIONS=================================================
void update_neighbour(char* data,int neighbour);
	/* updates the value corresponding to the number of the right neighbour*/	
int get_neighbour(char* data);
	/*retrieves the number of the current right neighbour of the block*/
void set_overflow_index(int fileDesc,char* data,int position,int overflowno);
	/*sets the overflow index of a record stored sizeof(int) bytes left of it*/
int get_overflow_index(int fileDesc,char* data,int position);
	/*returns the overflow index of a record stored sizeof(int) bytes left of it*/
void set_record(int fileDesc,char* data,int position,void* value1,void* value2);                    
	/*stores the record at position*/
void get_record(int fileDesc,char* data,int position,void* value1,void* value2);                       
	/*retrieves the record from position*/
int leaf_insert(int fileDesc,void* value1,void* value2,PATH* path,int *new_block_no,void *index_value);	
	/*if tree is empty it makes the first block ,otherwise it pushes the record depending on if its full
	or not.If it is,the middle value is stored in index value and given for insertion to index insert.
	returns 1 if split happens ,0 if not, -1 error*/
int Leaf_push(int fileDesc,char* data,void* carried_value,void *carried_value2);
	/*used if the original block is not full,places the record at the appropriate position pushing the remaining records to
	the right*/		
int Leaf_split_push(int fileDesc,char* old_data,char* new_data,void* carried_value,void *carried_value2,void *index_value);
	/*Used only if the original block is full and if the record is not duplicate.
	Inserts carried record  in the right position of the apropriate block,and afterwards pushes 
	the remaining records to the right ,meanwhile spliting all records of the original block.
	Returns -1 for error , else 0 for success.Stores 
	a value in index_value thats going to be inserted in the parent index block  */
int check_duplicate(int fileDesc,char *data,void *value1 ,void *value2);
	/*scans data buffer for a key that has value equal to value1
	if it finds one ,the record is pushed in an overflow block*/ 
void *find_next_leaf_entry(int scanDesc);
//========================================INDEX BLOCK SPECIFIC FUNCTIONS=========================================
void get_key(int fileDesc, char* data,int position ,void* key);
	/*returns the key value stored at position*/
void get_right(int fileDesc,char* data,int position,int *right);
	/*returns the value of the right index of the key located at position*/
void get_left(int fileDesc,char* data,int position,int *left);
	/*returns the value of the left index of the key located at position*/	
void set_key(int fileDesc,char* data,int position ,void* key);
	/*sets the key value at position*/
void set_right(int fileDesc,char* data,int position,int right);
	/*sets the value of te right index of the key located at position*/	
void set_left(int fileDesc,char* data,int position,int left);
	/*sets the value of the left index of the key located at position*/
int Index_push(int fileDesc,char* data,void* carried_value,int carried_right);
	/*Used only if the block in not full.Inserts carried value and carried right index in
	the right position of a block,and afterwards pushes the remaining records to the right ,if needed.
	returns -1 for error , else 0 for success  */
int Index_split_push(int fileDesc,char* old_data,char* new_data,void* carried_value,int carried_right,void *index_value);
	/*Used only if the original block is full.Inserts carried value and carried right index
	in the right position of the apropriate block,and afterwards pushes the remaining records 
	to the right ,if needed.Mean while moves half of the original entries to the new block.
	Returns -1 for error,else 0 for success.Stores a value in index_value
	that's going to be inserted on the parent index block  */	
int Index_make_root(int fileDesc,void* index_value,int new_block_no);
	/*creates an index block and makes it the root of the tree.Returns
	-1 for error, 0 for success*/
int index_insert(int fileDesc,void *index_value,PATH *path,int new_block_no);
	/*Called only after a leaf block has been split. Inserts index value and new block no in the appropriate block.If 
	a block is full ,a new one is made and the records are split between the two ,while the middle value becomes the index value for the 		higher level.If there is no index block in the tree or the index root has been split up it makes a new root.Returns -1 for error,
	0 for success.*/
//=======================================================OVERFLOW SPECIFIC FUNCTIONS============================================
void set_overflow_record(int fileDesc,char* data,int position,void* value1,void* value2);                  
	/*stores the record at position*/
void get_overflow_record(int fileDesc,char* data,int position,void* value1,void* value2);                     
	/*retrieves the record from position*/
int overflow_push(int fileDesc,char* leaf_data,int position,void* value1,void* value2);
	/* inserts a duplicate record at the corresponding overflow block.If there isn't an overflow block (-1 overflow index)
	it gets created.Returns -1 for error , 0 for success*/
void *find_next_overflow_entry(int scanDesc);
//=================================================================================================================
