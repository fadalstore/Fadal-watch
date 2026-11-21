#include "fsh_probe_abi.h"

.section .fsh_probe,"ax",@progbits

/* The relative call/pop sequence is position-independent after FSH is loaded. */
.global fsh_ring3_write_probe
.type fsh_ring3_write_probe, @function
fsh_ring3_write_probe:
    .byte 0xe8
    .long 0
    popl %eax
    subl $5, %eax

.org FSH_PROBE_STORE_OFFSET
.global fsh_ring3_write_probe_store
fsh_ring3_write_probe_store:
    movb $FSH_PROBE_ATTEMPT_BYTE, (%eax)
    ud2

.org FSH_PROBE_RESUME_OFFSET
.global fsh_ring3_write_probe_resume
fsh_ring3_write_probe_resume:
    ret
.size fsh_ring3_write_probe, .-fsh_ring3_write_probe

.section .note.GNU-stack,"",@progbits
