/*
 * Copyright(c) 2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Description:
 *    It's the user mode application to show the busy rate of GPU instances,
 *    including both the HW and virtual type. This app could be used to evaluate
 *    the GPU virtualization performance, which is known as GVT.
 *
 *    This app's work depends on an i915 exported sysfs interface:
 *    /sys/class/drm/card0/gvt_state.
 *    Check if this interface exists if you see app not works propertly.
 *
 * Authors:
 *    Pei Zhang <pei.zhang@intel.com>
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

typedef unsigned long long cycles_t;

#define GVT_STATE_ENGINES   5
#define GVT_STATE_GPUS	    15

static const char *bars[] = {
	" ",
	"▏",
	"▎",
	"▍",
	"▌",
	"▋",
	"▊",
	"▉",
	"█"
};

const char *intel_engine_name[] = {
	"RCS",
	"BCS",
	"VCS",
	"VCS2",
	"VECS"
};

struct engine_idle_state {
	cycles_t idle_time;
	unsigned int submit_count;
	unsigned int idle_count;
};

struct gpu_idle_state {
	bool valid;
	struct engine_idle_state engines[GVT_STATE_ENGINES];

	unsigned int read_trap_count;
	cycles_t read_trap_time;
	unsigned int write_trap_count;
	cycles_t write_trap_time;
};

long int getHexValue(char* str)
{
	return strtol(str, NULL, 16);
}

long int getIntValue(char* str)
{
	return strtol(str, NULL, 10);
}

char ** getFileStrings(FILE* file, int* lineCount)
{
	int x = 0;
	char line[500];
	char **lineArray = (char**) malloc(500 * sizeof(char*));

	if (NULL == file || NULL == lineCount || NULL == lineArray) {
		if (lineCount)
			*lineCount = 0;
		return NULL;
	}

	while (x < 500 && fgets(line, sizeof(line), file) ){
		// caller should free duplicated line memory
		lineArray[x++] = strdup(line);
		(*lineCount)++;
	}

	// caller should free lineArray memory.
	return lineArray;
}

class gvt_state
{
	public:
		cycles_t cur_time;
		struct gpu_idle_state host;
		struct gpu_idle_state vgpu[GVT_STATE_GPUS];

		// outer uses 2 states to compute JIT delta,
		// use is_current_state to identify.
		bool is_current_state;

		const char* gvt_state_sysfs_path;

	public:
		gvt_state()
		{
			cur_time = 0;
			gvt_state_sysfs_path = "/sys/class/drm/card0/gvt_state";
		}

		bool reset()
		{
			/* write any content to sysfs gvt_state will cause kernel data reset */
			FILE *f = fopen(gvt_state_sysfs_path, "r");
			if (f) {
				fprintf(f, "0");
				update(f);
				fclose(f);
				return true;
			}

			return false;
		}

		void update(FILE *f)
		{
			int i, vgpu_num;
			int global_head_lines = 2;
			int gpu_head_lines = 1;
			int gpu_info_lines = 6;
			int host_head_line = global_head_lines + gpu_head_lines;
			int vm_head_line = host_head_line + gpu_info_lines;
			char **lines = NULL;
			int lineCount = 0;

			lines = getFileStrings(f, &lineCount);
			if (lineCount < 8)
				goto out;
			cur_time = getHexValue(&lines[0][10]);

			// physical GPU information
			for (i = 0; i < GVT_STATE_ENGINES; i++) {
				host.engines[i].submit_count = getHexValue(&lines[host_head_line + i][3]);
				host.engines[i].idle_count = getHexValue(&lines[host_head_line + i][12]);
				host.engines[i].idle_time = getHexValue(&lines[host_head_line + i][21]);
			}

			// virtual GPU information
			vgpu_num = (lineCount - global_head_lines - gpu_info_lines) / gpu_info_lines;
			for (i = 0; i < GVT_STATE_GPUS; i++)
				vgpu[i].valid = false;
			for (i = 0; i < vgpu_num; i++) {
				int line_idx = global_head_lines + (i + 1) * gpu_info_lines;
				char *info = lines[line_idx];
				char *tmp;
				int idx = 0;

				if (!!(tmp = strstr(info, "vgpu")))
					idx = tmp - info;

				if (idx > 0) {
					int vgpu_idx = getIntValue(&info[idx + 5]);

					if (vgpu_idx >= GVT_STATE_GPUS)
						continue;

					line_idx++;
					vgpu[vgpu_idx].valid = true;
					for (int j = 0; j < GVT_STATE_ENGINES; j++) {
						vgpu[vgpu_idx].engines[j].submit_count =
							getHexValue(&lines[line_idx + j][3]);
						vgpu[vgpu_idx].engines[j].idle_count =
							getHexValue(&lines[line_idx + j][12]);
						vgpu[vgpu_idx].engines[j].idle_time =
							getHexValue(&lines[line_idx + j][21]);
					}
				}
			}

out:
			if (lines) {
				// free memory from function getFileStrings
				for (i = 0; i < lineCount; i++)
					free(lines[i]);
				free(lines);
			}
		}

		bool update()
		{
			FILE *f = fopen(gvt_state_sysfs_path, "r");
			if (f) {
				update(f);
				fclose(f);
				return true;
			} else {
				printf("err: failed to open gvt_state. errno is %d\n", errno);
				return false;
			}
		}
};

