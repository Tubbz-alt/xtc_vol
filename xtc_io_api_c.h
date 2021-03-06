/** \file xtc_io_api_c.h
 * @brief This file defines the interface and shared data structures between XTC2 iterator (C++) and HDF5 VOL (C).
 *
 *  Created on: Jan 20, 2020
 *      Author: tonglin
 */

 #include <stdint.h>

#ifndef XTC_IO_API_H_
#define XTC_IO_API_H_

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#define PATH_LEN_MAX 100

typedef void* xtc_func_t;

/* The xtc VOL info object */
//enum definition is identical with XtcData::DataType.
EXTERNC typedef enum XTC_Data_type{
    UINT8, UINT16, UINT32, UINT64,
    INT8, INT16, INT32, INT64,
    FLOAT, DOUBLE,
    CHARSTR,
    ENUMVAL, ENUMDICT
}xtc_data_type;

EXTERNC typedef enum XTC_Object_type{
    XTC_FILE,
    XTC_HEAD,//root group
    XTC_GROUP,
    XTC_DS,
    XTC_TIME_DS, // virtual dataset for time stamps
    XTC_LEAVE,
}xtc_object_type_t;

EXTERNC typedef enum EXECUTION_RETURN{
    SUCCESS,
    NOT_FOUND,
    RESERVED_1
}exec_ret;

//defined identical to H5O_token_t
typedef struct XTC_token {
    uint8_t __data[16];
} xtc_token_t;

EXTERNC typedef unsigned long long h5_size_t;

EXTERNC typedef struct Xtc_Location {
    int fd;
    void* fileIter; //XtcFileIterator* const
    void* dbgiter; //DebugIter* const
    void* dg; ////Dgram* dg = fileIter.next()
    void* root_node; //used in xtc_io_api.cc.
}xtc_location;

EXTERNC typedef enum Dataset_TYPE{
    DS_TIMESTAMP,
    DS_NON_L1,
    DS_L1
}ds_type;

//Used for dataset read
EXTERNC typedef struct dgram_info{
    uint64_t dgram_id;//Use timestamp as ID
    int64_t dgram_offset;
    int shapesdata_offset;//pointer offset
    int isL1;
    void* nonL1Dgram;
    ds_type ds_type;
    //uint64_t dgram_size;
}xtc_dgram_info;

EXTERNC typedef struct Xtc_dataset_handle{
    //uint64_t dgram_offset;
    uint64_t dgram_id;//Use timestamp as ID

    //uint64_t dgram_size;
    int index;
    //void* dgram;
    void* shapesData;
    void* namesLookup;
    xtc_dgram_info* dgram_info;
    //void* name;     //XtcData::Names
    //void* descdata;
    void* xtc_ptr; //Xtc* xtc, can be casted to descdata, shapesdata
}xtc_data_handle;



EXTERNC typedef struct XTC_DS_info {
    xtc_data_type type; //data type
    int dim_cnt;// for rank in H5Screate(rank, ), n of dimensions of dataspace.
    h5_size_t current_dims[6]; //1D array of size cnt_dimes,
    h5_size_t* maximum_dims; //Optional, maximum size of each dimension
    size_t total_pixel_cnt;

    int element_size;//similar to sizeof(type)
    int element_cnt; //image cnt
    xtc_data_handle* data_handle;
    void* data_ptr;
    ds_type ds_type;
    int isTimestampsDS;//bool
}xtc_ds_info;

EXTERNC typedef struct XTC_C_API_HELPER {
    int fd;
    int ref_cnt;
    char* obj_path_abs;
    char* obj_path_seg;
    exec_ret exec_state;//enum
    xtc_object_type_t obj_type;
    xtc_location* location;
    xtc_ds_info* ds_info;//info of dataset. Null for non-data-set objects.
    void* obj_token;//h5token, used as an UUID for this obj.
    void* tree_node;//points to it's node location in the tree
    void* data; //for timestamp DS now, but can be used for any additional data
}xtc_object;

//C API definition starts here
EXTERNC unsigned long xtc_h5token_new(xtc_token_t** token_in_out, unsigned int h5_token_size);

EXTERNC xtc_object* xtc_file_open(const char* path);//returns xtc_c_helper*
EXTERNC xtc_func_t xtc_it_open(void* param);// takes xtc_c_helper*
EXTERNC void xtc_file_close(xtc_object* helper);

EXTERNC size_t dataset_read_all(xtc_object* obj, void* buf_out);
//the parameter obj can be any valid obj in the tree.
EXTERNC xtc_object* xtc_obj_find(xtc_object* obj, const char* path);
EXTERNC xtc_object** xtc_get_children_list(xtc_object* group, int* num_out);
EXTERNC void extern_test_root(xtc_object* root_obj);
//used by  group_open() in XTC2 VOL
//EXTERNC xtc_object* outdated_xtc_path_search(xtc_object* file, char* path);



//C API definition ends here

#endif /* XTC_IO_API_H_ */
