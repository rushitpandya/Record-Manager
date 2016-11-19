________________

Record Manager
________________

Team Members
_____________

Rushit Pandya (Leader) - A20381916 - rpandya4@hawk.iit.edu
Subhdeep Roy - A20358508 - sroy7@hawk.iit.edu
Dutt Patel - A20380571 - dpatel106@hawk.iit.edu
Rumin Shah - A20369998 - rshah90@hawk.iit.edu
__________

File List
__________

README.txt
dberror.c
dberror.h
storage_mgr.c
storage_mgr.h
test_helper.h
Makefile
buffer_mgr.c
buffer_mgr_stat.c
test_assign3_1.c
test_assign3_2.c
buffer_mgr.h
buffer_mgr_stat.h
dt.h
expr.c
expr.h
record_mgr.c
record_mgr.h
rm_serializer.c
test_expr.c
mgmt.h
tables.h
____

Aim
____

To implement record manager that can handle tables with a given fixed schema. The record manager makes use of the buffer manager in order to access pages of file as each table is stored in a separate page file. User can use the facility of inserting, deleting,  updating records as well as scanning records in a table.  Scanning the records uses search conditions and hence it only retrieves those records that matches the given search condition.  
_________________________

Installation Instruction
_________________________

1. Go the path in the command prompt to the directory where the extracted files of zip folder named "assign3_group27.zip" are present.
2. To remove object files, run command: make clean
3. Run the command: make
4. Run the command to test test_assign3_1.c: ./test_assign3
5. Run the command to test test_expr.c: ./test_expr
6. Run the command to test extra test cases: test_assign3_2.c: ./test_assign3_2 
________________________________

Optional Extensions Implemented
________________________________

1. TIDs and Tombstone
Tombstone is used specially when a record is deleted. It is also used in functionalities like insert and update. We have created an array named tombstone for the implementation and set its value as "deleted". Whenever a record is deleted, we set that value as "deleted". So when the record is later inserted or updated, it comes to know that a space is empty when it encounters the tombstone variable set to "deleted".

2. Check Primary Key Constraint
The functions in record_mgr.c file are taking care of primary keys. In our case, we have used a structure to take care of primary keys. When a record is to be inserted, we cross check the primary key of that record with that of already existing keys. If the same key value is observed in the structure then appropriate error message will be returned. 
_______________________

Functions Descriptions
_______________________
___________________________________

Table and Record Manager Functions
___________________________________

initRecordManager()
This function initializes the record manager. If everything goes right then it returns RC_OK message.

shutdownRecordManager()
This function shuts down the record manager. If everything goes right then it returns RC_OK message.

createTable()
This function creates a table which is basically a page file. Once created, it is opened and the schema is serialized. The data in the page is written to file handler and the memory is made free.

openTable()
This function is used to open the table and initialize the buffer pool and record management structure. Once done, it deserializes the schema and returns appropriate message.

closeTable()
This function is used to shut down the opened buffer pool and close the table. Also, it frees the memory by making it null and closes the table.

deleteTable()
This function is used to delete the opened table or page file by calling destroyPageFile() function of storage manager.

getNumTuples()
This function is used to get the number or count of the tuple in a given relation. The function starts with first page of the relation and a count variable set to 0. The getRecord() function is used to get the record from the page if it exists. If it exists then the count variable is increased by 1. When the end of the relation is reached, the count variable is returned and the memory is set to free.
_________________

Record Functions
_________________

insertRecord()
This function is used to insert records into a given relation. It checks for soft delete after scanning the table and obtaining record and see if it finds a tombstone. If it does then the memory is made free and the page number is determined on which the record will be written. Since content will get updated, it pins the page, marks it dirty and write the content into the page. This function satisfies primary key constraint.

deleteRecord()
This function is used to delete the record. When a record is deleted, it creates a tombstone. This function also scans whole page file and pin the page that has the record. It then sets a flag of tombstone and marks the page dirty and then unpins the page. It also makes use of forcePage() function of buffer manager to free up the memory page. In case of any errors like no record found, it will throw appropriate error message. This function satisfies primary key constraint.

updateRecord()
This function is used to update data in the relation. It takes in relation and the record to be updated and finds the same record from the given relation. It then updates the record with given data. This function satisfies primary key constraint.

getRecord()
This function is used to get the desired record from the given relation. First it scans through all the records in the relation and when it gets the desired record, it pins the page and stores the ID and data of that record and then unpins the page. Appropriate error messages are returned by the function.

_______________

Scan Functions
_______________

startScan()
This function is used to initialize data to be scanned and puts them into a relation in order to retrieve those tuples that fulfils any given condition.

next()
This function is used to get the tuple that fulfils given condition.

closeScan()
This function is used to close scanning by making the scan data null and freeing that memory. 
_________________

Schema Functions
_________________

getRecordSize()
This function is used to get the size of the record.

createSchema()
This function is used to create schema and setting their attributes to a given value.

freeSchema()
This function is used to free attributes of schema. Appropriate error messages are returned after its execution.
____________________

Attribute Functions
____________________

createRecord()
This function is used to create records. This function allocates memory to the newly created record after it knows the required size of that record. It then adds the record to a record list.

freeRecord()
This function is used to free the memory occupied by the record that is to be deleted.

getAttr()
This function is used to get the attributes of any given record.

setAttr()
This function is used to set attribute value to Boolean, String, Float or Integer as required by that particular record.
____________________________

Extra Functions Implemented
____________________________

AssignRecodMemory()
This function gives the memory required by a given schema in terms of bytes. The memory depends on components like number of attributes and their respective data type.

serializeSchema()
This function is used to serialize the schema by taking in the structure of the object and converting its data type into string and then writing to the file. It allocates memory to the schema based on its requirement and sends the newly formatted data to a structure variable. 

deserializeSchema()
This function deserializes the schema by reading data from the structure variable. It then deserialize into schema objects based on respective data type of the object.

validatePKey()
This function is used to check primary key.

removePKey()
This function is used to remove primary key.
_______________________

Additional Error Codes 
_______________________

The error codes below were updated or added in dberror.h file:
RC_EMPTY_TABLE 17 
RC_INVALID_CONDITION 18 
RC_RM_ERROR_CREATING_TABLE 19 
RC_RM_NO_RECORD_FOUND 20 
RC_RM_NO_TABLE_INIT 21
___________________________________________________

Extra Structures Implemented In mgmt.h Header File
___________________________________________________

RM_RecordPoolInfo
This is a structure of all records that will be created.

RM_ScanPool
This structure is used for scan operations.

Record_PKey
It is a hashing structure created to check the primary key constraint. 
_____________

Extra Credit 
_____________

1. Implementation of Tombstone ID or TID and Tombstone.
2. Check Primary Key Constraint.
_________________

Additional Files
_________________

test_assign3_2.c
This file contains additional test cases for testing. Additional test cases includes implementation of TID and Tombstone and checking primary key constraint.
________________

No Memory Leaks 
________________

All the test cases are tested with Valgrind tool and no memory leaks exists. 
____________

Test Cases 
____________

testTombstoneImpl()
This test case function tests the tombstone implementation.

testPrimaryKeyImpl()
This test case function tests the primary key constraints.
