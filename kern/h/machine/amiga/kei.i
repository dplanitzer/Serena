;
;  machine/kei.i
;  kpi
;
;  Created by Dietmar Planitzer on 1/4/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

        ifnd KEI_I
KEI_I    set 1

ATOMIC_INT_EXCHANGE                 equ $a000   ; int atomic_int_exchange(volatile atomic_int* _Nonnull p{a0}, int val{d1})
ATOMIC_INT_COMPARE_EXCHANGE_STRONG  equ $a001   ; bool atomic_int_compare_exchange_strong(volatile atomic_int* _Nonnull p{a0}, volatile atomic_int* _Nonnull expected{a1}, int desired{d1})
ATOMIC_INT_FETCH_ADD                equ $a002   ; int atomic_int_fetch_add(volatile atomic_int* _Nonnull p{a0}, int op{d1})
ATOMIC_INT_FETCH_SUB                equ $a003   ; int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p{a0}, int op{d1})
ATOMIC_INT_FETCH_OR                 equ $a004   ; int atomic_int_fetch_or(volatile atomic_int* _Nonnull p{a0}, int op{d1})
ATOMIC_INT_FETCH_XOR                equ $a005   ; int atomic_int_fetch_xor(volatile atomic_int* _Nonnull p{a0}, int op{d1})
ATOMIC_INT_FETCH_AND                equ $a006   ; int atomic_int_fetch_and(volatile atomic_int* _Nonnull p{a0}, int op{d1})
RC_RELEASE                          equ $a007   ; bool rc_release(volatile ref_count_t* _Nonnull rc{a0})

ATOMIC_INSTR_FIRST                  equ 0
ATOMIC_INSTR_LAST                   equ RC_RELEASE - $a000

        endif
