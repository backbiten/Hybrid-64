/**
 * cpu_driver.h — Hybrid-64 CPU driver interface
 */
#ifndef HYBRID64_CPU_DRIVER_H
#define HYBRID64_CPU_DRIVER_H

#include "../hal/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cpu_load_program() — Reset the CPU and write an instruction sequence into
 * memory starting at address 0.
 *
 * @param dev    HAL device handle.
 * @param instrs Array of 32-bit instruction words.
 * @param count  Number of instructions.
 * @return HAL_OK on success.
 */
hal_status_t cpu_load_program(hal_device_t *dev,
                              const uint32_t *instrs, size_t count);

/**
 * cpu_dump_regs() — Print all registers and CPU flags to stdout.
 * @param dev HAL device handle.
 */
void cpu_dump_regs(const hal_device_t *dev);

/**
 * cpu_dump_mem() — Hexdump a region of device memory to stdout.
 *
 * @param dev  HAL device handle.
 * @param addr Start address.
 * @param len  Number of bytes.
 */
void cpu_dump_mem(const hal_device_t *dev, uint64_t addr, size_t len);

/**
 * cpu_run_report() — Run the CPU, then print a brief execution summary.
 *
 * @param dev        HAL device handle.
 * @param max_cycles Maximum cycles to run (0 = unlimited).
 * @return HAL_OK on success.
 */
hal_status_t cpu_run_report(hal_device_t *dev, uint64_t max_cycles);

#ifdef __cplusplus
}
#endif

#endif /* HYBRID64_CPU_DRIVER_H */
