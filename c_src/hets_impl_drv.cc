// The MIT License
//
// Copyright (C) 2011-2014 by Joseph Wayne Norton <norton@alum.mit.edu>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "hets_impl_drv.h"
#include "hets_impl_drv_lib.h"

#include <stdio.h>

#if 0
#define GOTOBADARG { fprintf(stderr, "GOTOBADARG %s:%d\n", __FILE__, __LINE__); goto badarg; }
#else
#define GOTOBADARG { goto badarg; }
#endif


#define LETS_BADARG              0x00
#define LETS_TRUE                0x01
#define LETS_FALSE               0x02
#define LETS_END_OF_TABLE        0x03
#define LETS_BINARY              0x04

#define LETS_OPEN6               0x00  // same as OPEN
#define LETS_DESTROY6            0x01  // same as DESTROY
#define LETS_REPAIR6             0x02  // same as REPAIR
#define LETS_DELETE2             0x03
#define LETS_DELETE3             0x04
#define LETS_DELETE_ALL_OBJECTS2 0x05
#define LETS_FIRST2              0x06
#define LETS_FIRST_ITER2         0x07
#define LETS_INFO_MEMORY1        0x08
#define LETS_INFO_SIZE1          0x09
#define LETS_INSERT3             0x0A
#define LETS_INSERT4             0x0B
#define LETS_INSERT_NEW3         0x0C
#define LETS_INSERT_NEW4         0x0D
#define LETS_LAST2               0x0E
#define LETS_LAST_ITER2          0x0F
#define LETS_LOOKUP3             0x10
#define LETS_MEMBER3             0x11
#define LETS_NEXT3               0x12
#define LETS_NEXT_ITER3          0x13
#define LETS_PREV3               0x14
#define LETS_PREV_ITER3          0x15

// DrvData
typedef struct {
    ErlDrvPort port;
    lets_impl impl;
} DrvData;

struct DrvAsync {
    DrvData* drvdata;
    ErlDrvTermData caller;
    int command;

    // outputs
    ErlDrvBinary* binary;
    int reply;

    // inputs
    leveldb::ReadOptions read_options;
    leveldb::WriteOptions write_options;
    leveldb::WriteBatch batch;

    DrvAsync(DrvData* d, ErlDrvTermData c, int cmd) :
        drvdata(d), caller(c), command(cmd), binary(NULL), reply(LETS_TRUE) {
    }
    DrvAsync(DrvData* d, ErlDrvTermData c, int cmd, leveldb::ReadOptions options) :
        drvdata(d), caller(c), command(cmd), binary(NULL), reply(LETS_TRUE), read_options(options) {
    }
    DrvAsync(DrvData* d, ErlDrvTermData c, int cmd, leveldb::WriteOptions options) :
        drvdata(d), caller(c), command(cmd), binary(NULL), reply(LETS_TRUE), write_options(options) {
    }
    DrvAsync(DrvData* d, ErlDrvTermData c, int cmd, leveldb::ReadOptions options, const char* key, int keylen) :
        drvdata(d), caller(c), command(cmd), binary(NULL), reply(LETS_TRUE), read_options(options) {
        binary = driver_alloc_binary(keylen);
        assert(binary);
        memcpy(binary->orig_bytes, key, binary->orig_size);
    }
    DrvAsync(DrvData* d, ErlDrvTermData c, int cmd, leveldb::WriteOptions options, const char* key, int keylen) :
        drvdata(d), caller(c), command(cmd), binary(NULL), reply(LETS_TRUE), write_options(options) {
        binary = driver_alloc_binary(keylen);
        assert(binary);
        memcpy(binary->orig_bytes, key, binary->orig_size);
    }

    ~DrvAsync() {
        if (binary) {
            driver_free_binary(binary);
        }
    }

    void put(const char* key, int keylen, const char* blob, int bloblen) {
        leveldb::Slice skey(key, keylen);
        leveldb::Slice sblob(blob, bloblen);
        batch.Put(skey, sblob);
    }

    void del(const char* key, int keylen) {
        leveldb::Slice skey(key, keylen);
        batch.Delete(skey);
    }
};

