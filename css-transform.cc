/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */
/*
 * css-transform.c:  (Derived from append-transform)
 *    Usage:
 *     (NT): CssTransform.dll <filename>
 *     (Solaris): css-transform.so <filename>
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <ts/ts.h>
#include <pcre.h>
#include <pcrecpp.h>

#define STATE_BUFFER_DATA     0
#define STATE_TRANSFORM_DATA  1 
#define STATE_OUTPUT_DATA     2
#define ASSERT_SUCCESS(_x) INKAssert ((_x) == INK_SUCCESS)

using namespace std;

typedef struct {
  int size, state;
  INKVIO output_vio;
  INKIOBuffer output_buffer;
  INKIOBufferReader output_reader;
  INKIOBuffer min_buffer;
  INKIOBufferReader min_buffer_reader;
} Data;

static Data * my_data_alloc() {
  Data *data;
  data = (Data *) INKmalloc(sizeof(Data));
  INKReleaseAssert(data && data != INK_ERROR_PTR);

  data->state = STATE_BUFFER_DATA;
  data->size = 0;
  data->output_vio = NULL;
  data->output_buffer = NULL;
  data->output_reader = NULL;
  data->min_buffer = INKIOBufferCreate();
  data->min_buffer_reader = INKIOBufferReaderAlloc(data->min_buffer);
  INKAssert(data->min_buffer_reader != INK_ERROR_PTR);
  return data;
}

static void my_data_destroy(Data * data) {
  if (data) {
    if (data->output_buffer) {
      ASSERT_SUCCESS(INKIOBufferDestroy(data->output_buffer));
    }
    INKfree(data);
  }
}

static void write_iobuffer(const char *buf, int len, INKIOBuffer output) {
  INKIOBufferBlock block;
  char *ptr_block;
  int ndone, ntodo, towrite, avail;

  ndone = 0;
  ntodo = len;
  while (ntodo > 0) {
    block = INKIOBufferStart(output);
    ptr_block = INKIOBufferBlockWriteStart(block, &avail);
    towrite = min(ntodo, avail);
    memcpy(ptr_block, buf + ndone, towrite);
    INKIOBufferProduce(output, towrite);
    ntodo -= towrite;
    ndone += towrite;
  }
}
static void cssmin_transform(Data *data) {
  INKIOBufferBlock block = INKIOBufferReaderStart(data->output_reader);

  while (block != NULL) {
    int blocklen;
    const char * blockptr = INKIOBufferBlockReadStart(block, data->output_reader, &blocklen);
    string str (blockptr);

    // Strip extra spaces
    pcrecpp::RE("\\s+").GlobalReplace(" ", &str);
    pcrecpp::RE("\\s}\\s*").GlobalReplace("}", &str);
    pcrecpp::RE("\\s{\\s*").GlobalReplace("{", &str);

    //  Remove extra semicolons
    pcrecpp::RE(";+").GlobalReplace(";", &str);
    pcrecpp::RE(":(?:0 )+0;").GlobalReplace(":0;", &str);

    // write this block and move on
    write_iobuffer(str.c_str(), strlen(str.c_str()), data->min_buffer);

    // Parse next block
    block = INKIOBufferBlockNext(block);
    if (block == INK_ERROR_PTR) {
      INKError("[strsearch_ioreader] Error while getting block from ioreader");
    }
  }
}

static int handle_buffering(INKCont contp, Data * data) {
  INKVIO write_vio;
  int towrite;
  int avail;

  /* Get the write VIO for the write operation that was performed on
     ourself. This VIO contains the buffer that we are to read from
     as well as the continuation we are to call when the buffer is
     empty. */
  write_vio = INKVConnWriteVIOGet(contp);

  /* Create the output buffer and its associated reader */
  if (!data->output_buffer) {
    data->output_buffer = INKIOBufferCreate();
    INKAssert(data->output_buffer);
    data->output_reader = INKIOBufferReaderAlloc(data->output_buffer);
    INKAssert(data->output_reader);
  }

  /* We also check to see if the write VIO's buffer is non-NULL. A
     NULL buffer indicates that the write operation has been
     shutdown and that the continuation does not want us to send any
     more WRITE_READY or WRITE_COMPLETE events. For this buffered
     transformation that means we're done buffering data. */

  if (!INKVIOBufferGet(write_vio)) {
    data->state = STATE_TRANSFORM_DATA;
    return 0;
  }

  towrite = INKVIONTodoGet(write_vio);
  if (towrite > 0) {
    /* The amount of data left to read needs to be truncated by
       the amount of data actually in the read buffer. */

    avail = INKIOBufferReaderAvail(INKVIOReaderGet(write_vio));
    if (towrite > avail) {
      towrite = avail;
    }

    if (towrite > 0) {
      /* Copy the data from the read buffer to the input buffer. */
      if (INKIOBufferCopy(data->output_buffer, INKVIOReaderGet(write_vio), towrite, 0) == INK_ERROR) {
        INKError("[css-transform] Unable to copy read buffer\n");
        return 0;
      }

      /* Tell the read buffer that we have read the data and are no
         longer interested in it. */
      if (INKIOBufferReaderConsume(INKVIOReaderGet(write_vio), towrite) == INK_ERROR) {
        INKError("[css-transform] Unable to copy read buffer\n");
        return 0;
      }

      /* Modify the write VIO to reflect how much data we've
         completed. */
      if (INKVIONDoneSet(write_vio, INKVIONDoneGet(write_vio)
                         + towrite) == INK_ERROR) {
        INKError("[css-transform] Unable to copy read buffer\n");
        return 0;
      }
    }
  }

  /* Now we check the write VIO to see if there is data left to read. */
  if (INKVIONTodoGet(write_vio) > 0) {
    if (towrite > 0) {
      /* Call back the write VIO continuation to let it know that we
         are ready for more data. */
      INKContCall(INKVIOContGet(write_vio), INK_EVENT_VCONN_WRITE_READY, write_vio);
    }
  } else {
    data->state = STATE_TRANSFORM_DATA;
    INKContCall(INKVIOContGet(write_vio), INK_EVENT_VCONN_WRITE_COMPLETE, write_vio);
  }

  return 1;
}

