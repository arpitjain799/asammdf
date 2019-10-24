#include <Python.h>
#include "numpy/arrayobject.h"
#include "numpy/ndarrayobject.h"

#define PY_PRINTF(o) \
    PyObject_Print(o, stdout, 0); printf("\n");


struct rec_info {
    unsigned long id;
    unsigned long size;
    PyObject* mlist;
}; 

struct node {
    struct node * next;
    struct rec_info info;
};

static PyObject* sort_data_block(PyObject* self, PyObject* args)
{
    int i=0, id_size;
    unsigned long rec_size, length, rec_id;
    PyObject *signal_data, *partial_records, *record_size, *optional, *mlist;
    PyObject *bts, *key, *value;
    char *buf, *end;
    struct node * head = NULL, *last=NULL, *item;

    if (!PyArg_ParseTuple(args, "OOOi|O", &signal_data, &partial_records, &record_size, &id_size, &optional))
    {
        printf("sort_data_block was called with wring parameters\n");
    }
    else
    {
        i = 0;
        Py_ssize_t pos = 0;
       
        while (PyDict_Next(record_size, &pos, &key, &value)) 
        {
            item = malloc(sizeof(struct node));
            item->info.id = PyLong_AsUnsignedLong(key);
            item->info.size = PyLong_AsUnsignedLong(value);
            item->info.mlist = PyDict_GetItem(partial_records, key);
            item->next = NULL;
            if (last)
                last->next = item;
            if (!head)
                head = item;
            last = item; 
        }
 
        buf = PyBytes_AS_STRING(signal_data);
        end = buf + PyBytes_GET_SIZE(signal_data);
        
        while (buf != end)
        {
            for (i=0, rec_id=0; i<id_size; i++)
                rec_id += (*buf++) << (i <<3);
            
            for (item=head; item!=NULL; item=item->next)
            {
                if (item->info.id == rec_id)
                {
                    rec_size = item->info.size;
                    mlist = item->info.mlist;
                    break;
                }
            }
            
            if (rec_size)
            {
                bts = PyBytes_FromStringAndSize(buf, rec_size);
                PyList_Append(
                    mlist,
                    bts
                );
                Py_DECREF(bts);

                buf += rec_size;
            }
            else
            {
                rec_size = (buf[3] << 24) + (buf[2] << 16) +(buf[1] << 8) + buf[0];
                length = rec_size + 4;
                bts = PyBytes_FromStringAndSize(buf, length);
                PyList_Append(mlist, bts);
                Py_DECREF(bts);
                buf += length;
            }
           
        }
    }

    Py_INCREF(Py_None);

    return Py_None;
}


static PyObject* extract(PyObject* self, PyObject* args)
{
    int i=0, j, count, max=0, is_byte_array;
    int pos=0;
    int size;
    PyObject *signal_data;
    char *buf;
    PyArrayObject *vals;
    PyArray_Descr *descr;
    void *addr;
    unsigned char * addr2;

    if(!PyArg_ParseTuple(args, "Oi", &signal_data, &is_byte_array))
    {
        printf("ext len 0\n");
    }
    else
    {
        buf = PyBytes_AS_STRING(signal_data);

        count = 0;

        while (pos < PyBytes_GET_SIZE(signal_data))
        {
            size = (buf[pos+3] << 24) + (buf[pos+2] << 16) +(buf[pos+1] << 8) +buf[pos];
            if (max < size)
                max = size;
            pos += 4 + size;
            count++;
        }

        if (is_byte_array)
        {

            npy_intp dims[2];
            dims[0] = count;
            dims[1] = max;

            vals = (PyArrayObject *) PyArray_ZEROS(2, dims, NPY_UBYTE, 0);

            addr = PyArray_GETPTR2(vals, 0, 0);
            addr2 = (unsigned char *) addr;

            for (i=0; i<count; i++)
            {
                size = (buf[3] << 24) + (buf[2] << 16) +(buf[1] << 8) +buf[0];
                buf += 4; 
                memcpy(addr2, buf, size);
                buf += size;
                addr2 += max;
            }
        }
        else
        {
            npy_intp dims[1];
            dims[0] = count;

            descr = PyArray_DescrFromType(NPY_STRING);
            descr = PyArray_DescrNew(descr);
            descr->elsize = max;

            vals = (PyArrayObject *) PyArray_Zeros(1, dims, descr, 0);

            addr = PyArray_GETPTR1(vals, 0);

            addr2 = (unsigned char *) addr;

            for (i=0; i<count; i++)
            {
                size = (buf[3] << 24) + (buf[2] << 16) +(buf[1] << 8) +buf[0];
                buf += 4; 
                memcpy(addr2, buf, size);
                buf += size;
                addr2 += max;
            }
        }
    }

    return (PyObject *) vals;
}


