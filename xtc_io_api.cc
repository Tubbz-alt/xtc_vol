#include <fcntl.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <unistd.h>


#include <xtcdata/xtc/XtcFileIterator.hh>
#include <xtcdata/xtc/XtcIterator.hh>
#include <xtcdata/xtc/ShapesData.hh>
#include <xtcdata/xtc/DescData.hh>

#include <vector>
#include <sys/time.h>
#include "xtc_io_api_c.h" //for extern C
#include "xtc_io_api_cpp.hh"
#include "xtc_tree.hh"

using namespace XtcData;
using namespace std;
using std::string;

typedef struct MappingLayer {
    void* xtc_root_it; //file root
    void* cur_it; //current xtc it, points to a xtc node.
    char* cur_it_name; // something like "/grp_l1/grp_l2/"
    void* iteration_stack; //
}mapping;

#define TIMESTAMP_DS_NAME "timestamps"
#define DEBUG_PRINT printf("%s():%d\n", __func__, __LINE__);
//H5 utility function prototype
//H5 name to xtc name, second
char* name_convert(const char* h5_name){
    char* xtc_name_surfix;

    return xtc_name_surfix;
}

//vector<string> _CURRENT_PATH_TOKENS;


// show name: xtc it -> h5 name string


// search/open: h5 name string -> xtc it BFS/DFS combined.

//==============================================

int check_update_dgram(int fd, xtc_dgram_info* dg_info, Dgram* current_dgram_in_out);

class SmdIter : public XtcIterator
{
public:
    enum { Stop, Continue };
    SmdIter() : XtcIterator()
    {
    }

    int process(Xtc* xtc)
    {
        switch (xtc->contains.id()) {
        case (TypeId::Parent): {
            iterate(xtc);
            break;
        }
        case (TypeId::Names): {
            Names& names = *(Names*)xtc;
            _namesLookup[names.namesId()] = NameIndex(names);
            break;
        }
        case (TypeId::ShapesData): {
            ShapesData& shapesdata = *(ShapesData*)xtc;
            // lookup the index of the names we are supposed to use
            NamesId namesId = shapesdata.namesId();
            // protect against the fact that this namesid
            // may not have a NamesLookup.  cpo thinks this
            // should be fatal, since it is a sign the xtc is "corrupted",
            // in some sense.
            if (_namesLookup.count(namesId)<=0) {
                printf("*** Corrupt xtc: namesid 0x%x not found in NamesLookup\n",(int)namesId);
                throw "invalid namesid";
                break;
            }
            DescData descdata(shapesdata, _namesLookup[namesId]);
            // see if the offset is in this ShapesData xtc
            Names& names = descdata.nameindex().names();
            for (unsigned i = 0; i < names.num(); i++) {
                Name& name = names.get(i);
                if (strcmp(name.name(),"intOffset")==0) {
                  offset    = descdata.get_value<uint64_t>(i);
                } else if (strcmp(name.name(),"intDgramSize")==0) {
                  dgramSize = descdata.get_value<uint64_t>(i);
                }
            }
            break;
        }
        default:
            break;
        }
        return Continue;
    }
    uint64_t offset;
    uint64_t dgramSize;

private:
    NamesLookup _namesLookup;
};

xtc_object* xtc_obj_new(int fd, void* fileIter, void* dbgiter, void* dg, const char* obj_path_abs);
xtc_object* xtc_obj_new(xtc_location* location, const char* obj_path_abs);

class DebugIter : public XtcIterator
{
public:
    enum { Stop, Continue };
    DebugIter() : XtcIterator()
    {

    }
    //enum DataType {UINT8, UINT16, UINT32, UINT64, INT8, INT16, INT32, INT64, FLOAT, DOUBLE, CHARSTR, ENUMVAL, ENUMDICT};
//    xtc_data_type to_xtc_type(Name::DataType native_type){
//        return native_type;
//    }

    xtc_ds_info* get_ds_info(int index, ShapesData* shapesdata, xtc_dgram_info &dgram_info){//int i Names& names,
        //ShapesData& shapesdata = *(ShapesData*)xtc;
        NamesId namesId = shapesdata->namesId();
        printf("get_ds_info: index = %d, shapesdata = %p, &shapesdata = %p, &(shapesdata*) = %p\n",
                index, shapesdata, &shapesdata, &(*shapesdata));
        DescData descdata(*shapesdata, _namesLookup[namesId]);
        DEBUG_PRINT
        Names& names = descdata.nameindex().names();


        xtc_ds_info* dataset_info = (xtc_ds_info*)calloc(1, sizeof(xtc_ds_info));
        Name& name = names.get(index);
        dataset_info->type = (xtc_data_type)(int)(name.type());
        dataset_info->dim_cnt = name.rank();//# of dimension: 0-5, how many dimensions of the shape:

        dataset_info->element_cnt = name.get_element_size(name.type());// how many elements

        //dataset_info->element_size = ;
        dataset_info->maximum_dims = NULL;

        uint32_t* shape = descdata.shape(name);

        dataset_info->total_pixel_cnt = 1;

//        printf("%s: xtc = %p, &(*xtc) = %p, index = %d, name = %s, type = %d, \n",
//                __func__, xtc, &(*xtc), index, name.name(), (int)(name.type()));

        if(name.type() == Name::CHARSTR){//single string
            dataset_info->dim_cnt = 1;
            dataset_info->current_dims[0] = 1;
        } else {
            for(int i = 0; i < name.rank(); i++)
                dataset_info->current_dims[i] = shape[i];
            //printf("dimesntion cnt = %d, shape dimensions: %d, %d, %d, %d, %d, %d\n", name.rank(), shape[0], shape[1], shape[2], shape[3], shape[4], shape[5]);
            if(dataset_info->dim_cnt > 0){
                printf("dim_cnt = %d, dimensions val: ", dataset_info->dim_cnt);
                for(int i = 0; i < dataset_info->dim_cnt; i++){
                    dataset_info->total_pixel_cnt *= dataset_info->current_dims[i];
                    printf("%d, ", dataset_info->current_dims[i]);
                }
                printf("\n");
            }//dim_cnt == 0
        }

        dataset_info->data_handle = (xtc_data_handle*)calloc(1, sizeof(xtc_data_handle));
        dataset_info->data_handle->dgram_id = dgram_info.dgram_id;
        dataset_info->data_handle->dgram = (void*) this->_current_dgram;
        dataset_info->data_handle->index = index;
        //dataset_info->data_handle->shapesData = (void*)(&shapesdata);//need to be cased to ShapesData**
        DEBUG_PRINT
        dataset_info->data_handle->shapesData = calloc(1, sizeof(void*));
        memcpy(dataset_info->data_handle->shapesData, &shapesdata, sizeof(void*));
        DEBUG_PRINT
        printf("get_ds_info: shapesdata = %p to data_handle->shapesData = %p\n",
                shapesdata, dataset_info->data_handle->shapesData);
        //= calloc(1, sizeof(void*));


        //dataset_info->data_handle->shapesData = (void*)(&shapesdata);
        DEBUG_PRINT
        printf("get_ds_info: index = %d, copied shapesdata address = %p\n", index, shapesdata);
        //dataset_info->data_handle->namesLookup = (void*) &_namesLookup;
        dataset_info->isTimestampsDS = 0;
        printf("get_ds_infodata_handle: xtc_ptr = %p\n",
                dataset_info->data_handle->xtc_ptr);

        return dataset_info;
    }