static void lets_output_create6(const char op, DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_create6(void* async_data);
static void lets_output_open6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_output_destroy6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_output_repair6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_output_insert3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_insert3(void* async_data);
static void lets_output_insert4(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_insert4(void* async_data);
// static void lets_output_insert_new3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
// static void lets_async_insert_new3(void* async_data);
// static void lets_output_insert_new4(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
// static void lets_async_insert_new4(void* async_data);
static void lets_output_delete2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_delete2(void* async_data);
static void lets_output_delete3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_delete3(void* async_data);
// static void lets_output_delete_all_objects2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
// static void lets_async_delete_all_objects2(void* async_data);
static void lets_output_lookup3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_lookup3(void* async_data);
static void lets_output_member3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_member3(void* async_data);
static void lets_output_first2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_first2(void* async_data);
static void lets_output_first_iter2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_first_iter2(void* async_data);
static void lets_output_last2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_last2(void* async_data);
static void lets_output_last_iter2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_last_iter2(void* async_data);
static void lets_output_next3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_next3(void* async_data);
static void lets_output_next_iter3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_next_iter3(void* async_data);
static void lets_output_prev3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_prev3(void* async_data);
static void lets_output_prev_iter3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
static void lets_async_prev_iter3(void* async_data);
// static void lets_output_info_memory1(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
// static void lets_async_info_memory1(void* async_data);
// static void lets_output_info_size1(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items);
// static void lets_async_info_size1(void* async_data);

static void
driver_send_int(DrvData* d, const int i, ErlDrvTermData caller=0)
{
    ErlDrvTermData port = driver_mk_port(d->port);
    ErlDrvTermData spec[] = {
        ERL_DRV_PORT, port,
        ERL_DRV_INT, (ErlDrvTermData) i,
        ERL_DRV_TUPLE, 2,
    };
    if (!caller) {
        caller = driver_caller(d->port);
    }
    erl_drv_send_term(port, caller, spec, sizeof(spec) / sizeof(spec[0]));
}

static void
driver_send_binary(DrvData* d, ErlDrvBinary* bin, ErlDrvTermData caller=0)
{
    ErlDrvTermData port = driver_mk_port(d->port);
    ErlDrvTermData spec[] = {
        ERL_DRV_PORT, port,
        ERL_DRV_INT, LETS_BINARY,
        ERL_DRV_BINARY, (ErlDrvTermData) bin, (ErlDrvTermData) bin->orig_size, 0,
        ERL_DRV_TUPLE, 3,
    };
    if (!caller) {
        caller = driver_caller(d->port);
    }
    erl_drv_send_term(port, caller, spec, sizeof(spec) / sizeof(spec[0]));
}

static void
driver_send_buf(DrvData* d, const char *buf, const ErlDrvUInt len, ErlDrvTermData caller=0)
{
    ErlDrvTermData port = driver_mk_port(d->port);
    ErlDrvTermData spec[] = {
        ERL_DRV_PORT, port,
        ERL_DRV_INT, LETS_BINARY,
        ERL_DRV_BUF2BINARY, (ErlDrvTermData) buf, len,
        ERL_DRV_TUPLE, 3,
    };
    if (!caller) {
        caller = driver_caller(d->port);
    }
    erl_drv_send_term(port, caller, spec, sizeof(spec) / sizeof(spec[0]));
}


//
// Callbacks
//
static ErlDrvEntry drv_driver_entry = {
    drv_init,
    drv_start,
    drv_stop,
    drv_output,
    NULL,
    NULL,
    (char*) "hets_impl_drv",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    drv_ready_async,
    NULL,
    NULL,
    NULL,
    (int) ERL_DRV_EXTENDED_MARKER,
    (int) ERL_DRV_EXTENDED_MAJOR_VERSION,
    (int) ERL_DRV_EXTENDED_MINOR_VERSION,
    (int) ERL_DRV_FLAG_USE_PORT_LOCKING,
    NULL,
    NULL,
    NULL
};

DRIVER_INIT (hets_impl_drv) // must match name in driver_entry
{
    return &drv_driver_entry;
}

int
drv_init()
{
    if (!lets_impl_drv_lib_init()) {
        return -1;
    }

    return 0;
}

ErlDrvData
drv_start(ErlDrvPort port, char* command)
{
    (void) command;

    DrvData* d;

    if (port == NULL) {
        return ERL_DRV_ERROR_GENERAL;
    }

    if ((d = (DrvData*) driver_alloc(sizeof(DrvData))) == NULL) {
        errno = ENOMEM;
        return ERL_DRV_ERROR_ERRNO;
    } else {
        memset(d, 0, sizeof(DrvData));
    }

    // port
    d->port = port;

    return (ErlDrvData) d;
}

void
drv_stop(ErlDrvData handle)
{
    DrvData* d = (DrvData*) handle;

    // alive
    d->impl.alive = 0;

    // db
    delete d->impl.db;

    // db_block_cache
    delete d->impl.db_block_cache;

    // db_filter_policy
    delete d->impl.db_filter_policy;

    // name
    delete d->impl.name;

    driver_free(handle);
}

void
drv_output(ErlDrvData handle, char* buf, ErlDrvSizeT len)
{
    DrvData* d = (DrvData*) handle;
    int ng, index, version, items;
    char command;

#if 0
    {
        int term_type, size_needed;

        index = 0;
        ng = ei_decode_version(buf, &index, &version);
        if (ng) GOTOBADARG;

        ng = ei_get_type(buf, &index, &term_type, &size_needed);
        if (ng) GOTOBADARG;
        fprintf(stderr, "DEBUG %c %d: ", term_type, (int) size_needed);

        index = 0;
        ng = ei_decode_version(buf, &index, &version);
        if (ng) GOTOBADARG;

        ei_print_term(stderr, buf, &index);
        fprintf(stderr, "\n");
    }
#endif

    index = 0;
    ng = ei_decode_version(buf, &index, &version);
    if (ng) GOTOBADARG;

    ng = ei_decode_tuple_header(buf, &index, &items);
    if (ng) GOTOBADARG;
    ng = (items < 1);
    if (ng) GOTOBADARG;

    ng = ei_decode_char(buf, &index, &command);
    if (ng) GOTOBADARG;

    items--;
    switch (command) {
    case LETS_OPEN6:
        ng = (items != 6);
        if (ng) GOTOBADARG;
        lets_output_open6(d, buf, len, &index, items);
        break;
    case LETS_DESTROY6:
        ng = (items != 6);
        if (ng) GOTOBADARG;
        lets_output_destroy6(d, buf, len, &index, items);
        break;
    case LETS_REPAIR6:
        ng = (items != 6);
        if (ng) GOTOBADARG;
        lets_output_repair6(d, buf, len, &index, items);
        break;
    case LETS_INSERT3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_insert3(d, buf, len, &index, items);
        break;
    case LETS_INSERT4:
        ng = (items != 3 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_insert4(d, buf, len, &index, items);
        break;
    case LETS_INSERT_NEW3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        GOTOBADARG;
        break;
    case LETS_INSERT_NEW4:
        ng = (items != 3 || !d->impl.alive);
        if (ng) GOTOBADARG;
        GOTOBADARG;
        break;
    case LETS_DELETE2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_delete2(d, buf, len, &index, items);
        break;
    case LETS_DELETE3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_delete3(d, buf, len, &index, items);
        break;
    case LETS_DELETE_ALL_OBJECTS2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        GOTOBADARG;
        break;
    case LETS_LOOKUP3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_lookup3(d, buf, len, &index, items);
        break;
    case LETS_MEMBER3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_member3(d, buf, len, &index, items);
        break;
    case LETS_FIRST2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_first2(d, buf, len, &index, items);
        break;
    case LETS_FIRST_ITER2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_first_iter2(d, buf, len, &index, items);
        break;
    case LETS_LAST2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_last2(d, buf, len, &index, items);
        break;
    case LETS_LAST_ITER2:
        ng = (items != 1 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_last_iter2(d, buf, len, &index, items);
        break;
    case LETS_NEXT3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_next3(d, buf, len, &index, items);
        break;
    case LETS_NEXT_ITER3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_next_iter3(d, buf, len, &index, items);
        break;
    case LETS_PREV3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_prev3(d, buf, len, &index, items);
        break;
    case LETS_PREV_ITER3:
        ng = (items != 2 || !d->impl.alive);
        if (ng) GOTOBADARG;
        lets_output_prev_iter3(d, buf, len, &index, items);
        break;
    case LETS_INFO_MEMORY1:
        ng = (items != 0 || !d->impl.alive);
        if (ng) GOTOBADARG;
        GOTOBADARG;
        break;
    case LETS_INFO_SIZE1:
        ng = (items != 0 || !d->impl.alive);
        if (ng) GOTOBADARG;
        GOTOBADARG;
        break;
    default:
        GOTOBADARG;
    }
    return;

 badarg:
    driver_send_int(d, LETS_BADARG);
    return;
}

void
drv_ready_async(ErlDrvData handle, ErlDrvThreadData async_data)
{
    DrvData* d = (DrvData*) handle;
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);

    switch (a->reply) {
    case LETS_BADARG:
        driver_send_int(d, LETS_BADARG, a->caller);
        break;
    case LETS_TRUE:
        driver_send_int(d, LETS_TRUE, a->caller);
        break;
    case LETS_FALSE:
        driver_send_int(d, LETS_FALSE, a->caller);
        break;
    case LETS_END_OF_TABLE:
        driver_send_int(d, LETS_END_OF_TABLE, a->caller);
        break;
    case LETS_BINARY:
        driver_send_binary(d, a->binary, a->caller);
        break;
    default:
        driver_send_int(d, LETS_BADARG, a->caller);
    }

    delete a;
}

void
drv_async_free(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    delete a;
}

static int
decode_read_options(leveldb::ReadOptions defaults, char* buf, int* index, int& items, leveldb::ReadOptions& options)
{
    int ng, term_type, size_needed;
    char atom[MAXATOMLEN];
    char* read_options;
    int read_options_len;

    ng = ei_decode_atom(buf, index, atom);
    if (!ng && strcmp(atom, "undefined") == 0) {
        options = defaults;
        items--;
        return 0;
    }

    ng = ei_get_type(buf, index, &term_type, &size_needed);
    if (ng) GOTOBADARG;
    if (!(term_type == ERL_LIST_EXT || term_type == ERL_NIL_EXT)) GOTOBADARG;
    read_options = buf + *index;
    read_options_len = size_needed;
    ng = ei_skip_term(buf, index);
    if (ng) GOTOBADARG;

    if (!lets_parse_read_options(options, read_options, read_options_len)) {
        GOTOBADARG;
    }
    items--;
    return 0;

 badarg:
    return -1;
}

static int
decode_write_options(leveldb::WriteOptions defaults, char* buf, int* index, int& items, leveldb::WriteOptions& options)
{
    int ng, term_type, size_needed;
    char atom[MAXATOMLEN];
    char* write_options;
    int write_options_len;

    ng = ei_decode_atom(buf, index, atom);
    if (!ng && strcmp(atom, "undefined") == 0) {
        options = defaults;
        items--;
        return 0;
    }

    ng = ei_get_type(buf, index, &term_type, &size_needed);
    if (ng) GOTOBADARG;
    if (!(term_type == ERL_LIST_EXT || term_type == ERL_NIL_EXT)) GOTOBADARG;
    write_options = buf + *index;
    write_options_len = size_needed;
    ng = ei_skip_term(buf, index);
    if (ng) GOTOBADARG;

    if (!lets_parse_write_options(options, write_options, write_options_len)) {
        GOTOBADARG;
    }
    items--;
    return 0;

 badarg:
    return -1;
}

//
// Commands
//

static void
lets_output_create6(const char op, DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;
    (void) items;

    DrvAsync* drv_async = NULL;
    char type;
    char privacy;
    char *name;
    long namelen;
    char* options;
    int options_len;
    char* read_options;
    int read_options_len;
    char* write_options;
    int write_options_len;

    int ng, term_type, size_needed;

    if (!ei_inspect_atom(buf, index, (char*) "set")) {
        type = SET;
    } else if (!ei_inspect_atom(buf, index, (char*) "ordered_set")) {
        type = ORDERED_SET;
    } else {
        GOTOBADARG;
    }

    if (!ei_inspect_atom(buf, index, (char*) "private")) {
        privacy = PRIVATE;
    } else if (!ei_inspect_atom(buf, index, (char*) "protected")) {
        privacy = PROTECTED;
    } else if (!ei_inspect_atom(buf, index, (char*) "public")) {
        privacy = PUBLIC;
    } else {
        GOTOBADARG;
    }

    ng = ei_inspect_binary(buf, index, (void**) &name, &namelen);
    if (ng) GOTOBADARG;
    if (!namelen) GOTOBADARG;

    ng = ei_get_type(buf, index, &term_type, &size_needed);
    if (ng) GOTOBADARG;
    if (!(term_type == ERL_LIST_EXT || term_type == ERL_NIL_EXT)) GOTOBADARG;
    options = buf + *index;
    options_len = size_needed;
    ng = ei_skip_term(buf, index);
    if (ng) GOTOBADARG;

    ng = ei_get_type(buf, index, &term_type, &size_needed);
    if (ng) GOTOBADARG;
    if (!(term_type == ERL_LIST_EXT || term_type == ERL_NIL_EXT)) GOTOBADARG;
    read_options = buf + *index;
    read_options_len = size_needed;
    ng = ei_skip_term(buf, index);
    if (ng) GOTOBADARG;

    ng = ei_get_type(buf, index, &term_type, &size_needed);
    if (ng) GOTOBADARG;
    if (!(term_type == ERL_LIST_EXT || term_type == ERL_NIL_EXT)) GOTOBADARG;
    write_options = buf + *index;
    write_options_len = size_needed;
    ng = ei_skip_term(buf, index);
    if (ng) GOTOBADARG;


    if (!lets_init(d->impl, type, privacy, name, namelen)) {
        GOTOBADARG;
    }

    if (!lets_parse_options(d->impl, options, options_len)) {
        GOTOBADARG;
    }
    if (!lets_parse_read_options(d->impl.db_read_options, read_options, read_options_len)) {
        GOTOBADARG;
    }
    if (!lets_parse_write_options(d->impl.db_write_options, write_options, write_options_len)) {
        GOTOBADARG;
    }

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), op);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_create6, drv_async, drv_async_free);
    } else {
        if (!lets_create(d->impl, op)) {
            GOTOBADARG;
        }
        driver_send_int(d, LETS_TRUE);
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_create6(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    if (!lets_create(d->impl, a->command)) {
        a->reply = LETS_BADARG;
    } else {
        a->reply = LETS_TRUE;
    }
}

static void
lets_output_open6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    lets_output_create6(OPEN, d, buf, len, index, items);
}