static PyObject* extract_parent(PyObject* self, PyObject* args)
{
    int i=0, j, count, max=0, is_byte_array;
    PyObject *data, *dtype, *dimensions_vals, *bts;
    char *buf, *buf2;
    PyArrayObject *vals;
    PyArray_Descr *descr;
    void *addr;
    unsigned char * addr2;
    unsigned long record_size, offset, size, dimensions;

    if(!PyArg_ParseTuple(args, "OiiiO", &data, &record_size, &offset, &size, &dtype))
    {
        printf("ext len 0\n");
    }
    else
    {
        buf = PyBytes_AS_STRING(data);
        
        /*npy_intp dims = (npy_intp) dimensions; 
        npy_intp dims_vals[dimensions];
        
        for (i=0; i<dimensions; i++)
        {
            dims_vals[i] = (npy_intp) PyLong_AsLong(PyTuple_GetItem(dimensions_vals, i));
        }
        
        vals = (PyArrayObject *) PyArray_Empty(dims, dims_vals, dtype, 0);*/
        
        count = (int) (PyBytes_GET_SIZE(data) / record_size);
        
        bts = PyBytes_FromStringAndSize(NULL, count * size);
        buf2 = PyBytes_AS_STRING(bts);
        
        buf += offset;
        for (i=0; i<count; i++)
        {
            memcpy(buf2, buf, size);
            buf2 += size;
            buf += record_size;
        }

    }

    return bts;
}


static PyObject* lengths(PyObject* self, PyObject* args)
{
    int i=0;
    Py_ssize_t count;
    int pos=0;
    PyObject *lst, *values, *item;

    if(!PyArg_ParseTuple(args, "O", &lst))
    {
        values = Py_None;
        Py_INCREF(Py_None);
    }
    else
    {

        count = PyList_Size(lst);

        values = PyTuple_New(count);

        for (i=0; i<(int)count; i++)
        {
            item = PyList_GetItem(lst, i);
            PyTuple_SetItem(values, i, PyLong_FromSsize_t(PyBytes_GET_SIZE(item)));
        }

    }

    return values;
}


static PyObject* get_vlsd_offsets(PyObject* self, PyObject* args)
{
    int i=0;
    Py_ssize_t count;
    int pos=0;
    PyObject *lst, *item, *result;
    npy_intp dim[1];
    PyArrayObject *values;

    unsigned long long current_size = 0;

    void *h_result;

    if(!PyArg_ParseTuple(args, "O", &lst))
    {
        printf("get_vlsd_offsets called with wrong parameters\n");
        result = Py_None;
        Py_INCREF(Py_None);
    }
    else
    {

        count = PyList_Size(lst);
        dim[0] = (int) count;
        values = (PyArrayObject *) PyArray_SimpleNew(1, dim, NPY_ULONGLONG);

        for (i=0; i<(int) count; i++)
        {
            h_result = PyArray_GETPTR1(values, i);
            item = PyList_GetItem(lst, i);
            *((unsigned long long*)h_result) = current_size;
            current_size += (unsigned long long)PyBytes_GET_SIZE(item);
        }
    }

    result = PyTuple_Pack(2, values, PyLong_FromUnsignedLong(current_size));

    return result;
}



// Our Module's Function Definition struct
// We require this `NULL` to signal the end of our method
// definition
static PyMethodDef myMethods[] =
{
    { "extract", extract, METH_VARARGS, "extract VLSD samples from raw block" },
    { "extract_parent", extract_parent, METH_VARARGS, "extract VLSD samples from raw block" },
    { "lengths", lengths, METH_VARARGS, "lengths" },
    { "get_vlsd_offsets", get_vlsd_offsets, METH_VARARGS, "get_vlsd_offsets" },
    { "sort_data_block", sort_data_block, METH_VARARGS, "sort raw data group block" },
    { NULL, NULL, 0, NULL }
};

// Our Module Definition struct
static struct PyModuleDef cutils =
{
    PyModuleDef_HEAD_INIT,
    "cutils",
    "helper functions written in C for speed",
    -1,
    myMethods
};

// Initializes our module using our above struct
PyMODINIT_FUNC PyInit_cutils(void)
{
    import_array();
    return PyModule_Create(&cutils);
}
