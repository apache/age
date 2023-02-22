/*
 * pack_utils.c 
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *	  src/spec/packstream/pack_utils.c
 */

#include "graph_protocol_stddef.h"

#include "spec/packstream/v100/packstream_v100.h"
#include "spec/packstream/v100/coretypes.h"
#include "spec/packstream/v100/marker.h"
#include "spec/packstream/pack_utils.h"
#include "utils/utils.h"


#include "spec/bolt/message/v440/signature.h"
#include "spec/bolt/message/message_utils.h"

int pack_appender(size_t dest_size, char **dest, char *pack) {

    size_t cat_len;
    int pack_len;
    pack_len = extract_pack_length(pack);
    cat_len = dest_size + pack_len;
    cat_len = concaternating_byte_array(dest, dest_size, pack, pack_len);

    free(pack);

    return pack_len;
}


bool is_marker_data_itself(uint8 c) {
    switch (c & NIBBLE_HIGH) {
        case PM_TINY_INT:
            return true;
    }
    return false;
}

bool is_marker_with_sizebyte(uint8 c) {
    switch (c) {
        case PM_STRING8:
        case PM_STRING16:
        case PM_INT8:
        case PM_INT16:
            return true;
        default:
            return false;
    }
}

bool is_tiny_container_marker(uint8 c) {
    switch (c) {
        case BS_RECORD:
        case BS_SUCCESS:
        case BS_PULL:
            return true;
        default:
            break;
    }

    switch (c & NIBBLE_HIGH) {
        case PM_TINY_LIST:
        case PM_TINY_DICT:
        case PM_TINY_STRUCT:
            return true;
        default:
            break;
    }
    return false;
}

bool is_tiny_marker(uint8 c) {
    switch (c) {
        case PM_NULL:
        case PM_FALSE:
        case PM_TRUE:
            return true;
        default:
            switch (c & NIBBLE_HIGH) {
                case PM_TINY_INT:
                case PM_TINY_STRING:
                    return true;
                default:
                    return false;
            }
    }
}

bool is_container_marker(uint8 c) {
    switch (c) {
        case PM_LIST8:
        case PM_LIST16:
        case PM_LIST32:
        case PM_DICT8:
        case PM_DICT16:
        case PM_DICT32:
            return true;
        default:
            return false;
    }
}


int extract_size_byte_from_marker(uint8 c) {
    int size_byte_length = 0;
    switch (c) {
        case PM_INT8:
        case PM_BYTES8:
        case PM_STRING8:
        case PM_LIST8:
        case PM_DICT8:
            size_byte_length = 1;
            break;
        case PM_INT16:
        case PM_BYTES16:
        case PM_STRING16:
        case PM_LIST16:
        case PM_DICT16:
            size_byte_length = 2;
            break;
        case PM_INT32:
        case PM_BYTES32:
        case PM_STRING32:
        case PM_LIST32:
        case PM_DICT32:
            size_byte_length = 4;
            break;
        case PM_INT64:
        case PM_FLOAT64:
            size_byte_length = 8;
            break;
        default:
            return 0;
    }

    return size_byte_length;
}

int extract_pack_length(const char pack[]) {

    int pack_len = 0;

    uint8 first_byte = pack[0]; // pack_type
    uint8 second_byte = pack[1]; // if not a tiny type, pack_sizetype

    union size_cal {
        uint8 c[8];
        uint16 s[4];
        uint32 i[2];
        uint64 l;
    } pack_contents_size;
    memset(&pack_contents_size, 0, sizeof(pack_contents_size));

    size_t total_pack_len;
    int size_byte_length = extract_size_byte_from_marker(first_byte);
    printf("size_byte_length=%d", size_byte_length);

    switch (first_byte) {
        case PM_INT16:
            return 3;
    }

//    switch (size_byte_length) {
//        case 1:
//            pack_contents_size.c[0] = pack[2];
//            total_pack_len = PACK_MARKER_LENGTH + size_byte_length + pack_contents_size.c[0];
//            printf("total_pack_len:%ld", total_pack_len);
//            return total_pack_len;
//        case 2:
//            pack_contents_size.c[0] = pack[2];
//            pack_contents_size.c[1] = pack[3];
//            total_pack_len = PACK_MARKER_LENGTH + size_byte_length + ntohs(pack_contents_size.s[0]);
//            printf("total_pack_len:%ld", total_pack_len);
//            return total_pack_len;
//        case 4:
//            pack_contents_size.c[0] = pack[2];
//            pack_contents_size.c[1] = pack[3];
//            pack_contents_size.c[2] = pack[4];
//            pack_contents_size.c[3] = pack[5];
//            total_pack_len = PACK_MARKER_LENGTH + size_byte_length + ntohl(pack_contents_size.i[0]);
//            printf("total_pack_len:%ld", total_pack_len);
//            return total_pack_len;
//        case 8:
//            pack_contents_size.c[0] = pack[2];
//            pack_contents_size.c[1] = pack[3];
//            pack_contents_size.c[2] = pack[4];
//            pack_contents_size.c[3] = pack[5];
//            pack_contents_size.c[4] = pack[6];
//            pack_contents_size.c[5] = pack[7];
//            pack_contents_size.c[6] = pack[8];
//            pack_contents_size.c[7] = pack[9];
//            total_pack_len = PACK_MARKER_LENGTH + size_byte_length + ntohl(pack_contents_size.l);
//            printf("total_pack_len:%ld", total_pack_len);
//            return total_pack_len;
//        default:
//            break;
//    }


    if (is_marker_data_itself(first_byte)
        || is_tiny_container_marker(first_byte)) {
        pack_len = PACK_MARKER_LENGTH;
    } else if (is_tiny_marker(first_byte)) {
        pack_len = (NIBBLE_LOW & first_byte) + PACK_MARKER_LENGTH;
    } else if (is_bolt_signature(first_byte)) {
        pack_len = 1;
    } else if (is_bolt_tag(first_byte)) {
        pack_len = 1;
    } else if (is_container_marker(first_byte)) {
        pack_len = PACK_MARKER_LENGTH + PACK_SIZE_LENGTH;
    } else if (is_marker_with_sizebyte(first_byte)) {
        pack_len = PACK_MARKER_LENGTH + PACK_SIZE_LENGTH + second_byte;
    } else {
        perror("non of pack_length extracted. pack_len=1 assigned");
        pack_len = 1;
    }

    return pack_len;
}