static void
lets_output_destroy6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    lets_output_create6(DESTROY, d, buf, len, index, items);
}

static void
lets_output_repair6(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    lets_output_create6(REPAIR, d, buf, len, index, items);
}

static void
lets_output_insert3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng, arity;
    char *key;
    long keylen;
    char *blob;
    long bloblen;
    leveldb::WriteOptions write_options;
    leveldb::WriteBatch batch;
    leveldb::Status status;

    ng = decode_write_options(d->impl.db_write_options, buf, index, items, write_options);
    if (ng) GOTOBADARG;
    ng = ei_decode_list_header(buf, index, &items);
    if (ng) GOTOBADARG;

    if (!items) {
        driver_send_int(d, LETS_TRUE);
        return;
    }

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_INSERT3, write_options);
        if (!drv_async) {
            GOTOBADARG;
        }
    }

    while (items) {
        ng = ei_decode_tuple_header(buf, index, &arity);
        if (ng) GOTOBADARG;
        ng = (arity != 2);
        if (ng) GOTOBADARG;
        ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
        if (ng) GOTOBADARG;
        ng = ei_inspect_binary(buf, index, (void**) &blob, &bloblen);
        if (ng) GOTOBADARG;

        if (drv_async) {
            drv_async->put((const char*) key, keylen, (const char*) blob, bloblen);
        } else {
            leveldb::Slice skey((const char*) key, keylen);
            leveldb::Slice sblob((const char*) blob, bloblen);
            batch.Put(skey, sblob);
        }
        items--;
    }

    ng = ei_decode_list_header(buf, index, &items);
    if (ng) GOTOBADARG;
    ng = (items != 0);
    if (ng) GOTOBADARG;

    if (drv_async) {
        driver_async(d->port, NULL, lets_async_insert3, drv_async, drv_async_free);
    } else {
        status = d->impl.db->Write(write_options, &batch);
        if (!status.ok()) {
            GOTOBADARG;
        }

        driver_send_int(d, LETS_TRUE);
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_insert3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Status status = d->impl.db->Write(a->write_options, &(a->batch));
    if (!status.ok()) {
        a->reply = LETS_BADARG;
    } else {
        a->reply = LETS_TRUE;
    }
}

