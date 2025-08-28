#include "mem_mon.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <stdio.h>

static void print_task_stack_watermarks() {
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t* tasks = (TaskStatus_t*)pvPortMalloc(sizeof(TaskStatus_t) * task_count);
    if (!tasks) {
        printf("Failed to allocate task array\n");
        return;
    }
    UBaseType_t written = uxTaskGetSystemState(tasks, task_count, NULL);
    for (UBaseType_t i = 0; i < written; ++i) {
        const TaskStatus_t& ts = tasks[i];
        const size_t stack_free_words = ts.usStackHighWaterMark;
        const size_t stack_free_bytes = stack_free_words * sizeof(StackType_t);
        printf("Task %s (PID %u): free stack bytes = %u\n", ts.pcTaskName, (unsigned)ts.xTaskNumber,
               (unsigned)stack_free_bytes);
    }
    vPortFree(tasks);
}

void heap_monitor_task(void* /*pv*/) {
    for (;;) {
        size_t free_total = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t min_free_total = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        printf("Heap: free=%u bytes, minEver=%u bytes\n", (unsigned)free_total,
               (unsigned)min_free_total);
        printf("Heap by region: internal=%u, spiram=%u\n", (unsigned)free_internal,
               (unsigned)free_spiram);
        print_task_stack_watermarks();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
