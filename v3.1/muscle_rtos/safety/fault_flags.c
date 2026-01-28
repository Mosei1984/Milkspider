/**
 * Fault Flags Implementation
 */

#include "fault_flags.h"
#include "FreeRTOS.h"
#include "task.h"

static volatile uint32_t s_fault_flags = 0;

void fault_flags_init(void) {
    s_fault_flags = 0;
}

void fault_flags_set(FaultFlag flag) {
    taskENTER_CRITICAL();
    s_fault_flags |= (uint32_t)flag;
    taskEXIT_CRITICAL();
}

void fault_flags_clear(FaultFlag flag) {
    taskENTER_CRITICAL();
    s_fault_flags &= ~(uint32_t)flag;
    taskEXIT_CRITICAL();
}

bool fault_flags_is_set(FaultFlag flag) {
    return (s_fault_flags & (uint32_t)flag) != 0;
}

uint32_t fault_flags_get_all(void) {
    return s_fault_flags;
}

void fault_flags_clear_all(void) {
    taskENTER_CRITICAL();
    s_fault_flags = 0;
    taskEXIT_CRITICAL();
}
