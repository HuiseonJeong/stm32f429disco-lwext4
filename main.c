/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include <hw_init.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <time.h>

#include <usb_msc_lwext4.h>
#include <ext4.h>
#include <ext4_mbr.h>
#include "test_lwext4.h"

/**@brief   Read-write size*/
#define READ_WRITE_SZIZE 1024 * 16

/**@brief   Delay test (slower LCD scroll)*/
#define TEST_DELAY_MS 1000

/**@brief   Input stream name.*/
char input_name[128] = "ext2";

/**@brief   Read-write size*/
static int rw_szie = READ_WRITE_SZIZE;

/**@brief   Read-write buffer*/
static uint8_t rw_buff[READ_WRITE_SZIZE];

/**@brief   Read-write size*/
static int rw_count = 100;

/**@brief   Directory test count*/
static int dir_cnt = 100;

/**@brief   Static or dynamic cache mode*/
static bool cache_mode = false;

/**@brief   Cleanup after test.*/
static bool cleanup_flag = false;

/**@brief   Block device stats.*/
static bool bstat = false;

/**@brief   Superblock stats.*/
static bool sbstat = false;

/**@brief   Block device handle.*/
static struct ext4_blockdev *parent_blockdev;

/**@brief   MBR blockdevices.*/
static struct ext4_mbr_bdevs bdevs;

/**@brief   Block cache handle.*/
static struct ext4_bcache *bc;

static bool open_filedev(void)
{
	parent_blockdev = ext4_usb_msc_get();
	if (!parent_blockdev) {
		printf("open_filedev: fail\n");
		return false;
	}
	return true;
}

static bool mbr_scan(void)
{
	int r;
	printf("ext4_mbr\n");
	r = ext4_mbr_scan(parent_blockdev, &bdevs);
	if (r != EOK) {
		printf("ext4_mbr_scan error\n");
		return false;
	}

	int i;
	printf("ext4_mbr_scan:\n");
	for (i = 0; i < 4; i++) {
		printf("mbr_entry %d:\n", i);
		if (!bdevs.partitions[i].bdif) {
			printf("\tempty/unknown\n");
			continue;
		}

		printf(" offeset: 0x%"PRIx64", %"PRIu64"MB\n",
			bdevs.partitions[i].part_offset,
			bdevs.partitions[i].part_offset / (1024 * 1024));
		printf(" size:    0x%"PRIx64", %"PRIu64"MB\n",
			bdevs.partitions[i].part_size,
			bdevs.partitions[i].part_size / (1024 * 1024));


	}

	return true;
}

static bool do_tests(struct ext4_blockdev *part_blockdev)
{
	tim_wait_ms(TEST_DELAY_MS);
	if (!test_lwext4_mount(part_blockdev, bc))
		return false;

	tim_wait_ms(TEST_DELAY_MS);

	ext4_cache_write_back("/mp/", 1);
	test_lwext4_cleanup();

	if (sbstat) {
		tim_wait_ms(TEST_DELAY_MS);
		test_lwext4_mp_stats();
	}

	tim_wait_ms(TEST_DELAY_MS);
	test_lwext4_dir_ls("/mp/");
	if (!test_lwext4_dir_test(dir_cnt))
		return false;

	tim_wait_ms(TEST_DELAY_MS);
	if (!test_lwext4_file_test(rw_buff, rw_szie, rw_count))
		return false;

	if (sbstat) {
		tim_wait_ms(TEST_DELAY_MS);
		test_lwext4_mp_stats();
	}

	if (cleanup_flag) {
		tim_wait_ms(TEST_DELAY_MS);
		test_lwext4_cleanup();
	}

	if (bstat) {
		tim_wait_ms(TEST_DELAY_MS);
		test_lwext4_block_stats();
	}

	ext4_cache_write_back("/mp/", 0);
	if (!test_lwext4_umount())
		return false;

	return true;
}


int main(void)
{
	int i;
	hw_init();

	setbuf(stdout, 0);
	printf("connect usb drive...\n");

	while (!hw_usb_connected())
		hw_usb_process();
	printf("usb drive connected\n");

	while (!hw_usb_enum_done())
		hw_usb_process();
	printf("usb drive enum done\n");

	hw_led_red(1);

	printf("test conditions:\n");
	printf("  rw size: %d\n", rw_szie);
	printf("  rw count: %d\n", rw_count);
	printf("  cache mode: %s\n", cache_mode ? "dynamic" : "static");

	if (!open_filedev())
		goto Finish;
	if (!mbr_scan())
		goto Finish;

	/*Execute tests for every scaned partition.*/
	for (i = 0; i < 4; i++) {
		if (!bdevs.partitions[i].bdif)
			continue;

		printf("do tests for mbr_entry %d:\n", i);
		printf(" offeset: 0x%"PRIx64", %"PRIu64"MB\n",
			bdevs.partitions[i].part_offset,
			bdevs.partitions[i].part_offset / (1024 * 1024));
		printf(" size:    0x%"PRIx64", %"PRIu64"MB\n",
			bdevs.partitions[i].part_size,
			bdevs.partitions[i].part_size / (1024 * 1024));

		tim_wait_ms(TEST_DELAY_MS);
		if (!do_tests(&bdevs.partitions[i]))
			goto Finish;
	}

	printf("press RESET button to restart\n");
Finish:
	while (1) {
		tim_wait_ms(500);
		hw_led_green(1);
		tim_wait_ms(500);
		hw_led_green(0);
	}
}
