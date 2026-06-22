/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2026 OpenMV, LLC.
 *
 * REFCLK system counter access for MicroPython (Alif Ensemble).
 *
 * The REFCLK generic counter is a single free-running 64-bit up-counter in the
 * SYSTOP domain, shared by both M55 cores (HP and HE) at fixed global
 * addresses. Register layout / addresses mirror the Alif DFP definitions in
 * lib/alif/Device/common/include (global_map.h, peripheral_types.h); they are
 * redefined here so this user C module does not depend on the DFP include
 * paths being visible to the MicroPython usermod sub-build.
 */
#include "py/runtime.h"
#include "py/obj.h"

// Generic counter module register blocks (global, shared by both cores).
#define REFCLK_CNTCONTROL_BASE  (0x1A200000UL)  // control block
#define REFCLK_CNTREAD_BASE     (0x1A210000UL)  // read-only count block
#define CNTCR_EN                (1U << 0)        // counter enable

// REFCLK runs at 100 MHz on the Ensemble (10 ns/tick) -> 100 ticks per us.
#define REFCLK_HZ               (100000000UL)
#define REFCLK_TICKS_PER_US     (REFCLK_HZ / 1000000UL)   // 100

typedef struct {
    volatile uint32_t CNTCR;    // (0x00) control
    volatile uint32_t CNTSR;    // (0x04) status
    volatile uint32_t CNTCVL;   // (0x08) count, low 32
    volatile uint32_t CNTCVH;   // (0x0C) count, high 32
} refclk_cntcontrol_t;

typedef struct {
    volatile uint32_t CNTCVL;   // (0x00) count, low 32
    volatile uint32_t CNTCVH;   // (0x04) count, high 32
} refclk_cntread_t;

#define REFCLK_CNTControl   ((refclk_cntcontrol_t *) REFCLK_CNTCONTROL_BASE)
#define REFCLK_CNTRead      ((refclk_cntread_t *) REFCLK_CNTREAD_BASE)

// Enable the shared counter. Only needs to be called once on either core; the
// hardware counter and its enable bit are shared. Touches the CNTControl block,
// so that region (0x1A200000) must be MPU-mapped as Device memory.
static mp_obj_t py_refclk_enable(void) {
    REFCLK_CNTControl->CNTCR |= CNTCR_EN;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_refclk_enable_obj, py_refclk_enable);

// Read the full 64-bit counter with a hi-lo-hi atomic snapshot across the two
// 32-bit reads. Only touches the read block (0x1A210000).
static uint64_t refclk_read64(void) {
    uint32_t hi, lo, hi2;
    do {
        hi = REFCLK_CNTRead->CNTCVH;
        lo = REFCLK_CNTRead->CNTCVL;
        hi2 = REFCLK_CNTRead->CNTCVH;
    } while (hi != hi2);
    return ((uint64_t) hi << 32) | lo;
}

// Return the raw 64-bit counter value as a Python int.
static mp_obj_t py_refclk_now(void) {
    return mp_obj_new_int_from_ull(refclk_read64());
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_refclk_now_obj, py_refclk_now);

// Return the counter value converted to microseconds (raw ticks / 100 at
// 100 MHz). Integer division, so sub-us remainder is truncated.
static mp_obj_t py_refclk_now_us(void) {
    return mp_obj_new_int_from_ull(refclk_read64() / REFCLK_TICKS_PER_US);
}
static MP_DEFINE_CONST_FUN_OBJ_0(py_refclk_now_us_obj, py_refclk_now_us);

static const mp_rom_map_elem_t refclk_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_refclk) },
    { MP_ROM_QSTR(MP_QSTR_enable), MP_ROM_PTR(&py_refclk_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_now), MP_ROM_PTR(&py_refclk_now_obj) },
    { MP_ROM_QSTR(MP_QSTR_now_us), MP_ROM_PTR(&py_refclk_now_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_HZ), MP_ROM_INT(REFCLK_HZ) },
};
static MP_DEFINE_CONST_DICT(refclk_module_globals, refclk_module_globals_table);

const mp_obj_module_t refclk_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &refclk_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_refclk, refclk_module);