gvt_state gvt_start;
gvt_state gvt_states[2];

static void print_percent_bar(int engine_id,
		signed int submit_count, signed int submit_total,
		double percent1, double percent2)
{
	int i;

	printf("%5s: BUSY RATE(JIT) %5.1f%%\t", intel_engine_name[engine_id], percent1);
	for (i = percent1; i >= 8; i-=8) {
		printf("%s", bars[8]);
	}
	if (i)
		printf("%s\n", bars[i]);
	else
		printf("\n");


	printf("       BUSY RATE(AVG) %5.1f%%\t", percent2);
	for (i = percent2; i >= 8; i-=8) {
		printf("%s", bars[8]);
	}
	if (i)
		printf("%s\n", bars[i]);
	else
		printf("\n");

	printf("       CMD:%5d/ %d\n", submit_count, submit_total);
}

static void update_gpu_info (struct gpu_idle_state &start,
		struct gpu_idle_state &pre,  struct gpu_idle_state &cur,
		unsigned long long interval, unsigned long long total_time)
{
	int i;
	unsigned int submit_count, submit_count_total;
	double percent_jit, percent_average;

	if (interval == 0 || total_time == 0)
		return;

	for (int engine_id = 0; engine_id < GVT_STATE_ENGINES; engine_id++) {
		unsigned long long idle_total = cur.engines[engine_id].idle_time -
			start.engines[engine_id].idle_time;

		submit_count_total = cur.engines[engine_id].submit_count - start.engines[engine_id].submit_count;
		submit_count = cur.engines[engine_id].submit_count - pre.engines[engine_id].submit_count;

		percent_jit = 100.0;
		if (cur.engines[engine_id].submit_count > pre.engines[engine_id].submit_count) {
			unsigned long long idle_interval = cur.engines[engine_id].idle_time -
				pre.engines[engine_id].idle_time;

			percent_jit = (100.0 * idle_interval) / interval;
			if (percent_jit > 100.0) {
				printf("err: percent_jit is abnormal %f. idle / total = %llx / %llx\n",
						percent_jit, idle_interval, interval);
				percent_jit = 100.0;
			}
		}

		percent_average = (100.0 * idle_total) / total_time;
		if (percent_average > 100.0) {
			printf("err: percent_average is abnormal %f. idle/total = %llx / %llx\n",
					percent_average, idle_total, total_time);
			percent_average = 100.0;
		}

		percent_average = 100.0 - percent_average;
		percent_jit = 100.0 - percent_jit;
		print_percent_bar(engine_id, submit_count, submit_count_total, percent_jit, percent_average);
	}
}

static void update_gvt_state()
{
	int i;
	static int cur_idx = 0;
	static int pre_idx = 0;

	if (gvt_states[0].is_current_state == gvt_states[1].is_current_state)
		gvt_states[0].is_current_state = !gvt_states[1].is_current_state;

	// update gvt_state to get delta data
	for (i = 0; i < 2; i++)
		if (gvt_states[i].is_current_state) {
			pre_idx = i;
			cur_idx = (i + 1) % 2;

			gvt_states[pre_idx].is_current_state = false;
			gvt_states[cur_idx].update();
			gvt_states[cur_idx].is_current_state = true;

			if (gvt_states[pre_idx].cur_time == 0) {
				printf("cur_time = %lld\n", gvt_states[cur_idx].cur_time);
				gvt_states[pre_idx] = gvt_states[cur_idx];
				gvt_states[pre_idx].is_current_state = false;
			}
			break;
		}

	// dump both physcial and virtual GPU information(JIT and average).
	{
		unsigned long long interval = gvt_states[cur_idx].cur_time - gvt_states[pre_idx].cur_time;
		unsigned long long total_time = gvt_states[cur_idx].cur_time - gvt_start.cur_time;

		printf("\n=================pGPU================\n");
		update_gpu_info(gvt_start.host, gvt_states[pre_idx].host, gvt_states[cur_idx].host, interval, total_time);
		for (int i = 0; i < GVT_STATE_GPUS; i++) {
			if (!gvt_states[cur_idx].vgpu[i].valid)
				continue;

			printf("\n=================vGPU-%d=================\n", i);
			update_gpu_info(gvt_start.vgpu[i], gvt_states[pre_idx].vgpu[i], gvt_states[cur_idx].vgpu[i], interval, total_time);
		}
	}
}

static unsigned long gettime(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (t.tv_usec + (t.tv_sec * 1000000));
}

int main (int argc, char* argv[])
{
	unsigned long long t1, t2;
	char clear_screen[] = {0x1b, '[', 'H',
		0x1b, '[', 'J', 0x0};

	t1 = gettime();
	gvt_start.update();

	while (true) {
		printf("%s", clear_screen);

		t2 = gettime();
		printf("Time: %lldS\n", (t2 - t1) / 1000000);

		update_gvt_state();

		/* fixed interval 1s to update.
		 * could be changed to user settable if need. */
		sleep(1);
	}

	printf("end\n");
	return 0;
}
