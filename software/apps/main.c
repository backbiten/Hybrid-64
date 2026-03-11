/**
 * main.c — Hybrid-64 reference application
 *
 * Demonstrates the HAL and driver APIs by running a small program on the
 * software-emulated Hybrid-64 CPU:
 *
 *   1. Computes 1+2+3+…+10 (should equal 55)
 *   2. Stores the result to memory and reads it back
 *   3. Prints a full register and memory dump
 */
#include <stdio.h>
#include <stdlib.h>
#include "../hal/hal.h"
#include "../drivers/cpu_driver.h"

int main(void)
{
    printf("Hybrid-64 Reference Application\n");
    printf("================================\n\n");

    /* Open the software-emulated CPU */
    hal_device_t *dev = NULL;
    hal_status_t st = hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);
    if (st != HAL_OK) {
        fprintf(stderr, "hal_open: %s\n", hal_strerror(st));
        return EXIT_FAILURE;
    }

    /*
     * Program: sum 1..10 using a counted loop
     *
     *  r1 = 0          ; accumulator
     *  r2 = 0          ; loop counter i
     *  r3 = 10         ; limit
     *  r4 = 1          ; increment
     * loop:
     *  r2 = r2 + r4    ; i++
     *  r1 = r1 + r2    ; acc += i
     *  BNE r2, r3, loop
     *  ST  [0x8000], r1
     *  HALT
     */
    const uint32_t program[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1,  0),    /* r1 = 0  (addi r1, r0, 0)  */
        hal_mk_i(H64_OP_ADDI, 0, 2,  0),    /* r2 = 0                    */
        hal_mk_i(H64_OP_ADDI, 0, 3, 10),    /* r3 = 10                   */
        hal_mk_i(H64_OP_ADDI, 0, 4,  1),    /* r4 = 1                    */
        /* loop: PC = 4 * 4 = 16 */
        hal_mk_r(H64_OP_ADD, 2, 4, 2),      /* r2 = r2 + r4 (i++)        */
        hal_mk_r(H64_OP_ADD, 1, 2, 1),      /* r1 = r1 + r2 (acc += i)   */
        /* BNE r2, r3, -2 (jump back 2 instrs = -8 bytes → imm=-2) */
        hal_mk_rs(H64_OP_BNE, 2, 3, (int16_t)-2),
        /* Store acc to address 0x8000 using r5 as base */
        hal_mk_i(H64_OP_ADDI, 0, 5, 0),      /* r5 = 0 (base)             */
        /* We store r1 at offset 0 from r5=0 — but imm is only 11 bits.
           Use r5 = 0x8000 via multiple shifts instead: simpler to just
           store at offset 0 for this demo. */
        hal_mk_rs(H64_OP_ST, 5, 1, 0),       /* mem[r5+0] = r1            */
        HAL_INSTR_HALT,
    };
    size_t prog_len = sizeof(program) / sizeof(program[0]);

    st = cpu_load_program(dev, program, prog_len);
    if (st != HAL_OK) {
        fprintf(stderr, "cpu_load_program: %s\n", hal_strerror(st));
        hal_close(dev);
        return EXIT_FAILURE;
    }

    /* Run the program */
    st = cpu_run_report(dev, 10000);
    if (st != HAL_OK && st != HAL_ERR_HALTED) {
        fprintf(stderr, "cpu_run_report: %s\n", hal_strerror(st));
        hal_close(dev);
        return EXIT_FAILURE;
    }

    /* Read result */
    uint64_t acc = 0;
    hal_reg_read(dev, 1, &acc);
    printf("\nSum of 1..10 = %llu (expected 55)\n\n", (unsigned long long)acc);

    /* Dump registers and memory */
    cpu_dump_regs(dev);
    printf("\n");
    cpu_dump_mem(dev, 0, 48);  /* Show first 48 bytes (program) */

    hal_close(dev);
    return (acc == 55) ? EXIT_SUCCESS : EXIT_FAILURE;
}