    //Names* parameter is not used.
    //int index, ShapesData* shapesdata, NamesLookup* lookup, void* data_out, size_t pixel_cnt
    size_t get_data(xtc_data_handle* data_handle, size_t pixel_cnt, void* data_out){//Names* namesd,
        assert(data_handle);

        if(!(this->_current_dgram)){
            this->_current_dgram = (Dgram*) data_handle->dgram;
        }
        printf("get_data: this = %p, this->_current_dgram = %p\n", this, this->_current_dgram);
        assert(this->_current_dgram);
        if(data_handle->dgram_id != this->_current_dgram->time.value()){//different dgram, need to load.
            unordered_map<uint64_t, xtc_dgram_info>::iterator it =_index_map->find(data_handle->dgram_id);
            if(it == _index_map->end()){
                printf("Can not find dgram info, id/timestamp = llu\n", data_handle->dgram_id);
                return -1;
            } else {
                //int check_update_dgram(int fd, xtc_dgram_info* dg_info, Dgram* current_dgram_in_out)
                printf(" Before: Current dgram id = %llu\n", _current_dgram->time.value());
                check_update_dgram(this->get_fd(), &(it->second), _current_dgram);
                printf(" After: Current dgram id = %llu\n", _current_dgram->time.value());
            }
        }

        ShapesData* shapesdata = (ShapesData*)data_handle->shapesData;
        int index = data_handle->index;
        //NamesLookup* lookup = (NamesLookup*)data_handle->lookup;

        printf("get_data() terminal data entry ============== shapesdata = %p, lookup = %p\n",
                shapesdata, &_namesLookup);
        assert(shapesdata);

        size_t pixel_size_out = 0;
        //ShapesData& shapesdata =  *(ShapesData*)xtc;
        NamesId namesId = shapesdata->namesId();

        DescData descdata = DescData(*shapesdata, _namesLookup[namesId]);
        Names& names = descdata.nameindex().names();
        Name& name = names.get(index);
        int data_rank = name.rank();
        int data_type = name.type();
        void* ret_data = NULL;
        char* tmp_buf = NULL;

        //printf("get_value() token = %s\n", name.name());
        //DEBUG_PRINT
        switch(name.type()){
        case(Name::UINT8):{
            assert(name.type() == xtc_data_type::UINT8);
            if(data_rank > 0){
                Array<uint8_t> arrT = descdata.get_array<uint8_t>(index);
                //
                /*
                auto t = arrT.shape();
                //rank == 3 5*6*7 is to describe the shape dimension of a element
                t[0] = 5
                  t[1]=6
                    t[2]=7
                  */
                //
                printf("type uint8_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data = arrT.data();
                    }
            else{
                printf("type uint8_t:  %s: %d\n",name.name(),descdata.get_value<uint8_t>(index));
                uint8_t val = descdata.get_value<uint8_t>(index);
                ret_data = &val;
            }
            printf("%d: pixel_size_out = %d\n", __LINE__, pixel_size_out);
            pixel_size_out = sizeof(uint8_t);
            printf("pixel_size_out = %d, sizeof(uint8_t) = %d\n", pixel_size_out, sizeof(uint8_t));
            break;
        }

        case(Name::UINT16):{
            assert(name.type() == xtc_data_type::UINT16);
            if(data_rank > 0){
                Array<uint16_t> arrT = descdata.get_array<uint16_t>(index);
                printf("type uint16_t:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type uint16_t: %s: %d\n",name.name(),descdata.get_value<uint16_t>(index));
                uint16_t val = descdata.get_value<uint16_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(uint16_t);
            printf("pixel_size_out = %d, sizeof(uint16_t) = %d\n", pixel_size_out, sizeof(uint16_t));
            break;
        }

        case(Name::UINT32):{
            assert(name.type() == xtc_data_type::UINT32);
            if(data_rank > 0){
                Array<uint32_t> arrT = descdata.get_array<uint32_t>(index);
                printf("type uint32_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type uint32_t: %s: %d\n",name.name(), descdata.get_value<uint32_t>(index));
                uint32_t val = descdata.get_value<uint32_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(uint32_t);
            printf("pixel_size_out = %d, sizeof(uint32_t) = %d\n", pixel_size_out, sizeof(uint32_t));
            break;
        }

        case(Name::UINT64):{
            assert(name.type() == xtc_data_type::UINT64);
            if(data_rank > 0){
                Array<uint64_t> arrT = descdata.get_array<uint64_t>(index);
                printf("type uint64_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type uint64_t %s: %d\n",name.name(),descdata.get_value<uint64_t>(index));
                uint64_t val = descdata.get_value<uint64_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(uint64_t);
            printf("pixel_size_out = %d, sizeof(uint64_t) = %d\n", pixel_size_out, sizeof(uint64_t));
            break;
        }

        case(Name::INT8):{
            assert(name.type() == xtc_data_type::INT8);
            if(data_rank > 0){
                Array<int8_t> arrT = descdata.get_array<int8_t>(index);
                printf("type int8:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type int8: %s: %d\n",name.name(),descdata.get_value<int8_t>(index));
                int8_t val = descdata.get_value<int8_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int8_t);
            printf("pixel_size_out = %d, sizeof(int8_t) = %d\n", pixel_size_out, sizeof(int8_t));
            break;
        }

        case(Name::INT16):{
            assert(name.type() == xtc_data_type::INT16);
            if(data_rank > 0){
                Array<int16_t> arrT = descdata.get_array<int16_t>(index);
                printf("type int16: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type int16: %s: %d\n",name.name(),descdata.get_value<int16_t>(index));
                int16_t val = descdata.get_value<int16_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int16_t);
            printf("pixel_size_out = %d, sizeof(int16_t) = %d\n", pixel_size_out, sizeof(int16_t));
            break;
        }

        case(Name::INT32):{
            assert(name.type() == xtc_data_type::INT32);
            if(data_rank > 0){
                Array<int32_t> arrT = descdata.get_array<int32_t>(index);
                printf("type int32: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type int32:   %s: %d\n",name.name(),descdata.get_value<int32_t>(index));
                int32_t val = descdata.get_value<int32_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int32_t);
            printf("pixel_size_out = %d, sizeof(int32_t) = %d\n", pixel_size_out, sizeof(int32_t));
            break;
        }

        case(Name::INT64):{
            assert(name.type() == xtc_data_type::INT64);
            if(data_rank > 0){
                Array<int64_t> arrT = descdata.get_array<int64_t>(index);
                printf("type int64:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type int64:   %s: %d\n",name.name(),descdata.get_value<int64_t>(index));
                int64_t val = descdata.get_value<int64_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int64_t);
            printf("pixel_size_out = %d, sizeof(int64_t) = %d\n", pixel_size_out, sizeof(int64_t));
            break;
        }

        case(Name::FLOAT):{
            assert(name.type() == xtc_data_type::FLOAT);
            //DEBUG_PRINT
            if(data_rank > 0){
                DEBUG_PRINT
                Array<float> arrT = descdata.get_array<float>(index);
                printf("type float:  %s: %f, %f\n",name.name(),arrT.data()[0],arrT.data()[1]);
                DEBUG_PRINT
                ret_data =arrT.data();
                DEBUG_PRINT
                    }
            else{
                DEBUG_PRINT
                printf("type float:  %s: %f\n",name.name(),descdata.get_value<float>(index));
                DEBUG_PRINT
                float val = descdata.get_value<float>(index);
                DEBUG_PRINT
                ret_data =&val;
                DEBUG_PRINT
            }
            DEBUG_PRINT
            pixel_size_out = sizeof(float);
            printf("pixel_size_out = %d, sizeof(float) = %d, \n", pixel_size_out,sizeof(float));
            DEBUG_PRINT
            break;
        }

        case(Name::DOUBLE):{
            assert(name.type() == xtc_data_type::DOUBLE);
            DEBUG_PRINT
            if(data_rank > 0){
                DEBUG_PRINT
                Array<double> arrT = descdata.get_array<double>(index);
                printf("type double: index = %d, filed_name:%s: %f, %f, %f\n", index, name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                DEBUG_PRINT
                ret_data =arrT.data();
                    }
            else{
                DEBUG_PRINT
                printf("type double: index = %d, filed_name:%s: %f\n", index, name.name(),descdata.get_value<double>(index));
                double val = descdata.get_value<double>(index);
                ret_data =&val;
            }
            DEBUG_PRINT
            pixel_size_out = sizeof(double);
            printf("pixel_size_out = %d, sizeof(double) = %d\n", pixel_size_out, sizeof(double));
            DEBUG_PRINT
            break;
        }

        case(Name::CHARSTR):{
            assert(name.type() == xtc_data_type::CHARSTR);
            assert(0);
            if(data_rank > 0){
                DEBUG_PRINT
                Array<char> arrT = descdata.get_array<char>(index);
                DEBUG_PRINT
                printf("type charstr: index = %d, rank = %d, num_elem = %lu,   name = [%s], str = [%s], data() = %p \n",
                        index, arrT.rank(), arrT.num_elem(), name.name(), arrT.data(), arrT.data());
                DEBUG_PRINT
                // =arrT.data();
                tmp_buf = (char*)calloc(arrT.num_elem() + 1, sizeof(char));

                for(int i = 0; i < arrT.num_elem(); i++){
                    tmp_buf[i] = arrT.data()[i];
                }
                printf("tmp_but = %p, data = %s\n", tmp_buf, (char*)tmp_buf);
                assert(0);
                ret_data = &tmp_buf;

                DEBUG_PRINT
                    }
            else{
                printf("type charstr: %s: string with no rank?!?\n",name.name());
                char val = descdata.get_value<char>(index);
                ret_data =&val;
            }
            DEBUG_PRINT
            assert(0);
            pixel_size_out = sizeof(char*);
            printf("tmp_buf = %s, \n", tmp_buf);
            break;
        }

        case(Name::ENUMVAL):{
            assert(name.type() == xtc_data_type::ENUMVAL);
            if(data_rank > 0){
                Array<int32_t> arrT = descdata.get_array<int32_t>(index);
                printf("type ENUMVAL: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                ret_data =arrT.data();
                    }
            else{
                printf("type ENUMVAL %s: %d\n",name.name(),descdata.get_value<int32_t>(index));
                int32_t val = descdata.get_value<int32_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int32_t);
            printf("pixel_size_out = %d, sizeof(int32_t) = %d\n", pixel_size_out, sizeof(int32_t));
            break;
        }

        case(Name::ENUMDICT):{
            assert(name.type() == xtc_data_type::ENUMDICT);
            if(data_rank > 0){
                printf("type ENUMDICT %s: enumdict with rank?!?\n", name.name());
                ret_data =NULL;
            } else{
                printf("type ENUMDICT  %s: %d\n",name.name(),descdata.get_value<int32_t>(index));
                int32_t val = descdata.get_value<int32_t>(index);
                ret_data =&val;
            }
            pixel_size_out = sizeof(int32_t);
            printf("pixel_size_out = %d, sizeof(int32_t) = %d\n", pixel_size_out, sizeof(int32_t));
            break;
        }
        default:
            assert(0 && "Unsupported type.");
            break;
        }
        DEBUG_PRINT
        printf("Final pixel_size_out = %d\n", pixel_size_out);
        size_t read_size = pixel_cnt * pixel_size_out;
        memcpy(data_out, ret_data, read_size);

//        if(tmp_buf)
//            free(tmp_buf);

        return read_size;
    }

    void get_value(int i, Name& name, DescData& descdata){
        int data_rank = name.rank();
        int data_type = name.type();
        printf("get_value() terminal data entry ==============  %d: name:[%s], rank = %d, type = %d\n",
                i, name.name(), data_rank, data_type);
        //printf("get_value() token = %s\n", name.name());
        switch(name.type()){
        case(Name::UINT8):{
            if(data_rank > 0){
                Array<uint8_t> arrT = descdata.get_array<uint8_t>(i);
                printf("type uint8_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type uint8_t:  %s: %d\n",name.name(),descdata.get_value<uint8_t>(i));
            }
            break;
        }

        case(Name::UINT16):{
            if(data_rank > 0){
                Array<uint16_t> arrT = descdata.get_array<uint16_t>(i);
                printf("type uint16_t:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type uint16_t: %s: %d\n",name.name(),descdata.get_value<uint16_t>(i));
            }
            break;
        }

        case(Name::UINT32):{
            if(data_rank > 0){
                Array<uint32_t> arrT = descdata.get_array<uint32_t>(i);
                printf("type uint32_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type uint32_t: %s: %d\n",name.name(),descdata.get_value<uint32_t>(i));
            }
            break;
        }

        case(Name::UINT64):{
            if(data_rank > 0){
                Array<uint64_t> arrT = descdata.get_array<uint64_t>(i);
                printf("type uint64_t: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type uint64_t %s: %d\n",name.name(),descdata.get_value<uint64_t>(i));
            }
            break;
        }

        case(Name::INT8):{
            if(data_rank > 0){
                Array<int8_t> arrT = descdata.get_array<int8_t>(i);
                printf("type int8:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type int8: %s: %d\n",name.name(),descdata.get_value<int8_t>(i));
            }
            break;
        }

        case(Name::INT16):{
            if(data_rank > 0){
                Array<int16_t> arrT = descdata.get_array<int16_t>(i);
                printf("type int16: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type int16: %s: %d\n",name.name(),descdata.get_value<int16_t>(i));
            }
            break;
        }

        case(Name::INT32):{
            if(data_rank > 0){
                Array<int32_t> arrT = descdata.get_array<int32_t>(i);
                printf("type int32: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type int32:   %s: %d\n",name.name(),descdata.get_value<int32_t>(i));
            }
            break;
        }

        case(Name::INT64):{
            if(data_rank > 0){
                Array<int64_t> arrT = descdata.get_array<int64_t>(i);
                printf("type int64:  %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type int64:   %s: %d\n",name.name(),descdata.get_value<int64_t>(i));
            }
            break;
        }

        case(Name::FLOAT):{
            if(data_rank > 0){
                Array<float> arrT = descdata.get_array<float>(i);
                printf("type float:  %s: %f, %f\n",name.name(),arrT.data()[0],arrT.data()[1]);
                    }
            else{
                printf("type float:  %s: %f\n",name.name(),descdata.get_value<float>(i));
            }
            break;
        }

        case(Name::DOUBLE):{
            if(data_rank > 0){
                Array<double> arrT = descdata.get_array<double>(i);
                printf("type double: %s: %f, %f, %f\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type double: %s: %f\n",name.name(),descdata.get_value<double>(i));
            }
            break;
        }

        case(Name::CHARSTR):{
            if(data_rank > 0){
                Array<char> arrT = descdata.get_array<char>(i);
                printf("type charstr:  rank = %d, num_elem = %lu, name = [%s], str = [%s], data() = %p \n",
                        arrT.rank(), arrT.num_elem(), name.name(), arrT.data(), arrT.data());
                    }
            else{
                printf("type charstr: %s: string with 0 rank?!?\n",name.name());
            }
            break;
        }

        case(Name::ENUMVAL):{
            if(data_rank > 0){
                Array<int32_t> arrT = descdata.get_array<int32_t>(i);
                printf("type ENUMVAL: %s: %d, %d, %d\n",name.name(),arrT.data()[0],arrT.data()[1], arrT.data()[2]);
                    }
            else{
                printf("type ENUMVAL %s: %d\n",name.name(),descdata.get_value<int32_t>(i));
            }
            break;
        }

        case(Name::ENUMDICT):{
            if(data_rank > 0){
                printf("type ENUMDICT %s: enumdict with rank?!?\n", name.name());
            } else{
                printf("type ENUMDICT  %s: %d\n",name.name(),descdata.get_value<int32_t>(i));
            }
            break;
        }
        }
    }

    int process_old(Xtc* xtc)
    {
        switch (xtc->contains.id()) {
        case (TypeId::Parent): {
            printf("Found TypeID == Parent, iterating...\n");
            iterate(xtc);
            break;
        }
        case (TypeId::Names): {
            Names& names = *(Names*)xtc;
            _namesLookup[names.namesId()] = NameIndex(names);
            Alg& alg = names.alg();
        printf("===============================\n");
	    printf("***  Per names metadata: DetName: %s, Segment %d, DetType: %s, Alg: %s, "
	            "Version: 0x%6.6x, namesid: 0x%x, Names:\n",
                   names.detName(), names.segment(), names.detType(),
                   alg.name(), alg.version(), (int)names.namesId());
	    printf("process(): TypeId::Names: current token = %s\n", names.detName());
	    //_CURRENT_PATH_TOKENS.push_back(names.detName());
	    append_token(names.detName());
        for (unsigned i = 0; i < names.num(); i++) {
            Name& name = names.get(i);
            printf("      process() level 2 token = %s\n", name.name());
            //_CURRENT_PATH_TOKENS.push_back(name.name());
            append_token(name.name());
            print_local_path();
            printf("      Name: %s, Type: %d, Rank: %d.\n",name.name(),name.type(), name.rank());
            pop_token();
        }
        pop_token();

            break;
        }
        case (TypeId::ShapesData): {
            ShapesData& shapesdata = *(ShapesData*)xtc;
            // lookup the index of the names we are supposed to use
            NamesId namesId = shapesdata.namesId();
            // protect against the fact that this namesid
            // may not have a NamesLookup.  cpo thinks this
            // should be fatal, since it is a sign the xtc is "corrupted",
            // in some sense.
            if (_namesLookup.count(namesId)<=0) {
                printf("*** Corrupt xtc: namesid 0x%x not found in NamesLookup\n",(int)namesId);
                throw "invalid namesid";
                break;
            }
            DescData descdata(shapesdata, _namesLookup[namesId]);
            Names& names = descdata.nameindex().names();
            Data& data = shapesdata.data();
            printf("===============================\n");
	    printf("ShapesData group:Found %d names, the token is detName = [%s]\n",names.num(), names.detName());
	    append_token(names.detName());
            for (unsigned i = 0; i < names.num(); i++) {
                Name& name = names.get(i);
                append_token(name.name());
                print_local_path();
                get_value(i, name, descdata);
                pop_token();
            }
        pop_token();
            break;
        }
        default:
            break;
        }
        return Continue;
    }

    int process(Xtc* xtc){
        int ret = -1;;
        switch(get_iterator_type()){
            case LIST_ALL:
                ret = process_list_all(xtc);
                break;
            case SEARCH:
                ret = process_search(xtc);
                break;
            case LIST_W_DEPTH:
                assert(0 && "LIST_W_DEPTH Not implemented.");
                break;

            default:
                ret = -1;
                break;
        }
        return ret;
    }

    int process_list_all(Xtc* xtc)
    {
        printf("%s:%d:  dbgiter = %p, this->_current_dgram = %p\n",
                __func__, __LINE__, this, this->_current_dgram);
        assert(this->_current_dgram);
        uint64_t dgram_id = this->_current_dgram->time.value();
        unordered_map<uint64_t, xtc_dgram_info>::iterator it = _index_map->find(dgram_id);
        if(it == _index_map->end()){
            printf("Can not find dgram_id.\n");
            return -1;
        }

        xtc_dgram_info dgram_info = it->second;

        switch (xtc->contains.id()) {
        case (TypeId::Parent): {
            printf("Found TypeID == Parent, iterating...\n");
            iterate(xtc);
            break;
        }
        case (TypeId::Names): {
            printf("=============================== find Names!!!\n");
            Names& names = *(Names*)xtc;
            _namesLookup[names.namesId()] = NameIndex(names);
            Alg& alg = names.alg();

            printf("set _namesLookup = %p\n", &_namesLookup);
//      printf("***  Per names metadata: DetName: %s, Segment %d, DetType: %s, Alg: %s, "
//              "Version: 0x%6.6x, namesid: 0x%x, Names:\n",
//                   names.detName(), names.segment(), names.detType(),
//                   alg.name(), alg.version(), (int)names.namesId());
      //printf("process(): TypeId::Names: current token = %s\n", names.detName());

            string token = names.detName();
            token += "_";
            token += names.detType();
            token += "_";
            token += alg.name();
            append_token(token);
            append_token(to_string(names.segment()));
            xtc_object* new_obj = xtc_obj_new(CURRENT_LOCATION, get_current_path().c_str());
            xtc_tree_node_add(new_xtc_node(new_obj));

            for (unsigned i = 0; i < names.num(); i++) {
                Name& name = names.get(i);
                append_token(name.name());
                //print_local_path();
                string path_str = get_current_path();
                xtc_object* new_obj = xtc_obj_new(CURRENT_LOCATION, path_str.c_str());
                xtc_tree_node_add(new_xtc_node(new_obj));
                pop_token();
            }
            pop_token();//segment
            pop_token();//detName_detType_alg
            break;
        }
        case (TypeId::ShapesData): {
            printf("=============================== find ShapesData!!!\n");
            ShapesData& shapesdata_ref = *(ShapesData*)xtc; //Unsafe downcasting!

            ShapesData* shapesdata = (ShapesData*)calloc(1, sizeof(ShapesData*));
            shapesdata = static_cast<ShapesData*>(xtc);//if sure it is such a subclass type
            printf("&shapesdata_ref = %p, static_cast: shapesdata = %p, xtc = %p, &_namesLookup = %p\n", &shapesdata_ref, shapesdata, xtc, &_namesLookup);
            // lookup the index of the names we are supposed to use
//            ShapesData* shapesdata_ptr = (ShapesData*)(calloc(1, sizeof(ShapesData)));
//            memcpy(shapesdata_ptr, &shapesdata, sizeof(ShapesData*));
//            shapesdata_ptr = &shapesdata;
            //shapesdata_ptr = &shapesdata;
            //std::addressof(shapesdata);

            NamesId namesId = shapesdata->namesId();
            // protect against the fact that this namesid
            // may not have a NamesLookup.  cpo thinks this
            // should be fatal, since it is a sign the xtc is "corrupted",
            // in some sense.
            if (_namesLookup.count(namesId)<=0) {
                printf("*** Corrupt xtc: namesid 0x%x not found in NamesLookup\n",(int)namesId);
                throw "invalid namesid";
                break;
            }
            DescData descdata(*shapesdata, _namesLookup[namesId]);

            Names& names = descdata.nameindex().names();
            Data& data = shapesdata->data();

            //printf("ShapesData group:Found %d names, the token is detName = [%s]\n",names.num(), names.detName());
            string token = names.detName();
            token += "_";
            token += names.detType();
            token += "_";
            token += names.alg().name();
            append_token(token);
            append_token(to_string(names.segment()));

            string time_stamp_str = to_string(((Dgram*)CURRENT_LOCATION->dg)->time.asDouble());

//            append_token(string("tmp_str"));
            //check DS holder group existence.
            string cur_path = get_current_path();
            printf("search cur_path = %s\n", cur_path.c_str());
            xtc_node* group_ds_holder = xtc_tree_node_find(&cur_path); //the direct holder group of datasets
            if(!group_ds_holder){
                printf("Create group_ds_holder: %s\n", cur_path.c_str());
                xtc_object* new_obj = xtc_obj_new(CURRENT_LOCATION, get_current_path().c_str());
                xtc_tree_node_add(new_xtc_node(new_obj));
            } //do nothing if group exists.

            //  Store time stamps in a virtual dataset
            cur_path += "/";
            cur_path += TIMESTAMP_DS_NAME;
            xtc_node* timestamp_ds_node = xtc_tree_node_find(&cur_path);
            double time_stamp = ((Dgram*)CURRENT_LOCATION->dg)->time.asDouble();

            //append_token(TIMESTAMP_DS_NAME);
            if(!timestamp_ds_node){// create timestamp dataset
                printf("Create timestamp_ds: %s\n", cur_path.c_str());
                append_token(TIMESTAMP_DS_NAME);
                xtc_object* new_obj = xtc_obj_new(CURRENT_LOCATION, get_current_path().c_str());
                new_obj->obj_type = XTC_TIME_DS;//XTC_TIME_DS
                vector<double>* tsv = new vector<double>();
                tsv->push_back(time_stamp);
                new_obj->data = (void*) tsv;
                xtc_ds_info* ds_info = (xtc_ds_info*)calloc(1, sizeof(xtc_ds_info));
                ds_info->current_dims[0] = 1;
                ds_info->type = DOUBLE;
                ds_info->element_cnt = 1;
                ds_info->dim_cnt = 1; //1D array
                ds_info->maximum_dims = NULL;
                ds_info->isTimestampsDS = 1;

                new_obj->ds_info = ds_info;
                xtc_tree_node_add(new_xtc_node(new_obj));
                pop_token();//ds name
            } else {//add timestamp to dataset
                assert(timestamp_ds_node->my_obj);
                assert(timestamp_ds_node->my_obj->data);
                ((vector<double>*)(timestamp_ds_node->my_obj->data))->push_back(time_stamp);
                timestamp_ds_node->my_obj->ds_info->element_cnt =
                        ((vector<double>*)(timestamp_ds_node->my_obj->data))->size();
            }
            //pop_token();//TIMESTAMP_DS_NAME


            for (unsigned i = 0; i < names.num(); i++) {
                Name& name = names.get(i);
                if (strcmp(name.name(),"intOffset")==0) {
                    uint64_t offset = descdata.get_value<uint64_t>(i);
                    //printf("Type=ShapesData  offset = 0x%x\n", offset);
                }
                append_token(name.name());//ds name
                print_local_path();
                string path_str = get_current_path();
                xtc_object* new_obj = xtc_obj_new(CURRENT_LOCATION, get_current_path().c_str());
                new_obj->obj_type = XTC_DS;

                new_obj->ds_info = get_ds_info(i, shapesdata, dgram_info);
                xtc_tree_node_add(new_xtc_node(new_obj));
                get_value(i, name, descdata);
                pop_token();//ds name
            }

//        pop_token();//tmp_str
        pop_token();//segment
        pop_token();//detName_detType_alg
            break;
        }
        default:
            break;
        }
        return Continue;
    }

    int process_search(Xtc* xtc)
    {
        switch (xtc->contains.id()) {
        case (TypeId::Parent): {

            iterate(xtc);
            break;
        }
        case (TypeId::Names): {
            Names& names = *(Names*)xtc;
            DEBUG_PRINT
            _namesLookup[names.namesId()] = NameIndex(names);
            Alg& alg = names.alg();


        string candidate_str = names.detName();
        candidate_str += "_";
        candidate_str += names.detType();
        candidate_str += "_";
        candidate_str += names.alg().name();
        append_token(candidate_str);
        append_token(to_string(names.segment()));

        int match = compare_input_path();//1: continue to scan, -1 mismatch, 0 all match.

        if(match == 1){
            printf("\n=============================== Names:: Found 2nd token match: str = %s, index = %d\n",
                    candidate_str.c_str(), _scan_index);
            print_input_path();
            print_local_path();
            for (unsigned i = 0; i < names.num(); i++) {
                Name& name = names.get(i);
                printf("      process() level 2 token = %s\n", name.name());

                candidate_str = name.name();
                append_token(candidate_str);
                print_local_path();

                match = compare_input_path();
                if(match == 0){
                    printf("\n=============================== Found terminal token match: str = %s, index = %d\n",
                            candidate_str.c_str(), _scan_index);
                    print_input_path();
                    print_local_path();
                    printf("===============================\n\n");
                    //return 0;
                }
                pop_token();

                if(candidate_str.compare(_INPUT_PATH_TOKENS[_scan_index]) == 0){

                    printf("\n=============================== Terminal token match: str = %s, index = %d\n",
                            candidate_str.c_str(), _scan_index);
                    print_input_path();
                    print_local_path();
                    //return 0;
                    printf("      Name: %s, Type: %d, Rank: %d.\n",name.name(),name.type(), name.rank());
                    printf("===============================\n\n");
                }
            }
        } else if(match == 0 ) {//all match.
            printf("Find target group: ");
            print_local_path();
            //need to return something?
        }
        else if(match == 2) {
            printf("Scanned too much: ");
            print_local_path();
        }
        pop_token();
        pop_token();
            break;
        }
        case (TypeId::ShapesData): {
            DEBUG_PRINT
            ShapesData& shapesdata = *(ShapesData*)xtc;
            // lookup the index of the names we are supposed to use
            NamesId namesId = shapesdata.namesId();
            // protect against the fact that this namesid
            // may not have a NamesLookup.  cpo thinks this
            // should be fatal, since it is a sign the xtc is "corrupted",
            // in some sense.
            DEBUG_PRINT
            if (_namesLookup.count(namesId)<=0) {
                printf("*** Corrupt xtc: namesid 0x%x not found in NamesLookup\n",(int)namesId);
                throw "invalid namesid";
                break;
            }
            DEBUG_PRINT
            DescData descdata(shapesdata, _namesLookup[namesId]);
            Names& names = descdata.nameindex().names();
            Data& data = shapesdata.data();

            string candidate_str = names.detName();
            candidate_str += "_";
            candidate_str += names.detType();
            candidate_str += "_";
            candidate_str += names.alg().name();
            append_token(candidate_str);
            append_token(to_string(names.segment()));

            int match = compare_input_path();//1: continue to scan, -1 mismatch, 0 all match.

            if(match == 1 ){
                printf("\n=============================== ShapesData:: Found 2nd token match: str = %s, index = %d\n",
                        candidate_str.c_str(), _scan_index);
                print_input_path();
                print_local_path();
                for (unsigned i = 0; i < names.num(); i++) {
                    Name& name = names.get(i);
                    candidate_str = name.name();
                    append_token(candidate_str);
                    print_local_path();
                    match = compare_input_path();
                    if(match == 0){
                        printf("\n=============================== Found terminal token match: str = %s, index = %d\n",
                                candidate_str.c_str(), _scan_index);
                        print_input_path();
                        print_local_path();
                        get_value(i, name, descdata); //this should be the terminal.
                        printf("===============================\n\n");
                    }
                    pop_token();
                }
                printf("===============================\n\n");
            }
            else if(match == 0 ) {//all match.
                printf("Find target group: ");
                print_local_path();
            }
            else if(match == 2) {
                printf("Scanned too much: ");
                print_local_path();
            }
        pop_token();//pop segment #
        pop_token();// pop detName, alg, etc,.
            break;
        }// end case ShapesData
        default:
            break;
    }//end switch
        return Continue;
    }
    void index_init(){
        _scan_index = -1;
    }

    int get_index(){
        return _scan_index;
    }

    void append_token(string str){
        _CURRENT_PATH_TOKENS.push_back(str);
        _index_increment();
    }
    void set_input_path(vector<string> str_vec){
        _INPUT_PATH_TOKENS = str_vec;
    }

    int compare_input_path(){
        for(int i = 0; i < min(_INPUT_PATH_TOKENS.size(), _CURRENT_PATH_TOKENS.size()); i++){
            if(_INPUT_PATH_TOKENS[i].compare(_CURRENT_PATH_TOKENS[i]) != 0)
                return -1;
        }
        if(_CURRENT_PATH_TOKENS.size() < _INPUT_PATH_TOKENS.size())
            return 1;//continue to scan
        if(_CURRENT_PATH_TOKENS.size() == _INPUT_PATH_TOKENS.size())
            return 0;//find a perfect match
        if(_CURRENT_PATH_TOKENS.size() > _INPUT_PATH_TOKENS.size())
            return 2;//scan too much, should return one step earlier.
        return 0;
    }
    void pop_token(){
        _CURRENT_PATH_TOKENS.pop_back();
        _index_decrement();
    }
    void print_input_path(){
        printf("input path = ");
        print_path(_INPUT_PATH_TOKENS);
    }
    void print_local_path(void){
        printf("current local");
        print_path(_CURRENT_PATH_TOKENS);
        printf("\n");
    }

    string get_current_path(){
        string ret("/");
        for(int i = 0; i < _CURRENT_PATH_TOKENS.size(); i++ ){
            ret += _CURRENT_PATH_TOKENS[i];
            if(i != _CURRENT_PATH_TOKENS.size() - 1)
                ret += "/";
        }
        return ret;
    }

    int SCAN_DEPTH;

    typedef enum IteratorType{
        LIST_ALL,
        SEARCH,
        LIST_W_DEPTH,
        DEFAULT
    }ItType;

    void set_iterator_type(ItType type){
        iterator_type = type;
    }

    ItType get_iterator_type(){
        return iterator_type;
    }


    vector<string> _INPUT_PATH_TOKENS;

    void xtc_tree_init(xtc_object* root_obj){
        ROOT_NODE = new_xtc_node(root_obj);
        ROOT_NODE->parent = NULL;
        (*root_obj).tree_node = (void*)ROOT_NODE;
    }
    void xtc_tree_init(int fd){
        xtc_object* root_obj = (xtc_object*)calloc(1, sizeof(xtc_object));
        root_obj->fd = fd;
        root_obj->obj_path_abs = strdup("/");
        xtc_h5token_new((xtc_token_t**)&(root_obj->obj_token), 16);
        ROOT_NODE = new_xtc_node(root_obj);
        ROOT_NODE->parent = NULL;
    }

    int xtc_tree_node_add(xtc_node* node){
        return add_xtc_node(ROOT_NODE, node);
    }

    xtc_node* xtc_tree_node_find(const char* target_path){
        return find_xtc_node(ROOT_NODE, target_path);
    }
    xtc_node* xtc_tree_node_find(string* target_path_in){
        return xtc_tree_node_find(target_path_in->c_str());
    }
    xtc_node* get_root(){
        return ROOT_NODE;
    }
    int xtc_tree_print(){
        return print_tree(ROOT_NODE);
    }
    int attach_helper(xtc_object* h){
        assert(h);
        h->ref_cnt++;
        return 0;
    }

    int detach_helper(){
        extern_helper->ref_cnt--;
        return 0;
    }

    xtc_object* get_helper(){
        return extern_helper;
    }

    xtc_location* CURRENT_LOCATION;
    vector<string> _CURRENT_PATH_TOKENS;
    NamesLookup* namesLookup(){
        return &_namesLookup; //map
    }

    void set_current_dgram(Dgram* dgram){
        assert(dgram);
        this->_current_dgram = dgram;
        printf("set_current_dgram: resulting id = %llu\n", this->_current_dgram->time.value());
    }

    void set_index_map(unordered_map<uint64_t, xtc_dgram_info>* map){
        assert(map);
        _index_map = map;
    }
    unordered_map<uint64_t, xtc_dgram_info>* _index_map;
    Dgram* _current_dgram;

    int get_fd(){
        return _fd;
    }
private:
    int _fd;
    ItType iterator_type;
    NamesLookup _namesLookup;


    xtc_object* extern_helper;
    xtc_node* ROOT_NODE;
    int _scan_index; //mark which token to compare

    int _index_increment(){
        _scan_index++;
        return _scan_index;
    }
    int _index_decrement(){
        _scan_index--;
        return _scan_index;
    }
};

size_t dataset_read_all(xtc_object* obj, void* buf_out){
    size_t read_size = 0;
    assert(obj->location && obj->ds_info &&
            (obj->obj_type == XTC_DS || obj->obj_type == XTC_TIME_DS));

    assert(obj->location->dbgiter);

    DebugIter* dbgit = (DebugIter*)(obj->location->dbgiter);

    if(obj->ds_info->isTimestampsDS){//timestamps dataset
        vector<double>* time_stamps_ds = (vector<double>*) obj->data;
        read_size = time_stamps_ds->size()* sizeof(double);

        for(int i = 0; i < time_stamps_ds->size(); i++){
            double ts = time_stamps_ds->at(i);
            memcpy((char*)buf_out + (i * sizeof(double)), &ts, sizeof(double));
            printf("dataset_read_all: timestamp = %lf\n", ts);
        }
        //*buf_out = (void*)ts_data;
    } else {//regular xtc2 dataset
        //XtcData::Names* names = (XtcData::Names*)(obj->ds_info->data_handle->names);
        Xtc* xtc = (Xtc*)(obj->ds_info->data_handle->xtc_ptr);
        // &shapesdata
        ShapesData** shapesdata = (ShapesData**)(obj->ds_info->data_handle->shapesData);//it's written in &shapesdata
        NamesLookup* namesLookup = (NamesLookup*) obj->ds_info->data_handle->namesLookup;
        int index = obj->ds_info->data_handle->index;
        int dim_cnt = obj->ds_info->dim_cnt;
        assert(dim_cnt >= 0 && dim_cnt <= 5);
        printf("dataset_read_all: shapesdata(**) = %p, *shapesdata(*) = %p \n ", shapesdata, *shapesdata);
        DEBUG_PRINT
        //void* data = NULL;

        //size_t read_size = dbgit->get_data(index, shapesdata, buf_out, obj->ds_info->total_pixel_cnt);
        //size_t read_size = dbgit->get_data(index, *shapesdata, namesLookup, buf_out, obj->ds_info->total_pixel_cnt);
        size_t read_size = dbgit->get_data(obj->ds_info->data_handle, obj->ds_info->total_pixel_cnt, buf_out);
        DEBUG_PRINT
        //read_size = total_pixel_cnt * sizeof_pixel;
        DEBUG_PRINT
//        printf("total_pixel_cnt = %d, ds_info.element_cnt(images_cnt) = %d\n",
//                obj->ds_info->total_pixel_cnt, obj->ds_info->element_cnt);
        printf("data = [%s], data = %p, read_size = %d\n", (char*)(buf_out), buf_out, read_size);
        //memcpy(buf_out, data, read_size);
        DEBUG_PRINT
    }
    DEBUG_PRINT
    return read_size;
}

void print_path(vector<string>vec)
{
    if(vec.size()==0)
        return;
    printf(" path = ");
    for(vector<string>::iterator it = vec.begin(); it != vec.end(); ++it){
        printf("/%s", (*it).c_str());
    }
}

vector<string> str_tok(const char* str, const char* delimiters_str){
    assert(str && delimiters_str);
    char* str_mod = strdup(str);
    vector<string> res;
    char * pch;
    pch = strtok(str_mod, delimiters_str);
    while (pch != NULL){
      res.push_back(string(pch));
      pch = strtok (NULL, delimiters_str);
    }
    free(str_mod);
    return res;
}

string tok_to_str(vector<string> token_list){
    string s("/");
    for(auto i : token_list){
        s += i;
        if(i != token_list.back())
            s += "/";
    }
    return s;
}
void test_str_tok(){
    char sample[] = "this is a sample string, contains/multiple.delimiters. End.";

    vector<string> v = str_tok(sample, " ,/.");
    int n = v.size();
    for(int i = 0; i < n; i++){
        cout<< v[i] << endl;
    }
}

void test_token_comp(){

}

Dgram* iterate_with_depth(int fd, int depth){

    if(depth < 1 ){
        printf("Depth must be positive.\n");
        return NULL;
    }

    Dgram* it = NULL;


    XtcFileIterator iter(fd, 0x4000000);
    Dgram* dg;
    unsigned nevent=0;
    DebugIter dbgiter;
    bool debugprint = true;
    dbgiter.index_init();
    dbgiter.set_iterator_type(DebugIter::LIST_W_DEPTH);
    dbgiter.SCAN_DEPTH = depth;
    int i =0;

    dg = iter.next();//first dg, for configure transition.
    string candidate_str = string(TransitionId::name(dg->service()));

    dbgiter.append_token(candidate_str);
    dbgiter.print_local_path();
    dbgiter.iterate(&(dg->xtc));
    dbgiter.pop_token();

    printf("\n=============================== Configure transition completed. ===============================\n");
    while ((dg = iter.next())) {//each data item in the file
        i++;
        nevent++;

        DEBUG_PRINT
        string candidate_str = string(TransitionId::name(dg->service()));
        dbgiter.append_token(candidate_str);
        if(dbgiter.get_index() <= dbgiter.SCAN_DEPTH)
            debugprint = false;
        else
            debugprint = false;
        DEBUG_PRINT
        if (debugprint) {
            dbgiter.iterate(&(dg->xtc));
        }
        //if false, dg->xtc need to move to the next block.

        DEBUG_PRINT
        dbgiter.pop_token();
    }
    return it;
}


//int _load_dgram(int fd, xtc_dgram_info* dg_info, Dgram* bigdg_in_out){
//    assert(dg_info);
//    int ret = -1;
//    if (lseek(fd, (off_t)(dg_info->dgram_offset), SEEK_SET) < 0) {//move cursor to the where the offset points to
//        printf("lseek error\n");
//        return -1;
//    }
//    if (::read(fd, bigdg_in_out, dg_info->dgram_size) == 0) {//read from the offset to bigdg
//        printf("Big dgram read error\n");
//        return -1;
//    }
//    return 0;
//}

//Check if the current dgram is the target, if not, load dgram.
int check_update_dgram(int fd, xtc_dgram_info* dg_info, Dgram* current_dgram_in_out){
    assert(dg_info && current_dgram_in_out);
    if((current_dgram_in_out)->time.value() != dg_info->dgram_id){
        printf("dgram_id(%llu) doesn't match current one(%llu), load dgram ...\n",
                dg_info->dgram_id, (current_dgram_in_out)->time.value());
        //return *_load_dgram(fd, ds_handle, current_dgram_in_out);
        if (lseek(fd, (off_t)(dg_info->dgram_offset), SEEK_SET) < 0) {//move cursor to the where the offset points to
            printf("lseek error\n");
            return -1;
        }
        if (::read(fd, current_dgram_in_out, dg_info->dgram_size) == 0) {//read from the offset to bigdg
            printf("Big dgram read error\n");
            return -1;
        }
        printf(" done. Current dgram id = %llu\n", (current_dgram_in_out)->time.value());
        return 0;
    }

    return 0;
}

unordered_map<uint64_t, xtc_dgram_info>* load_index(int index_fd){
    XtcFileIterator iter(index_fd, 0x4000000);
    unordered_map<uint64_t, xtc_dgram_info>* index_map = new unordered_map<uint64_t, xtc_dgram_info>;
    Dgram* smalldg;
    unsigned nevent=0;
    SmdIter smditer;
    while ((smalldg = iter.next())) {
        smditer.iterate(&(smalldg->xtc));
        xtc_dgram_info info;
        info.dgram_id = smalldg->time.value();
        info.dgram_offset = smditer.offset;
        info.dgram_size = smditer.dgramSize;
        index_map->insert(pair<uint64_t, xtc_dgram_info>(info.dgram_id, info));
    }
    return index_map;
}

xtc_object* iterate_list_all(int fd, int index_fd){

    static const unsigned bigdgBufferSize = 0x4000000;
    XtcFileIterator* iter = new XtcFileIterator(fd, bigdgBufferSize);
    //XtcFileIterator* iter_index = new XtcFileIterator(index_fd, 0x4000000);


    //Dgram* index_dg;
    unsigned nevent=0;
    DebugIter* dbgiter = new DebugIter();
    printf("%s:%d:  dbgiter = %p, dbgiter->_current_dgram = %p\n",
            __func__, __LINE__, dbgiter, dbgiter->_current_dgram);
    bool debugprint = true;
    unordered_map<uint64_t, xtc_dgram_info>* imap = load_index(index_fd);
    if(!imap)
        return NULL;
    dbgiter->set_index_map(imap);
    dbgiter->_current_dgram = (Dgram*)malloc(bigdgBufferSize);
    //location init
    dbgiter->CURRENT_LOCATION = (xtc_location*)calloc(1, sizeof(xtc_location));
    dbgiter->CURRENT_LOCATION->fd = fd;
    dbgiter->CURRENT_LOCATION->fileIter = (void*)(iter);
    dbgiter->CURRENT_LOCATION->dbgiter = (void*)(dbgiter);
    dbgiter->CURRENT_LOCATION->dg = NULL;

    dbgiter->index_init();//-1
    dbgiter->set_iterator_type(DebugIter::LIST_ALL);
    int i = 0;
    //Dgram* dg = dbgiter->_current_dgram;
    dbgiter->_current_dgram = iter->next();//first dg, for configure transition.
    //index_dg = iter_index->next();

    dbgiter->CURRENT_LOCATION->dg = (void*) dbgiter->_current_dgram;
    xtc_object* head_obj = xtc_obj_new(fd, iter, dbgiter, dbgiter->_current_dgram, "/");
    //dbgiter->set_current_dgram(dg);
    head_obj->obj_type = XTC_HEAD;
    dbgiter->xtc_tree_init(head_obj);

    string candidate_str = string(TransitionId::name(dbgiter->_current_dgram->service()));
    //dbgiter->set_current_dgram(dg);
    dbgiter->append_token(candidate_str);
    dbgiter->iterate(&(dbgiter->_current_dgram->xtc));
    dbgiter->pop_token();
    //dbgiter->set_current_dgram(dg);
    DEBUG_PRINT

    int L1accept_cnt = 0;
    bool isConfigureTransition = true;

    while ((dbgiter->_current_dgram = iter->next())) {//each data item in the file
        //dbgiter->set_current_dgram(dg);
        //Configure transition

//        if(isConfigureTransition){
//            string candidate_str = string(TransitionId::name(dg->service()));
//            dbgiter->append_token(candidate_str);
//            dbgiter->iterate(&(dg->xtc));
//            dbgiter->pop_token();
//            isConfigureTransition = false;
//            printf("\n=============================== Configure transition completed. ===============================\n");
//        }
        printf("%s:%d:  dbgiter = %p, dbgiter->_current_dgram = %p\n",
                __func__, __LINE__, dbgiter, dbgiter->_current_dgram);
        dbgiter->CURRENT_LOCATION->dg = dbgiter->_current_dgram; //no use?
        i++;
        nevent++;

        DEBUG_PRINT
        string candidate_str = string(TransitionId::name(dbgiter->_current_dgram->service()));
        dbgiter->append_token(candidate_str);
        xtc_object* xtc_obj = xtc_obj_new(fd, iter, dbgiter, dbgiter->_current_dgram, dbgiter->get_current_path().c_str());
        dbgiter->xtc_tree_node_add(new_xtc_node(xtc_obj));

        DEBUG_PRINT
        Xtc* xtc = &(dbgiter->_current_dgram->xtc);
        if (debugprint) {
            ShapesData& shapesdata = *(ShapesData*)xtc;
            printf("iterate_list_all(): xtc = %p, shapesdata = %p\n", xtc, &shapesdata);
            dbgiter->iterate(xtc);
        }

        dbgiter->pop_token();
    }
    printf("\n\n\n\n");

    dbgiter->xtc_tree_print();
    printf("\n\n\n\n");
    return head_obj;

}

xtc_object* xtc_obj_new(int fd, void* fileIter, void* dbgiter, void* dg, const char* obj_path_abs){
    xtc_object* ret = (xtc_object*)calloc(1, sizeof(xtc_object));
    ret->fd = fd;

    ret->location = (xtc_location*) calloc(1, sizeof(xtc_location));
    ret->location->fd = fd;
    ret->location->dg = dg;
    ret->location->dbgiter = dbgiter;
    ret->location->fileIter = fileIter;

    ret->obj_type = XTC_GROUP;
    ret->obj_token = calloc(1, sizeof(xtc_token_t));
    ret->obj_path_abs = strdup(obj_path_abs);
    xtc_h5token_new((xtc_token_t**)&(ret->obj_token), 16);
    return ret;
}

xtc_object* xtc_obj_new(xtc_location* location, const char* obj_path_abs){
    assert(location);
    xtc_object* obj = xtc_obj_new(location->fd,
            location->fileIter, location->dbgiter, location->dg, obj_path_abs);
    return obj;
}
xtc_object* _target_open(xtc_object* helper_in, const char* obj_vol_path){
    assert(helper_in && obj_vol_path);
    int fd = helper_in->fd;

    //XtcFileIterator iter(fd, 0x4000000);
    if(0 == strcmp(obj_vol_path, "/")){
        return helper_in;
    }

    XtcFileIterator* iter = (XtcFileIterator*)(helper_in->location->fileIter);
    Dgram* dg;
    unsigned nevent=0;

    DebugIter* dbgiter = (DebugIter*)(helper_in->location->dbgiter);//new DebugIter();
    bool debugprint = true;
    DEBUG_PRINT
    dbgiter->set_input_path(str_tok(obj_vol_path, "/ "));
    dbgiter->print_input_path();
    dbgiter->index_init();//set to -1;
    dbgiter->set_iterator_type(DebugIter::SEARCH);
    dbgiter->print_input_path();

    printf("\n=============================== Configure transition completed. ===============================\n");
    while ((dg = iter->next())) {//each data item in the file
        DEBUG_PRINT
        string candidate_str = string(TransitionId::name(dg->service()));
        to_string(dg->time.asDouble());
        dbgiter->append_token(candidate_str);
        bool token_match = (candidate_str.compare(dbgiter->_INPUT_PATH_TOKENS[0]) == 0);//dbgiter->_scan_index
        if(token_match){
            printf("========= Transition match (%s), _scan_index = %d =========\n", candidate_str.c_str(), dbgiter->get_index());
            DEBUG_PRINT
            debugprint = true;
        }
        else{//skip
            DEBUG_PRINT
            printf("========= Transition doesn't match (%s), skip =========\n", candidate_str.c_str());
            debugprint = false;
        }

        DEBUG_PRINT
        if (debugprint) {
            dbgiter->iterate(&(dg->xtc));
        }
        //if false, dg->xtc need to move to the next block.
        DEBUG_PRINT

        dbgiter->pop_token();
    }
    xtc_object* ret = (xtc_object*)calloc(1, sizeof(xtc_object));
    ret->fd = fd;
    ret->location->dg = (void*)dg;
    ret->location->dbgiter = (void*)dbgiter;
    return ret;
}


unsigned long xtc_h5token_new(xtc_token_t** token, unsigned int h5_token_size) {
    if(!*token)
        *token = (xtc_token_t*)calloc(1, sizeof(xtc_token_t));

    struct timeval tp;
    gettimeofday(&tp, NULL);
    int32_t tval = (int32_t)((1000000 * tp.tv_sec) + tp.tv_usec);
    int n = sizeof(int32_t)/sizeof(uint8_t);
    for(int i = 0; i < h5_token_size; i++){//16
        if(i < n)
            (*token)->__data[i] = *(uint8_t*)(&tval + i * sizeof(uint8_t));
        else
            (*token)->__data[i] = 0;
    }
    return tval;
}


//xtc_object* _file_open(int fd){
////    XtcFileIterator* fileIter = new XtcFileIterator(fd, 0x4000000);
////    DebugIter* dbgiter = new DebugIter();
////    bool debugprint = true;
////    DEBUG_PRINT
////    dbgiter->index_init();//-1
////    dbgiter->set_iterator_type(DebugIter::LIST_ALL);//LIST_W_DEPTH
////    dbgiter->SCAN_DEPTH = 1;
////    int i = 0;
////    DEBUG_PRINT
////    Dgram* dg = new Dgram();
////    DEBUG_PRINT
////    dg = fileIter->next();//first dg, for configure transition.
////    DEBUG_PRINT
////    string candidate_str = string(TransitionId::name(dg->service()));
////    DEBUG_PRINT
////    dbgiter->append_token(candidate_str);
////    //dbgiter->print_local_path();
////    DEBUG_PRINT
////    dbgiter->iterate(&(dg->xtc));
////    dbgiter->pop_token();
////    DEBUG_PRINT
////    xtc_object* head_obj = xtc_obj_new(fd, fileIter, dbgiter, dg, "/");
//????  start after initial scan for configure transitions
////
////    printf("\n=========================== Configure transition completed. ===========================\n");
////    DEBUG_PRINT
//    return head_obj;
//
//}

void cc_extern_test_root(void* root_obj){
    DEBUG_PRINT
    return extern_test_root((xtc_object*)root_obj);
}

EXTERNC void extern_test_root(xtc_object* root_obj){
    assert(root_obj);
    assert(root_obj->location);
    printf("%s:%d:  xtc_obj = %p, dbg = %p, dbg->get_root() = %p\n",
            __func__, __LINE__, root_obj, root_obj->location->dbgiter,
            ((DebugIter*)(root_obj->location->dbgiter))->get_root());
}

EXTERNC xtc_object* xtc_obj_find(xtc_object* root_obj, const char* path){
//    printf("%s:%d:  xtc_obj = %p, dbg = %p\n", __func__, __LINE__, root_obj, root_obj->location->dbgiter);
    assert(root_obj && path);
    //DEBUG_PRINT
    assert(root_obj->location);
    //DEBUG_PRINT
    if(strcmp(path, "/") == 0){
        return root_obj;
    }
    //DEBUG_PRINT
    assert(((DebugIter*)(root_obj->location->dbgiter))->get_root());
    //DEBUG_PRINT
    xtc_node* node = ((DebugIter*)(root_obj->location->dbgiter))->xtc_tree_node_find(path);
    //xtc_node* root_node = (xtc_node*)(root_obj->tree_node);
    //DEBUG_PRINT
    if(node)
        return node->my_obj;
    else
        return NULL;
}

EXTERNC xtc_object* xtc_file_open(const char* file_path){
    DEBUG_PRINT
    int fd = open(file_path, O_RDONLY);
    string fname = string(file_path);
    DEBUG_PRINT
    int index_fd = open((fname+".smd").c_str(), O_RDONLY);
    DEBUG_PRINT
    printf("fd = %d, index_fd = %d\n", fd, index_fd);
    xtc_object* head_obj = iterate_list_all(fd, index_fd);
    return head_obj;//contains a pointer to root node.
}

EXTERNC void xtc_file_close(xtc_object* head){
    assert(head);
    assert(head->location);
    if(head->location->dbgiter){
        DebugIter* dbgiter = (DebugIter*) head->location->dbgiter;
        delete dbgiter;
    }
    if(head->location->dg){
        Dgram* dg = (Dgram*) head->location->dg;
        delete dg;
    }

    close(head->fd);
    //delete helper->dbgiter;
    //delete (Dgram*)(helper->target_it);
//    free(helper);
}

EXTERNC xtc_object** xtc_get_children_list(xtc_object* group_in, int* num_out){
    return get_children_obj(group_in, num_out);//NULL if no children
}

//
//EXTERNC xtc_object* outdated_xtc_path_search(xtc_object* file, char* path){
//    assert(file && file->location->dbgiter);
//    cout <<"Searching path: "<< path <<endl;
//    xtc_object* path_obj = _target_open(file, path);
//    path_obj->ref_cnt = 0;
//    return path_obj;
//}
//open it and what's next?
EXTERNC xtc_func_t xtc_it_open(void* param){
    xtc_object* p = (xtc_object*)param;
    Dgram* dg = (Dgram*) p->location->dg;
    XtcFileIterator iter(p->fd, 0x4000000);
    DebugIter* dbgiter = (DebugIter*) p->location->dbgiter;

    Dgram* dg_random;
    //"L1Accept # 1B th, index = 5"

    while ((dg = iter.next())) {//each data item in the file
        DEBUG_PRINT
        string candidate_str = string(TransitionId::name(dg->service()));
        dbgiter->append_token(candidate_str);
        dbgiter->print_local_path();
        DEBUG_PRINT
        if (true) {
            dbgiter->iterate(&(dg->xtc));
        }
        dbgiter->pop_token();
    }
    return NULL;
}


void usage(char* progname)
{
    fprintf(stderr, "Usage: %s -f <filename> [-h]\n", progname);
}

