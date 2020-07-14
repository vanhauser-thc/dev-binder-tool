/* Copyright 2008 The Android Open Source Project
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define TRACE 1

#include "binder.h"

uint32_t svcmgr_lookup(struct binder_state *bs, uint32_t target, const char *name) {
  uint32_t handle;
  unsigned iodata[512 / 4];
  struct binder_io msg, reply;

  bio_init(&msg, iodata, sizeof(iodata), 4);
  bio_put_uint32(&msg, 0); // strict mode header
  bio_put_string16_x(&msg, SVC_MGR_NAME);
  bio_put_string16_x(&msg, name);

  if (binder_call(bs, &msg, &reply, target, SVC_MGR_CHECK_SERVICE))
    return 0;

  handle = bio_get_ref(&reply);

  if (handle)
    binder_acquire(bs, handle);

  binder_done(bs, &msg, &reply);

  return handle;
}

int svcmgr_publish(struct binder_state *bs, uint32_t target, const char *name, void *ptr) {
  int status;
  unsigned iodata[512 / 4];
  struct binder_io msg, reply;

  bio_init(&msg, iodata, sizeof(iodata), 4);
  bio_put_uint32(&msg, 0); // strict mode header
  bio_put_string16_x(&msg, SVC_MGR_NAME);
  bio_put_string16_x(&msg, name);
  bio_put_obj(&msg, ptr);

  if (binder_call(bs, &msg, &reply, target, SVC_MGR_ADD_SERVICE))
    return -1;

  status = bio_get_uint32(&reply);

  if (status == 0) {
    printf("Press ENTER to terminate publish\n");
    status = fgetc(stdin);
    status = 0;
  }

  binder_done(bs, &msg, &reply);

  return status;
}

int svcmgr_list(struct binder_state *bs, uint32_t target) {
  int status = 0, index = 0, fail = 0;
  size_t len;
  unsigned iodata[512 / 4];
  struct binder_io msg, reply;
  uint16_t *s;

  while (!status && fail < 100) {
    bio_init(&msg, iodata, sizeof(iodata), 4);
    bio_put_uint32(&msg, 0); // strict mode header
    bio_put_string16_x(&msg, SVC_MGR_NAME);
    bio_put_uint32(&msg, index);

    if ((status = binder_call(bs, &msg, &reply, target, SVC_MGR_LIST_SERVICES)) == 0) {
      s = bio_get_string16(&reply, &len);
      if (s && len) {
        printf("Index %d: ", index);
        for (int i = 0; i < len; i++)
          printf("%c", (unsigned char)s[i]);
        printf("\n");
      } else {
        fail++;
      }
    } else
      fail++;
    index++;
  }

  return status;
}

unsigned token;

int main(int argc, char **argv) {
  int fd;
  struct binder_state *bs;
  uint32_t svcmgr = BINDER_SERVICE_MANAGER;
  uint32_t handle;

  if (argc < 2 || argv[1][0] == '-') {
    printf("Syntax: %s [list|alt|lookup <service>|publish <service>]\n\n", argv[0]);
    printf("/dev/binder test tool\n");
    return -1;
  }

  bs = binder_open(128 * 1024);
  if (!bs) {
    fprintf(stderr, "failed to open binder driver\n");
    return -1;
  }

  argc--;
  argv++;
  while (argc > 0) {
    if (!strcmp(argv[0], "alt")) {
      handle = svcmgr_lookup(bs, svcmgr, "alt_svc_mgr");
      if (!handle) {
        fprintf(stderr, "cannot find alt_svc_mgr\n");
        return -1;
      }
      svcmgr = handle;
      fprintf(stderr, "svcmgr is via %x\n", handle);
    } else if (!strcmp(argv[0], "lookup")) {
      if (argc < 2) {
        fprintf(stderr, "argument required\n");
        return -1;
      }
      handle = svcmgr_lookup(bs, svcmgr, argv[1]);
      fprintf(stderr, "lookup(%s) = %x\n", argv[1], handle);
      argc--;
      argv++;
    } else if (!strcmp(argv[0], "publish")) {
      if (argc < 2) {
        fprintf(stderr, "argument required\n");
        return -1;
      }
      svcmgr_publish(bs, svcmgr, argv[1], &token);
      argc--;
      argv++;
    } else if (!strcmp(argv[0], "list")) {
      svcmgr_list(bs, svcmgr);
      argc--;
      argv++;
    } else {
      fprintf(stderr, "unknown command %s\n", argv[0]);
      return -1;
    }
    argc--;
    argv++;
  }
  return 0;
}
