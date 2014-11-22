/*
 * e_barectf_tracing.c
 *
 * Copyright (c) 2014 Philippe Proulx <philippe.proulx@efficios.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program, see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "e_lib.h"
#include "barectf.h"

#define PACKET_SIZE	512
#define FIRST_ROW	32
#define FIRST_COL	8
#define WAND_BIT	(1 << 3)

/* shared memory of all 16 cores for copying CTF packets */
static char shared_outbuf[16][PACKET_SIZE] SECTION("shared_dram");

/* this core's CTF packet in local memory */
static char outbuf[PACKET_SIZE];

/**
 * Gets the true row and column numbers of this core.
 *
 * @param row	Where to put the row number
 * @param col	Where to put the column number
 */
static void get_row_col_numbers(unsigned int* row, unsigned int* col)
{
	e_coreid_t coreid = e_get_coreid();

	*row = (coreid >> 6) & 0x3f;
	*col = coreid & 0x3f;
}

/**
 * Returns a pointer to this core's shared memory block for copying
 * its CTF packet. This core owns PACKET_SIZE bytes from this address.
 *
 * @returns	This core's shared memory block address
 */
static char* get_my_shared_buf_part(void)
{
	unsigned int row;
	unsigned int col;

	get_row_col_numbers(&row, &col);

	unsigned int nous = (row - FIRST_ROW) * 4 + (col - FIRST_COL);

	return shared_outbuf[nous];
}

/**
 * WAND interrupt routine. Does nothing, but must exist.
 *
 * @param signum	Not used
 */
static void __attribute__((interrupt)) wand_trace_isr(int signum)
{
	(void) signum;
}

/**
 * Clock callback for barectf. Returns the negative value of CTIMER1,
 * since CTIMERs count downwards and CTF expects an incremental clock
 * value.
 *
 * @param data	Unused user data
 * @returns	Current clock value
 */
static uint32_t get_clock(void* data)
{
	return -e_ctimer_get(E_CTIMER_1);
}

/**
 * Initializes the clock (using CTIMER1).
 */
static void init_clock(void)
{
	e_ctimer_stop(E_CTIMER_1);
	e_ctimer_set(E_CTIMER_1, E_CTIMER_MAX);
}

/**
 * Starts the clock.
 */
static void start_clock(void)
{
	e_ctimer_start(E_CTIMER_1, E_CTIMER_CLK);
}

/**
 * Initialize interrupts.
 */
static void init_interrupts(void)
{
	/* enable interrupts globally */
	e_irq_global_mask(E_FALSE);

	/* enable WAND interrupt */
	e_irq_attach(WAND_BIT, wand_trace_isr);
	e_irq_mask(WAND_BIT, E_FALSE);
}

/**
 * Initializes the barectf context and opens the packet.
 *
 * @param ctx	barectf context
 */
static void init_barectf(struct barectf_ctx* ctx)
{
	barectf_init(ctx, outbuf, PACKET_SIZE, get_clock, NULL);

	unsigned int row;
	unsigned int col;

	get_row_col_numbers(&row, &col);

	/* row and col are CTF stream packet context fields */
	barectf_open_packet(ctx, (uint8_t) row, (uint8_t) col);

	/* first event: no payload */
	barectf_trace_init(ctx);
}

/**
 * Finalizes the barectf context.
 *
 * @param ctx	barectf context
 */
static void fini_barectf(struct barectf_ctx* ctx)
{
	barectf_close_packet(ctx);
}

/**
 * This core's main job.
 *
 * @param ctx	barectf context to use for tracing
 */
static void do_stuff(struct barectf_ctx* ctx)
{
	unsigned int i;
	uint32_t sum = 0;

	for (i = 0; i < 32; ++i) {
		uint32_t j;

		sum += i;

		/*
		 * This busy-wait loop simulates some job that could be done
		 * by this core. Its duration is directly dependent on this
		 * core's ID, so that all cores should have different delays.
		 */
		for (j = 0; j < e_get_coreid(); ++j);

		/* trace the current sum */
		barectf_trace_epiphanious(ctx, sum);
	}
}

/**
 * Copies this core's local CTF packet to its shared buffer block.
 */
static void copy_local_to_shared(void)
{
	memcpy(get_my_shared_buf_part(), outbuf, PACKET_SIZE);
}

int main(void) {
	/* initialize clock and interrupts */
	init_clock();
	init_interrupts();

	/* initial barrier to synchronize all 16 cores */
	__asm__ __volatile__("wand");
	__asm__ __volatile__("idle");

	/* clear WAND interrupt status bit */
	unsigned int irq_state = e_reg_read(E_REG_STATUS);

	irq_state &= ~WAND_BIT;
	e_reg_write(E_REG_STATUS, irq_state);

	/* start clock now */
	start_clock();

	/* initialize barectf context */
	struct barectf_ctx barectf_ctx;

	init_barectf(&barectf_ctx);

	/* compute stuff */
	do_stuff(&barectf_ctx);

	/* finalize barectf context */
	fini_barectf(&barectf_ctx);

	/* CTF packet is now complete and ready: copy to shared memory */
	copy_local_to_shared();

	__asm__ __volatile__("idle");

	return EXIT_SUCCESS;
}
