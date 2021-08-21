#define OPEN_FILES_SIZE 20       //max registrations on Open files array
#define OPEN_SCANS_SIZE 20       //max registrations on Open scans array
//================================================ STRUCTS=========================================================

typedef struct BLOCK_NUM{
	/*this is a node struct for storing the number 
	of a block in a file and a pointer to the next node*/
	int no;
	struct BLOCK_NUM* next_block_num;
}BLOCK_NUM;
				/*member functions*/

void BLOCK_NUM_Init(BLOCK_NUM **block_num, int no, BLOCK_NUM *previous_top_num);
	/*allocates space for a node and initializes its members.Member no is initialized 
	by the number of the block pushed in the stack and the pointer is initialized 
	with the address of the previous block that was on the top of the stack.The first
	node pushed in the stack, which is also the bottom node , will have next_block_num
	pointing at NULL*/
void BLOCK_NUM_Destroy(BLOCK_NUM **block_num);
	/*frees allocated space pointed at by *block_num*/
int BLOCK_NUM_Getno(BLOCK_NUM *block_num);
	/*returns the number of the block stored in block_num*/
BLOCK_NUM* BLOCK_NUM_Getnext(BLOCK_NUM *block_num);
	/*returns the address of the next node pointed at by next_block_num*/

                      //================================

typedef struct PATH{
	/*this is the header of a stack of BLOCK_NUM nodes.It stores 
	the number of nodesand point at the top node of the stack*/
	int block_total;
	BLOCK_NUM *top_block_num;
}PATH;
				/*member functions*/
void PATH_Init(PATH **path);
	/*allocates space for the header and initializes its members 
	to represent an empty stack*/
int PATH_Pop(PATH *path);
	/*retrieves and returns the block number stored in the top node of the stack
	deletes that node and stores the address of the new top node that 
	has surfaced*/
void PATH_Push(PATH *path,int no);
	/*allocates a new node at the top of the stack to store the value of no.
	The new node becomes the top node and points at the previous top node.Also increments/updates
	the number of nodes in the stack by +1*/
int PATH_NotEmpty(PATH *path);
	/*checks if the stack is empty.Stack is empty if there are no
	nodes in it*/
void PATH_Destroy(PATH **path);
	/*destroys the stack by destroying(using pop) all the nodes from top
	to bottom and then the header*/ 

                //===============================

typedef struct INDEX_DATA{		
/*Initiallized in AM_OpenIndex.Retrieves and stores
metadata from block 0 for an open file instance*/

	int fileDesc;                   //this is the actual fileDescriptor returned from BF_CreateFile, 
	char* fileName;      		//name of file 
	int root_block;			//Descriptor for finding the root block initialized to -1 for none
	char f_type1,f_type2;		//types of field 1 and 2
	int  f_len1, f_len2;		//lengths of field 1 and 2
	int open_scans;			//the number of open scans on the file initiallized to 0,increased/decreased accordingly
	int max_records;                //the number of maximum records that can be stored in one data block
					//also the number of maximum pointers that can be stored in an index block
}INDEX_DATA;

			/*member functions*/

void INDEX_DATA_init(INDEX_DATA** index_data,int fileDesc,char* fileName,char* metadata);
/*Allocates space for the struct of the file's metadata and copies 
the metadata stored in the first block of the opened file initializing all the struct members
 .Sets pointer in Open files array.*/
void INDEX_DATA_destroy(INDEX_DATA** index_data,char* metadata);
/*Frees the space allocated for the struct and updates the metadata in 
the first block of the file*/

                 //================================

typedef struct SCAN_DATA{
/*this struct stores data for individual 
currently running index scans*/
	int fileDesc;                //this is the position(index) of the the opened file's metadata in Open_files array
	int next_block;		     //next scan will start from this block
	int next_record;	     //next scan will start from this record inside the specified block
	int overflow_block;          //if > 0 indicates the number of the overflow block that should be scanned next
	int next_overflow_record;    //indicates the number of the record in overflow block that should be scanned next
	void *value;
	int op;
}SCAN_DATA;
			/*member functions*/

void SCAN_DATA_init(SCAN_DATA** scan_data,int fileDesc,int op,void *value,int start_block);
/*stores the file descriptor the op and the value and sets the next block 
and the next record to be scanned to start_block and 0*/
void SCAN_DATA_destroy(SCAN_DATA** scan_data);
/*frees the space allocated for the scan data*/
//======================================================ERROR CODES ======================================================================
typedef enum AME_ErrorCode {
  AM_OK,
  AM_OPEN_FILES_LIMIT_ERROR,     /* Υπάρχουν ήδη BF_MAX_OPEN_FILES αρχεία ανοικτά */
  AM_INVALID_FILE_ERROR,         /* Ο αναγνωριστικός αριθμός αρχείου δεν αντιστιχεί σε κάποιο ανοιχτό αρχείο */
  AM_ACTIVE_ERROR,               /* Το επίπεδο BF είναι ενεργό και δεν μπορεί να αρχικοποιηθεί */
  AM_FILE_ALREADY_EXISTS,        /* Το αρχείο δεν μπορεί να δημιουργιθεί γιατι υπάρχει ήδη */
  AM_FULL_MEMORY_ERROR,          /* Η μνήμη έχει γεμίσει με ενεργά block */
  AM_INVALID_BLOCK_NUMBER_ERROR, /* Το block που ζητήθηκε δεν υπάρχει στο αρχείο */
  AM_ERROR,
  AM_FILE_OPEN,                  //can't destroy open file
  AM_REMOVE_FAILED,		//remove failed to delete file
  AM_FILES_ARRAY_LIMIT,          //max registration on open files array
  AM_INVALID_FILENAME,           //file named fileName does not exist
  AM_SCAN_RUNNING,               //unable to close file ,scan is running
  AM_INVALID_FILEDESC,            //filedesc array index is invalid
  AM_INVALID_SCANDESC,            //scandesc array index is invalid
  AM_TREE_EMPTY,                  //tree has no block to scan
  AM_SCANS_ARRAY_LIMIT,           //max registrations on open scans array
  AM_INVALID_FIELD_SIZE,          //AttrLength does not match AttrType
  AM_INVALID_TYPE                 //compare cannot find a match for this type
} AM_ErrorCode;
//=====================================================GLOBALS========================================================================
extern INDEX_DATA *Open_files[OPEN_FILES_SIZE];			//array of pointers to INDEX_DATA for the currently open files
extern SCAN_DATA *Open_scans[OPEN_SCANS_SIZE];                     //array of pointers to SCAN_DATA for the currently running scans

