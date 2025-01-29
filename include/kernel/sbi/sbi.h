#pragma once

#define SBI_EXT_BASE 0x10
enum : uint32_t {
    SBI_BASE_FN_GET_SPEC_VERSION,
    SBI_BASE_FN_GET_IMPL_ID,
    SBI_BASE_FN_GET_IMPL_VERSION,
    SBI_BASE_FN_PROBE_EXTENSION,
    SBI_BASE_FN_GET_MVENDORID,
    SBI_BASE_FN_GET_MARCHID,
    SBI_BASE_FN_GET_MIMPID,
};

#define SBI_EXT_DBCN 0x4442434e
#define SBI_EXT_TIME 0x54494d45

#define SBI_EXT_HSM  0x48534d
enum : uint32_t {
    SBI_HSM_FN_HART_START,
    SBI_HSM_FN_HART_STOP,
    SBI_HSM_FN_HART_GET_STATUS,
    SBI_HSM_FN_HART_SUSPEND
};
enum SBI_HSM_STATE : uint32_t {
    SBI_HSM_STATE_STARTED,
    SBI_HSM_STATE_STOPPED,
    SBI_HSM_STATE_START_PENDING,
    SBI_HSM_STATE_STOP_PENDING,
    SBI_HSM_STATE_SUSPENDED,
    SBI_HSM_STATE_SUSPEND_PENDING,
    SBI_HSM_STATE_RESUME_PENDING,

    SBI_HSM_STATE_ERROR = -1
};

#define SBI_EXT_SRST 0x53525354
enum : uint32_t {
    SBI_SRST_TYPE_SHUTDOWN,
    SBI_SRST_TYPE_COLD_REBOOT,
    SBI_SRST_TYPE_WARM_REBOOT
};
enum : uint32_t {
    SBI_SRST_REASON_NONE,
    SBI_SRST_REASON_SYSTEM_FAILURE
};

typedef struct sbiret {
    long error;
    long value;
} sbiret;

sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid);

void probe_sbi_extension(long eid, const char *name);
enum SBI_HSM_STATE hart_get_status(long hartid);