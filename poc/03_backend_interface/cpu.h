/*
 * cpu.h - abstract CPU backend vtable.
 *
 * Every recompiler/interpreter behind this interface is interchangeable.
 * Nothing outside the backend implementations knows or cares which
 * backend is active. Ballistic plugs in by supplying one CPU_Backend
 * instance - exactly the same shape as the dynarmic and noop backends.
 *
 * This is the production interface (lifted verbatim from DESIGN.MD §5)
 * not a POC sketch. It is what dynarmic_backend.c implements today and
 * what ballistic_stub.c will implement once Ballistic exposes the
 * needed surface.
 */

#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CPU_State CPU_State; /* opaque, backend-defined */
    typedef struct CPU_Backend CPU_Backend;

    /*  Callbacks fired by the backend into the host  */

    /* SVC immediate is `swi`; the syscall number itself lives in X8 per the
     * Switch ABI. Userdata is whatever was passed to create(). */
    typedef void (*CPU_SVC_Handler)(CPU_State *state,
                                    uint32_t swi,
                                    void *userdata);

    typedef void (*CPU_Undefined_Handler)(CPU_State *state,
                                          uint32_t instruction,
                                          void *userdata);

    typedef void (*CPU_Breakpoint_Handler)(CPU_State *state,
                                           uint64_t address,
                                           void *userdata);

    /*  The vtable  */

    struct CPU_Backend
    {
        /* lifecycle */
        CPU_State *(*create)(void *guest_ram,
                             uint64_t ram_size,
                             void *userdata);
        void (*destroy)(CPU_State *state);

        /* execution */
        void (*run)(CPU_State *state, uint64_t entry_point);
        void (*step)(CPU_State *state);
        void (*run_until)(CPU_State *state, uint64_t address);

        /* general purpose registers X0-X30 */
        uint64_t (*get_reg)(CPU_State *state, uint8_t reg_index);
        void (*set_reg)(CPU_State *state, uint8_t reg_index, uint64_t value);

        /* special registers */
        uint64_t (*get_pc)(CPU_State *state);
        void (*set_pc)(CPU_State *state, uint64_t value);
        uint64_t (*get_sp)(CPU_State *state);
        void (*set_sp)(CPU_State *state, uint64_t value);
        uint32_t (*get_pstate)(CPU_State *state);
        void (*set_pstate)(CPU_State *state, uint32_t value);

        /* MRS / MSR */
        uint64_t (*get_sys_reg)(CPU_State *state, uint32_t encoded_reg);
        void (*set_sys_reg)(CPU_State *state, uint32_t encoded_reg, uint64_t value);

        /* execution control */
        void (*halt)(CPU_State *state);
        bool (*is_halted)(CPU_State *state);

        /* code cache management - call after writing to guest RAM
         * (self-modifying code, JIT patching, page remap) */
        void (*invalidate_cache)(CPU_State *state, uint64_t guest_address, uint64_t size_bytes);
        void (*clear_cache)(CPU_State *state);

        /* callback registration */
        void (*set_svc_handler)(CPU_State *state, CPU_SVC_Handler h);
        void (*set_undefined_handler)(CPU_State *state, CPU_Undefined_Handler h);
        void (*set_breakpoint_handler)(CPU_State *state, CPU_Breakpoint_Handler h);

        /* metadata */
        const char *name; /* "noop" | "interpreter" | "dynarmic" | "ballistic" */
        const char *version;
        bool supports_jit;
        bool supports_wasm; /* true only for ballistic when WASM target enabled */
    };

/* Register index reference - Switch ABI / ARM AAPCS64 */
#define CPU_REG_X0 0
#define CPU_REG_X8 8   /* syscall number */
#define CPU_REG_X29 29 /* frame pointer */
#define CPU_REG_X30 30 /* link register  */
#define CPU_REG_XZR 31 /* zero register  */

    /* The two backends in this POC. */
    extern const CPU_Backend CPU_BACKEND_DYNARMIC;
    extern const CPU_Backend CPU_BACKEND_BALLISTIC;

#ifdef __cplusplus
}
#endif

#endif /* CPU_H */
