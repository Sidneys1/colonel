#include <common.h>
#include <kernel.h>
#include <sbi/sbi.h>
#include <string.h>

sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                         : "memory");
    return (sbiret){.error = a0, .value = a1};
}

void probe_sbi_extension(long eid, const char *name) {
    struct sbiret value = sbi_call(eid, 0, 0, 0, 0, 0, SBI_BASE_FN_PROBE_EXTENSION, SBI_EXT_BASE);
    kprintf("probe_extension[0x%x]: value=%s0x%x %s\033[0m\terror=%d\t(%s Extension)\n", eid,
            value.value ? CSTR("\033[32m") : CSTR("\033[31m"), value.value,
            value.value ? CSTR("(available)    ") : CSTR("(not available)"), value.error, name);
}

enum SBI_HSM_STATE hart_get_status(long hartid) {
    sbiret value = sbi_call(hartid, 0, 0, 0, 0, 0, SBI_HSM_FN_HART_GET_STATUS, SBI_EXT_HSM);
    if (value.error == -3)
        return SBI_HSM_STATE_ERROR;
    // kprintf("hart_get_status[%d] (Hart State Management Extension): value=0x%x %s\terror=%d %s\n",
    //     hartid,
    //     value.value,
    //     value.value == SBI_HSM_STATE_STARTED
    //         ? "(started)        "
    //         : value.value == SBI_HSM_STATE_STOPPED ? "(stopped)        "
    //         : value.value == SBI_HSM_STATE_START_PENDING ? "(start pending)  "
    //         : value.value == SBI_HSM_STATE_STOP_PENDING ? "(stop pending)   "
    //         : value.value == SBI_HSM_STATE_SUSPENDED ? "(suspended)      "
    //         : value.value == SBI_HSM_STATE_SUSPEND_PENDING ? "(suspend pending)"
    //         : value.value == SBI_HSM_STATE_RESUME_PENDING ? "(resume pending) "
    //         :                    "(unknown)        ",
    //     value.error,
    //     value.error ==  0 ? "(no error)" : "(unknown)");
    return (enum SBI_HSM_STATE)value.value;
}
