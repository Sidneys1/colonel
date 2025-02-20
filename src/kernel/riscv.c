#include <kernel.h>
#include <stddef.h>

#define SSTATUS_SIE (0x1 << 1)

// static inline uint64_t r_sstatus() {
//   uint64_t x;
//   __asm__ __volatile__("csrr %0, sstatus" : "=r" (x) );
//   return x;
// }

// static inline void w_sstatus(uint64_t x) {
//   __asm__ __volatile__("csrw sstatus, %0" : : "r" (x));
// }

int intr_get() {
  uint32_t sstatus = READ_CSR(sstatus);
  return (sstatus & SSTATUS_SIE) != 0;
}

// disable device interrupts
void intr_off() {
  uint32_t sstatus = READ_CSR(sstatus);
  WRITE_CSR(sstatus, sstatus & ~SSTATUS_SIE);
  // w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// enable device interrupts
void intr_on() {
  uint32_t sstatus= READ_CSR(sstatus);
  WRITE_CSR(sstatus, sstatus | SSTATUS_SIE);
  // w_sstatus(r_sstatus() | SSTATUS_SIE);
}
