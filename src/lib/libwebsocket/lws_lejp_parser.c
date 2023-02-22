/*
 * lws_lejp_parser.c 
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
 *	  src/lib/libwebsocket/lws_lejp_parser.c
 */
#include <stdio.h>
#include <stddef.h>
#include "libwebsockets.h"

#include "spec/packstream/pack_utils.h"
#include "spec/packstream/v100/packstream_v100.h"
#include "libwebsocket/lws.h"

#define LEJP_JSON_BUFFER 100

static struct lejp_json *lj;

static const char *const reason_names[] = {
        "LEJPCB_CONSTRUCTED",
        "LEJPCB_DESTRUCTED",
        "LEJPCB_START",
        "LEJPCB_COMPLETE",
        "LEJPCB_FAILED",
        "LEJPCB_PAIR_NAME",
        "LEJPCB_VAL_TRUE",
        "LEJPCB_VAL_FALSE",
        "LEJPCB_VAL_NULL",
        "LEJPCB_VAL_NUM_INT",
        "LEJPCB_VAL_NUM_FLOAT",
        "LEJPCB_VAL_STR_START",
        "LEJPCB_VAL_STR_CHUNK",
        "LEJPCB_VAL_STR_END",
        "LEJPCB_ARRAY_START",
        "LEJPCB_ARRAY_END",
        "LEJPCB_OBJECT_START",
        "LEJPCB_OBJECT_END",
        "LEJPCB_OBJECT_END_PRE",
};

static const char *const tok[] = {
        "dummy___"
};

static signed char
cb(struct lejp_ctx *ctx, char reason) {
    char buf[1024];
    char *p = buf;
    char *end = &buf[sizeof(buf)];
    int n;

    // user code
    static size_t dict_len;
    static char *pack_dict_key = NULL;
    static size_t pack_dict_key_len = 0;
    static char *pack_dict_val = NULL;
    static size_t pack_dict_val_len = 0;

    for (n = 0; n < ctx->sp; n++)
        *p++ = ' ';
    *p = '\0';

    if (reason & LEJP_FLAG_CB_IS_VALUE) {
        switch (reason) {
            case LEJPCB_VAL_TRUE:
            case LEJPCB_VAL_FALSE:
            case LEJPCB_VAL_NULL:
            case LEJPCB_VAL_NUM_INT:
            case LEJPCB_VAL_NUM_FLOAT:
            case LEJPCB_VAL_STR_CHUNK:
            case LEJPCB_VAL_STR_END:
                pack_dict_val = ctx->buf;
                pack_dict_val_len = strlen(buf);
                lj->value = ctx->buf;
                break;
            default:
                p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "   value '%s' ", ctx->buf);
                if (ctx->ipos) {
                    p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "(array indexes: ");
                    for (n = 0; n < ctx->ipos; n++) {
                        p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), "%d ", ctx->i[n]);
                    }
                    p += lws_snprintf(p, lws_ptr_diff_size_t(end, p), ") ");
                }
        }
        lwsl_notice("%s (%s)\r\n", buf, reason_names[(unsigned int) (reason) & (LEJP_FLAG_CB_IS_VALUE - 1)]);
        return 0;
    }

    switch (reason) {
        case LEJPCB_COMPLETE:
//            lwsl_notice("%sParsing Completed (LEJPCB_COMPLETE)\n", buf);
            break;
        case LEJPCB_PAIR_NAME:
            lwsl_notice("%spath: '%s' (LEJPCB_PAIR_NAME)\n", buf, ctx->path);
            pack_dict_key = ctx->path;
            pack_dict_key_len = strlen(buf);
            lj->key = ctx->path;
            break;
    }
    return 0;
}

int lws_lejp_parser(char json_str[]) {
    int n = 1;
    int ret = 1;
    int m = 0;
    struct lejp_ctx ctx;
    char buf[512];

    lejp_construct(&ctx, cb, NULL, tok, LWS_ARRAY_SIZE(tok));

    memcpy(buf, json_str, strlen(json_str) + 1);
    n = strlen(json_str) + 1;

    m = lejp_parse(&ctx, (uint8_t *) buf, n);

    if (m < 0 && m != LEJP_CONTINUE) {
        lwsl_err("parse failed %d\n", m);
    }
    lwsl_notice("parse okay (%d)\n", m);
    lejp_destruct(&ctx);
    return 0;
}

struct lejp_json *get_lejp_json() {
    return lj;
}

struct lejp_json *insert_lejp_json(struct lejp_json *top, struct lejp_json *target) {
    struct lejp_json *tmp = top;

    while (1) {
        if (tmp->next == NULL)
            break;
        else {
            tmp = tmp->next;
        }
    }
    top->next = target;
    return lj;
}

char *lejp_to_pack(struct lejp_json *l) {
    char *ret;
    char *pack_concat = NULL;
    size_t pack_concat_len = 0;
    char *k;
    char *v;

    size_t total_count = 0;
    for (struct lejp_json *i = l; i->next != NULL; i = i->next) {
        k = i->key;
        v = i->value;
        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_string8(k));
        pack_concat_len += pack_appender(pack_concat_len, &pack_concat, write_string8(v));
        total_count++;
    }

    pack_concat_len += pack_appender(pack_concat_len, &ret, write_tiny_struct_marker(total_count));
    return "";
}