static int handle_output(INKCont contp, Data * data) {
  /* Check to see if we need to initiate the output operation. */
  if (!data->output_vio) {
    INKVConn output_conn;

    /* Get the output connection where we'll write data to. */
    output_conn = INKTransformOutputVConnGet(contp);

    data->output_vio =
      INKVConnWrite(output_conn, contp, data->min_buffer_reader, INKIOBufferReaderAvail(data->min_buffer_reader));

    INKAssert(data->output_vio);
  }
  return 1;
}

static void handle_transform(INKCont contp) {
  Data *data;
  int done;

  /* Get our data structure for this operation. The private data
     structure contains the output VIO and output buffer. If the
     private data structure pointer is NULL, then we'll create it
     and initialize its internals. */

  data = (Data *) INKContDataGet(contp);
  if (!data) {
    data = my_data_alloc();
    INKContDataSet(contp, data);
  }

  do {
    switch (data->state) {
    case STATE_BUFFER_DATA:
      done = handle_buffering(contp, data);
      break;
    case STATE_TRANSFORM_DATA:
      cssmin_transform(data);
    case STATE_OUTPUT_DATA:
      done = handle_output(contp, data);
      break;
    default:
      done = 1;
      break;
    }
  } while (!done);
}


static int transform(INKCont contp, INKEvent event, void *edata) {
  //Check to see if the transformation has been closed by a call to INKVConnClose
  if (INKVConnClosedGet(contp)) {
    my_data_destroy((Data *) INKContDataGet(contp));
    ASSERT_SUCCESS(INKContDestroy(contp));
    return 0;
  } else {
    switch (event) {
    case INK_EVENT_ERROR: {
        INKVIO write_vio;
        write_vio = INKVConnWriteVIOGet(contp);
        INKContCall(INKVIOContGet(write_vio), INK_EVENT_ERROR, write_vio);
      }
      break;
    case INK_EVENT_VCONN_WRITE_COMPLETE:
      ASSERT_SUCCESS(INKVConnShutdown(INKTransformOutputVConnGet(contp), 0, 1));
      break;
    case INK_EVENT_VCONN_WRITE_READY:
    default:
      handle_transform(contp);
      break;
    }
  }
  return 0;
}

static int transformable(INKHttpTxn txnp) {
  INKMBuffer bufp;
  INKMLoc hdr_loc;
  INKMLoc field_loc;
  INKHttpStatus resp_status;
  const char *value;
  int val_length;

  INKHttpTxnServerRespGet(txnp, &bufp, &hdr_loc);

  if (INK_HTTP_STATUS_OK == (resp_status = INKHttpHdrStatusGet(bufp, hdr_loc))) {
    field_loc = INKMimeHdrFieldFind(bufp, hdr_loc, "Content-Type", 12);
    if (!field_loc) {
      ASSERT_SUCCESS(INKHandleMLocRelease(bufp, INK_NULL_MLOC, hdr_loc));
      return 0;
    }

    value = INKMimeHdrFieldValueGet(bufp, hdr_loc, field_loc, 0, &val_length);
#ifndef _WIN32
    if (value && (strncasecmp(value, "text/css", sizeof("text/css") - 1) == 0)) {
#else
    if (value && (strnicmp(value, "text/css", sizeof("text/css") - 1) == 0)) {
#endif
      ASSERT_SUCCESS(INKHandleStringRelease(bufp, field_loc, value));
      ASSERT_SUCCESS(INKHandleMLocRelease(bufp, hdr_loc, field_loc));
      ASSERT_SUCCESS(INKHandleMLocRelease(bufp, INK_NULL_MLOC, hdr_loc));
      return 1;
    } else {
      ASSERT_SUCCESS(INKHandleStringRelease(bufp, field_loc, value));
      ASSERT_SUCCESS(INKHandleMLocRelease(bufp, hdr_loc, field_loc));
      ASSERT_SUCCESS(INKHandleMLocRelease(bufp, INK_NULL_MLOC, hdr_loc));
      return 0;
    }
  }
  return 0;
}

static void transform_add(INKHttpTxn txnp) {
  INKVConn connp;
  connp = INKTransformCreate(transform, txnp);
  if (INKHttpTxnHookAdd(txnp, INK_HTTP_RESPONSE_TRANSFORM_HOOK, connp) == INK_ERROR) {
    INKError("[css-transform] Unable to attach plugin to http transaction\n");
  }
}

static int transform_plugin(INKCont contp, INKEvent event, void *edata) {
  INKHttpTxn txnp = (INKHttpTxn) edata;
  switch (event) {
  case INK_EVENT_HTTP_READ_RESPONSE_HDR:
    if (transformable(txnp)) {
      transform_add(txnp);
    }
    ASSERT_SUCCESS(INKHttpTxnReenable(txnp, INK_EVENT_HTTP_CONTINUE));
    return 0;
  default:
    break;
  }

  return 0;
}

void INKPluginInit(int argc, const char *argv[]) {
  if (INKHttpHookAdd(INK_HTTP_READ_RESPONSE_HDR_HOOK, INKContCreate(transform_plugin, NULL)) == INK_ERROR) {
    INKError("[css-transform] Unable to set read response header\n");
    INKError("[css-transform] Unable to initialize plugin\n");
  }
  return;
}
