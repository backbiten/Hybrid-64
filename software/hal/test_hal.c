/**
 * test_hal.c — Unit tests for the Hybrid-64 HAL
 *
 * A self-contained test runner (no external framework required).
 * Exit code 0 = all pass; non-zero = at least one failure.
 */
#include "hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Mini test framework ──────────────────────────────────────────────────── */
static int g_pass = 0, g_fail = 0;

#define EXPECT(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d  " #expr "\n", __FILE__, __LINE__); \
        g_fail++; \
    } else { \
        printf("PASS %s:%d  " #expr "\n", __FILE__, __LINE__); \
        g_pass++; \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    uint64_t _a = (uint64_t)(a), _b = (uint64_t)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d  " #a " == " #b " (got 0x%llx, expected 0x%llx)\n", \
                __FILE__, __LINE__, \
                (unsigned long long)_a, (unsigned long long)_b); \
        g_fail++; \
    } else { \
        printf("PASS %s:%d  " #a " == " #b "\n", __FILE__, __LINE__); \
        g_pass++; \
    } \
} while (0)

/* ── Helper: load a program and run ──────────────────────────────────────────*/
static hal_status_t run_prog(hal_device_t *dev,
                              const uint32_t *prog, size_t n,
                              uint64_t max_cyc)
{
    hal_reset(dev);
    for (size_t i = 0; i < n; i++)
        hal_mem_write_instr(dev, (uint64_t)(i * H64_INSTR_SIZE), prog[i]);
    return hal_run(dev, max_cyc);
}

/* ── Test: lifecycle ──────────────────────────────────────────────────────── */
static void test_lifecycle(void)
{
    printf("\n── Lifecycle ──────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;

    EXPECT(hal_open(HAL_BACKEND_SOFTWARE, 0, &dev) == HAL_OK);
    EXPECT(dev != NULL);
    EXPECT(hal_reset(dev) == HAL_OK);

    hal_cpu_status_t s;
    EXPECT(hal_get_status(dev, &s) == HAL_OK);
    EXPECT(s.pc == 0);
    EXPECT(!s.halted);

    hal_close(dev);
}

/* ── Test: null/invalid args ─────────────────────────────────────────────── */
static void test_null_args(void)
{
    printf("\n── Null / invalid args ─────────────────────────────────────────\n");
    EXPECT(hal_open(HAL_BACKEND_SOFTWARE, 0, NULL) == HAL_ERR_NULL);
    EXPECT(hal_reset(NULL) == HAL_ERR_NULL);
    EXPECT(hal_step(NULL)  == HAL_ERR_NULL);
    EXPECT(hal_run(NULL, 0) == HAL_ERR_NULL);

    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);
    EXPECT(hal_reg_read(dev,  99, NULL) == HAL_ERR_NULL);
    EXPECT(hal_reg_read(dev,  99, &(uint64_t){0}) == HAL_ERR_RANGE);
    EXPECT(hal_reg_write(dev, 99, 0) == HAL_ERR_RANGE);
    hal_close(dev);
}

/* ── Test: ADDI + HALT ──────────────────────────────────────────────────── */
static void test_addi_halt(void)
{
    printf("\n── ADDI + HALT ────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1, 42),
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 1000);

    uint64_t v = 0;
    hal_reg_read(dev, 1, &v);
    EXPECT_EQ(v, 42);

    hal_cpu_status_t s;
    hal_get_status(dev, &s);
    EXPECT(s.halted);
    hal_close(dev);
}

/* ── Test: ADD / SUB ────────────────────────────────────────────────────── */
static void test_add_sub(void)
{
    printf("\n── ADD / SUB ──────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1, 100),
        hal_mk_i(H64_OP_ADDI, 0, 2, 55),
        hal_mk_r(H64_OP_ADD, 1, 2, 3),
        hal_mk_r(H64_OP_SUB, 1, 2, 4),
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 1000);

    uint64_t r3, r4;
    hal_reg_read(dev, 3, &r3);
    hal_reg_read(dev, 4, &r4);
    EXPECT_EQ(r3, 155);
    EXPECT_EQ(r4, 45);
    hal_close(dev);
}

/* ── Test: AND / OR / XOR ────────────────────────────────────────────────── */
static void test_bitwise(void)
{
    printf("\n── Bitwise ────────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1, 0xFF),
        hal_mk_i(H64_OP_ADDI, 0, 2, 0x0F),
        hal_mk_r(H64_OP_AND,  1, 2, 3),   /* r3 = 0x0F */
        hal_mk_r(H64_OP_OR,   1, 2, 4),   /* r4 = 0xFF */
        hal_mk_r(H64_OP_XOR,  1, 1, 5),   /* r5 = 0    */
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 1000);

    uint64_t r3, r4, r5;
    hal_reg_read(dev, 3, &r3);
    hal_reg_read(dev, 4, &r4);
    hal_reg_read(dev, 5, &r5);
    EXPECT_EQ(r3, 0x0F);
    EXPECT_EQ(r4, 0xFF);
    EXPECT_EQ(r5, 0);
    hal_close(dev);
}

/* ── Test: LD / ST ────────────────────────────────────────────────────────── */
static void test_load_store(void)
{
    printf("\n── LD / ST ────────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    /* Store 0xDEAD to mem[0x100], load it back */
    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1, 0x100),   /* r1 = 0x100 */
        hal_mk_i(H64_OP_ADDI, 0, 2, 0x0EAD),  /* r2 = value (truncated to 11b) */
        hal_mk_rs(H64_OP_ST,  1, 2, 0),        /* mem[r1+0] = r2               */
        hal_mk_i(H64_OP_LD,   1, 3, 0),        /* r3 = mem[r1+0]               */
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 1000);

    uint64_t r2, r3;
    hal_reg_read(dev, 2, &r2);
    hal_reg_read(dev, 3, &r3);
    EXPECT_EQ(r2, r3);  /* loaded value matches stored */
    hal_close(dev);
}

/* ── Test: BNE loop (sum 1..10) ─────────────────────────────────────────── */
static void test_bne_loop(void)
{
    printf("\n── BNE loop (sum 1..10) ────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 1,  0),    /* r1=0 acc   */
        hal_mk_i(H64_OP_ADDI, 0, 2,  0),    /* r2=0 i     */
        hal_mk_i(H64_OP_ADDI, 0, 3, 10),    /* r3=10 limit*/
        hal_mk_i(H64_OP_ADDI, 0, 4,  1),    /* r4=1 step  */
        /* loop: */
        hal_mk_r(H64_OP_ADD, 2, 4, 2),      /* r2 += 1    */
        hal_mk_r(H64_OP_ADD, 1, 2, 1),      /* r1 += r2   */
        hal_mk_rs(H64_OP_BNE, 2, 3, (int16_t)-2 & 0x7FF),
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 100000);

    uint64_t acc;
    hal_reg_read(dev, 1, &acc);
    EXPECT_EQ(acc, 55);
    hal_close(dev);
}

/* ── Test: register 0 is read-only ─────────────────────────────────────── */
static void test_r0_readonly(void)
{
    printf("\n── r0 read-only ────────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    const uint32_t prog[] = {
        hal_mk_i(H64_OP_ADDI, 0, 0, 99),   /* try to write r0 */
        HAL_INSTR_HALT,
    };
    run_prog(dev, prog, sizeof(prog)/sizeof(prog[0]), 1000);

    uint64_t r0;
    hal_reg_read(dev, 0, &r0);
    EXPECT_EQ(r0, 0);
    hal_close(dev);
}

/* ── Test: mem_read64 / mem_write64 alignment ────────────────────────────── */
static void test_mem_alignment(void)
{
    printf("\n── Memory alignment ────────────────────────────────────────────\n");
    hal_device_t *dev = NULL;
    hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

    EXPECT(hal_mem_write64(dev, 0x100, 0xCAFEBABEDEADBEEF) == HAL_OK);
    uint64_t v = 0;
    EXPECT(hal_mem_read64(dev, 0x100, &v) == HAL_OK);
    EXPECT_EQ(v, 0xCAFEBABEDEADBEEF);

    /* Misaligned should fail */
    EXPECT(hal_mem_write64(dev, 0x101, 0) == HAL_ERR_ALIGN);
    EXPECT(hal_mem_read64(dev,  0x103, &v) == HAL_ERR_ALIGN);

    hal_close(dev);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void)
{
    printf("Hybrid-64 HAL Unit Tests\n");
    printf("========================\n");

    test_lifecycle();
    test_null_args();
    test_addi_halt();
    test_add_sub();
    test_bitwise();
    test_load_store();
    test_bne_loop();
    test_r0_readonly();
    test_mem_alignment();

    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0)
        printf("ALL TESTS PASSED\n");
    else
        printf("SOME TESTS FAILED\n");

    return (g_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