static void
lets_output_insert4(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    char *blob;
    long bloblen;
    leveldb::WriteOptions write_options;
    leveldb::WriteBatch batch;
    leveldb::Status status;

    ng = decode_write_options(d->impl.db_write_options, buf, index, items, write_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &blob, &bloblen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_INSERT4, write_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        drv_async->put((const char*) key, keylen, (const char*) blob, bloblen);
    } else {
        leveldb::Slice skey((const char*) key, keylen);
        leveldb::Slice sblob((const char*) blob, bloblen);
        batch.Put(skey, sblob);
    }

    if (drv_async) {
        driver_async(d->port, NULL, lets_async_insert4, drv_async, drv_async_free);
    } else {
        status = d->impl.db->Write(write_options, &batch);
        if (!status.ok()) {
            GOTOBADARG;
        }

        driver_send_int(d, LETS_TRUE);
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_insert4(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Status status = d->impl.db->Write(a->write_options, &(a->batch));
    if (!status.ok()) {
        a->reply = LETS_BADARG;
    } else {
        a->reply = LETS_TRUE;
    }
}

static void
lets_output_delete2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    leveldb::WriteOptions write_options;
    leveldb::WriteBatch batch;
    leveldb::Status status;

    ng = decode_write_options(d->impl.db_write_options, buf, index, items, write_options);
    if (ng) GOTOBADARG;

    // force sync
    write_options.sync = true;

    // alive
    d->impl.alive = 0;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_DELETE2, write_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_delete2, drv_async, drv_async_free);
    } else {
        status = d->impl.db->Write(write_options, &batch);
        if (!status.ok()) {
            GOTOBADARG;
        }

        // @TBD This is quite risky ... need to re-consider.
        // delete d->impl.db;
        // d->impl.db = NULL;

        driver_send_int(d, LETS_TRUE);
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_delete2(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::WriteBatch batch;
    leveldb::Status status = d->impl.db->Write(a->write_options, &batch);
    if (!status.ok()) {
        a->reply = LETS_BADARG;
    } else {
        // @TBD This is quite risky ... need to re-consider.
        // delete d->impl.db;
        // d->impl.db = NULL;
        a->reply = LETS_TRUE;
    }
}

static void
lets_output_delete3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::WriteOptions write_options;
    leveldb::WriteBatch batch;
    leveldb::Status status;

    ng = decode_write_options(d->impl.db_write_options, buf, index, items, write_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_DELETE3, write_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        drv_async->del((const char*) key, keylen);
    } else {
        leveldb::Slice skey((const char*) key, keylen);
        batch.Delete(skey);
    }

    if (drv_async) {
        driver_async(d->port, NULL, lets_async_delete3, drv_async, drv_async_free);
    } else {
        status = d->impl.db->Write(write_options, &batch);
        if (!status.ok()) {
            GOTOBADARG;
        }

        driver_send_int(d, LETS_TRUE);
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_delete3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Status status = d->impl.db->Write(a->write_options, &(a->batch));
    if (!status.ok()) {
        a->reply = LETS_BADARG;
    } else {
        a->reply = LETS_TRUE;
    }
}

static void
lets_output_lookup3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_LOOKUP3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_lookup3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid() || it->key().compare(skey) != 0) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->value().data(), it->value().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_lookup3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid() || it->key().compare(skey) != 0) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_realloc_binary(a->binary, it->value().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->value().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_member3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_MEMBER3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_member3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid() || it->key().compare(skey) != 0) {
            driver_send_int(d, LETS_FALSE);
            delete it;
            return;
        }

        driver_send_int(d, LETS_TRUE);
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_member3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid() || it->key().compare(skey) != 0) {
        a->reply = LETS_FALSE;
        delete it;
        return;
    }

    a->reply = LETS_TRUE;
    delete it;
}

static void
lets_output_first2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_FIRST2, read_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_first2, drv_async, drv_async_free);
    } else  {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        it->SeekToFirst();
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->key().data(), it->key().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_first2(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    it->SeekToFirst();
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_alloc_binary(it->key().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->key().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_first_iter2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_FIRST_ITER2, read_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_first_iter2, drv_async, drv_async_free);
    } else  {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        it->SeekToFirst();
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->value().data(), it->value().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_first_iter2(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    it->SeekToFirst();
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_alloc_binary(it->value().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->value().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_last2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_LAST2, read_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_last2, drv_async, drv_async_free);
    } else  {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        it->SeekToLast();
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->key().data(), it->key().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_last2(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    it->SeekToLast();
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_alloc_binary(it->key().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->key().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}


static void
lets_output_last_iter2(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_LAST_ITER2, read_options);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_last_iter2, drv_async, drv_async_free);
    } else  {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        it->SeekToLast();
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->value().data(), it->value().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
    return;
}

static void
lets_async_last_iter2(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    it->SeekToLast();
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_alloc_binary(it->value().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->value().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_next3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_NEXT3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_next3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        if (it->key().compare(skey) == 0) {
            it->Next();
            if (!it->Valid()) {
                driver_send_int(d, LETS_END_OF_TABLE);
                delete it;
                return;
            }
        }

        driver_send_buf(d, it->key().data(), it->key().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
}

static void
lets_async_next3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    if (it->key().compare(skey) == 0) {
        it->Next();
        if (!it->Valid()) {
            a->reply = LETS_END_OF_TABLE;
            delete it;
            return;
        }
    }

    ErlDrvBinary* binary = driver_realloc_binary(a->binary, it->key().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->key().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}


static void
lets_output_next_iter3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_NEXT_ITER3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_next_iter3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        if (it->key().compare(skey) == 0) {
            it->Next();
            if (!it->Valid()) {
                driver_send_int(d, LETS_END_OF_TABLE);
                delete it;
                return;
            }
        }

        driver_send_buf(d, it->value().data(), it->value().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
}

static void
lets_async_next_iter3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    if (it->key().compare(skey) == 0) {
        it->Next();
        if (!it->Valid()) {
            a->reply = LETS_END_OF_TABLE;
            delete it;
            return;
        }
    }

    ErlDrvBinary* binary = driver_realloc_binary(a->binary, it->value().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->value().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_prev3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_PREV3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_prev3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid()) {
            it->SeekToLast();
        } else {
            it->Prev();
        }

        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->key().data(), it->key().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
}

static void
lets_async_prev3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid()) {
        it->SeekToLast();
    } else {
        it->Prev();
    }

    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_realloc_binary(a->binary, it->key().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->key().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}

static void
lets_output_prev_iter3(DrvData* d, char* buf, ErlDrvSizeT len, int* index, int items)
{
    (void) len;

    DrvAsync* drv_async = NULL;
    int ng;
    char *key;
    long keylen;
    leveldb::ReadOptions read_options;

    ng = decode_read_options(d->impl.db_read_options, buf, index, items, read_options);
    if (ng) GOTOBADARG;
    ng = ei_inspect_binary(buf, index, (void**) &key, &keylen);
    if (ng) GOTOBADARG;

    if (d->impl.async) {
        drv_async = new DrvAsync(d, driver_caller(d->port), LETS_PREV_ITER3, read_options, (const char*) key, keylen);
        if (!drv_async) {
            GOTOBADARG;
        }
        driver_async(d->port, NULL, lets_async_prev_iter3, drv_async, drv_async_free);
    } else {
        leveldb::Iterator* it = d->impl.db->NewIterator(read_options);
        if (!it) {
            GOTOBADARG;
        }

        leveldb::Slice skey((const char*) key, keylen);
        it->Seek(skey);
        if (!it->Valid()) {
            it->SeekToLast();
        } else {
            it->Prev();
        }

        if (!it->Valid()) {
            driver_send_int(d, LETS_END_OF_TABLE);
            delete it;
            return;
        }

        driver_send_buf(d, it->value().data(), it->value().size());
        delete it;
    }
    return;

 badarg:
    if (drv_async) { delete drv_async; }
    driver_send_int(d, LETS_BADARG);
}

static void
lets_async_prev_iter3(void* async_data)
{
    DrvAsync* a = (DrvAsync*) async_data;
    assert(a != NULL);
    DrvData* d = a->drvdata;

    leveldb::Iterator* it = d->impl.db->NewIterator(a->read_options);
    if (!it) {
        a->reply = LETS_BADARG;
        return;
    }

    leveldb::Slice skey((const char*) a->binary->orig_bytes, a->binary->orig_size);
    it->Seek(skey);
    if (!it->Valid()) {
        it->SeekToLast();
    } else {
        it->Prev();
    }

    if (!it->Valid()) {
        a->reply = LETS_END_OF_TABLE;
        delete it;
        return;
    }

    ErlDrvBinary* binary = driver_realloc_binary(a->binary, it->value().size());
    if (binary) {
        memcpy(binary->orig_bytes, it->value().data(), binary->orig_size);
        a->binary = binary;
        a->reply = LETS_BINARY;
    } else {
        a->reply = LETS_BADARG;
    }

    delete it;
}
