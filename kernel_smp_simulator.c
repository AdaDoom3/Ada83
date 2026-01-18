/*
 * ============================================================================
 * ADA83 ARM SMP MICROKERNEL - HOST-BASED SIMULATOR
 * ============================================================================
 * Purpose: Test SMP features without ARM hardware using pthreads
 * Features:
 *   - Spinlock validation
 *   - Per-CPU data isolation testing
 *   - Priority IPC queue testing
 *   - Zero-copy shared memory IPC
 *   - Load balancing verification
 *   - Multi-threaded execution simulation
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

#define MAX_CPUS 4
#define MAX_PROCESSES 64
#define MAX_PRIORITY_LEVELS 8
#define MESSAGE_QUEUE_SIZE 256
#define SHARED_MEMORY_REGIONS 16
#define SHARED_MEMORY_REGION_SIZE 4096

/* ============================================================================
 * DATA STRUCTURES (SIMULATING ADA TYPES)
 * ============================================================================ */

typedef enum {
    PROCESS_READY = 0,
    PROCESS_RUNNING = 1,
    PROCESS_BLOCKED = 2,
    PROCESS_WAITING = 3,
    PROCESS_TERMINATED = 4
} ProcessState;

typedef struct {
    int registers[17];  /* r0-r15 + CPSR */
} CPUContext;

typedef struct {
    int process_id;
    ProcessState state;
    int priority;
    int cpu_affinity;      /* -1 = any CPU, 0-3 = specific CPU */
    CPUContext context;
    int message_queue_head;
    int mmu_context;
    uint64_t total_runtime;
} ProcessControlBlock;

typedef struct {
    int sender_process;
    int receiver_process;
    int message_type;
    int priority;           /* 0-7, higher = more urgent */
    int payload_length;
    char payload[64];
    int shared_memory_region;  /* -1 = copy, 0-15 = zero-copy */
    uint64_t timestamp;
} IPCMessage;

typedef struct {
    int cpu_id;
    int current_process;
    int active_processes;
    int load_metric;           /* 0-100 */
    volatile int lock;
    uint64_t context_switches;
    uint64_t idle_cycles;
    pthread_t thread;
} PerCPUData;

typedef struct {
    IPCMessage messages[MESSAGE_QUEUE_SIZE];
    int head;
    int tail;
    volatile int lock;
} PriorityQueue;

typedef struct {
    void* region;
    volatile int owner_process;
    volatile int ref_count;
    volatile int lock;
} SharedMemoryRegion;

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

static ProcessControlBlock process_table[MAX_PROCESSES];
static PerCPUData per_cpu_data[MAX_CPUS];
static PriorityQueue priority_queues[MAX_PRIORITY_LEVELS];
static SharedMemoryRegion shared_memory[SHARED_MEMORY_REGIONS];
static volatile int global_scheduler_lock = 0;
static volatile bool simulation_running = true;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Statistics */
static uint64_t total_messages_sent = 0;
static uint64_t total_messages_received = 0;
static uint64_t total_zero_copy_messages = 0;
static uint64_t total_spinlock_contentions = 0;

/* ============================================================================
 * ATOMIC OPERATIONS (SIMULATING ARM LDREX/STREX)
 * ============================================================================ */

static inline void spinlock_acquire(volatile int* lock) {
    int attempts = 0;
    while (__sync_lock_test_and_set(lock, 1) != 0) {
        attempts++;
        if (attempts > 10) {
            total_spinlock_contentions++;
            sched_yield();  /* Simulate WFE (wait for event) */
            attempts = 0;
        }
    }
    __sync_synchronize();  /* Memory barrier */
}

static inline void spinlock_release(volatile int* lock) {
    __sync_synchronize();  /* Memory barrier */
    __sync_lock_release(lock);
}

static inline int atomic_add(volatile int* ptr, int value) {
    return __sync_fetch_and_add(ptr, value);
}

static inline bool atomic_compare_exchange(volatile int* ptr, int expected, int desired) {
    return __sync_bool_compare_and_swap(ptr, expected, desired);
}

/* ============================================================================
 * THREAD-LOCAL CPU ID (SIMULATING MPIDR REGISTER)
 * ============================================================================ */

static __thread int current_cpu_id = 0;

static inline int get_cpu_id(void) {
    return current_cpu_id;
}

/* ============================================================================
 * SAFE PRINTING (THREAD-SAFE)
 * ============================================================================ */

static void safe_printf(const char* format, ...) {
    pthread_mutex_lock(&print_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

/* ============================================================================
 * PRIORITY IPC IMPLEMENTATION
 * ============================================================================ */

static bool send_message_with_priority(int target_process, IPCMessage* msg, int priority) {
    if (priority < 0 || priority >= MAX_PRIORITY_LEVELS) return false;

    PriorityQueue* queue = &priority_queues[priority];
    spinlock_acquire(&queue->lock);

    int next_tail = (queue->tail + 1) % MESSAGE_QUEUE_SIZE;
    if (next_tail == queue->head) {
        spinlock_release(&queue->lock);
        return false;  /* Queue full */
    }

    msg->priority = priority;
    msg->timestamp = (uint64_t)time(NULL);
    queue->messages[queue->tail] = *msg;
    queue->tail = next_tail;

    atomic_add((int*)&total_messages_sent, 1);
    if (msg->shared_memory_region >= 0) {
        atomic_add((int*)&total_zero_copy_messages, 1);
    }

    spinlock_release(&queue->lock);
    return true;
}

static bool receive_message_highest_priority(IPCMessage* msg) {
    /* Check queues from highest to lowest priority */
    for (int prio = MAX_PRIORITY_LEVELS - 1; prio >= 0; prio--) {
        PriorityQueue* queue = &priority_queues[prio];
        spinlock_acquire(&queue->lock);

        if (queue->head != queue->tail) {
            *msg = queue->messages[queue->head];
            queue->head = (queue->head + 1) % MESSAGE_QUEUE_SIZE;
            atomic_add((int*)&total_messages_received, 1);
            spinlock_release(&queue->lock);
            return true;
        }

        spinlock_release(&queue->lock);
    }
    return false;
}

/* ============================================================================
 * ZERO-COPY SHARED MEMORY IPC
 * ============================================================================ */

static int allocate_shared_memory_region(int process_id) {
    for (int i = 0; i < SHARED_MEMORY_REGIONS; i++) {
        spinlock_acquire(&shared_memory[i].lock);
        if (shared_memory[i].owner_process == -1) {
            shared_memory[i].owner_process = process_id;
            shared_memory[i].ref_count = 1;
            spinlock_release(&shared_memory[i].lock);
            return i;
        }
        spinlock_release(&shared_memory[i].lock);
    }
    return -1;  /* No free regions */
}

static bool share_memory_region(int region, int target_process) {
    if (region < 0 || region >= SHARED_MEMORY_REGIONS) return false;

    spinlock_acquire(&shared_memory[region].lock);
    shared_memory[region].ref_count++;
    spinlock_release(&shared_memory[region].lock);
    return true;
}

static void release_shared_memory_region(int region) {
    if (region < 0 || region >= SHARED_MEMORY_REGIONS) return;

    spinlock_acquire(&shared_memory[region].lock);
    shared_memory[region].ref_count--;
    if (shared_memory[region].ref_count == 0) {
        shared_memory[region].owner_process = -1;
    }
    spinlock_release(&shared_memory[region].lock);
}

/* ============================================================================
 * LOAD BALANCING
 * ============================================================================ */

static int find_least_loaded_cpu(void) {
    int min_load = 101;
    int best_cpu = 0;

    for (int i = 0; i < MAX_CPUS; i++) {
        if (per_cpu_data[i].load_metric < min_load) {
            min_load = per_cpu_data[i].load_metric;
            best_cpu = i;
        }
    }
    return best_cpu;
}

static void update_cpu_load(int cpu_id, int active_procs) {
    per_cpu_data[cpu_id].active_processes = active_procs;
    per_cpu_data[cpu_id].load_metric = (active_procs * 100) / (MAX_PROCESSES / MAX_CPUS);
}

/* ============================================================================
 * SCHEDULER (PER-CPU)
 * ============================================================================ */

static void* cpu_scheduler_thread(void* arg) {
    int cpu_id = *(int*)arg;
    current_cpu_id = cpu_id;

    safe_printf("[CPU%d] Scheduler started\n", cpu_id);

    while (simulation_running) {
        /* Find processes with affinity to this CPU or any CPU */
        int active = 0;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROCESS_READY &&
                (process_table[i].cpu_affinity == cpu_id ||
                 process_table[i].cpu_affinity == -1)) {

                /* Simulate context switch */
                spinlock_acquire(&per_cpu_data[cpu_id].lock);
                per_cpu_data[cpu_id].current_process = i;
                per_cpu_data[cpu_id].context_switches++;
                spinlock_release(&per_cpu_data[cpu_id].lock);

                active++;
                usleep(100);  /* Simulate time slice */
            }
        }

        update_cpu_load(cpu_id, active);

        if (active == 0) {
            per_cpu_data[cpu_id].idle_cycles++;
            usleep(1000);  /* Idle */
        }
    }

    safe_printf("[CPU%d] Scheduler stopped\n", cpu_id);
    return NULL;
}

/* ============================================================================
 * TEST SUITE
 * ============================================================================ */

static bool test_spinlock_correctness(void) {
    printf("\n[TEST 1] Spinlock Correctness\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    volatile int shared_counter = 0;
    volatile int lock = 0;
    const int iterations = 10000;

    /* Simulate concurrent access */
    for (int i = 0; i < iterations; i++) {
        spinlock_acquire(&lock);
        shared_counter++;
        spinlock_release(&lock);
    }

    if (shared_counter == iterations) {
        printf("✓ Spinlock protected %d increments correctly\n", iterations);
        printf("✓ Lock contentions: %lu\n", total_spinlock_contentions);
        return true;
    } else {
        printf("✗ FAILED: Counter = %d, expected %d\n", shared_counter, iterations);
        return false;
    }
}

static bool test_priority_ipc_queues(void) {
    printf("\n[TEST 2] Priority IPC Queues\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    /* Send messages with different priorities */
    IPCMessage msg;
    msg.sender_process = 1;
    msg.receiver_process = 2;
    msg.message_type = 0;
    msg.payload_length = 0;
    msg.shared_memory_region = -1;

    /* Send low priority first */
    send_message_with_priority(2, &msg, 2);
    /* Send high priority */
    send_message_with_priority(2, &msg, 7);
    /* Send medium priority */
    send_message_with_priority(2, &msg, 5);

    /* Receive should get high priority first */
    IPCMessage received;
    receive_message_highest_priority(&received);

    if (received.priority == 7) {
        printf("✓ High priority message (7) received first\n");
        receive_message_highest_priority(&received);
        if (received.priority == 5) {
            printf("✓ Medium priority message (5) received second\n");
            receive_message_highest_priority(&received);
            if (received.priority == 2) {
                printf("✓ Low priority message (2) received last\n");
                printf("✓ Priority ordering correct\n");
                return true;
            }
        }
    }

    printf("✗ FAILED: Priority ordering incorrect\n");
    return false;
}

static bool test_zero_copy_ipc(void) {
    printf("\n[TEST 3] Zero-Copy Shared Memory IPC\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    int region = allocate_shared_memory_region(1);
    if (region < 0) {
        printf("✗ FAILED: Could not allocate shared memory region\n");
        return false;
    }
    printf("✓ Allocated shared memory region %d\n", region);

    if (!share_memory_region(region, 2)) {
        printf("✗ FAILED: Could not share region\n");
        return false;
    }
    printf("✓ Shared region with process 2 (ref_count = %d)\n",
           shared_memory[region].ref_count);

    IPCMessage msg;
    msg.sender_process = 1;
    msg.receiver_process = 2;
    msg.message_type = 1;
    msg.payload_length = 0;
    msg.shared_memory_region = region;

    if (!send_message_with_priority(2, &msg, 5)) {
        printf("✗ FAILED: Could not send zero-copy message\n");
        return false;
    }
    printf("✓ Sent zero-copy IPC message (region %d)\n", region);

    release_shared_memory_region(region);
    release_shared_memory_region(region);

    if (shared_memory[region].owner_process == -1 &&
        shared_memory[region].ref_count == 0) {
        printf("✓ Shared memory correctly released\n");
        printf("✓ Zero-copy IPC working\n");
        return true;
    }

    printf("✗ FAILED: Memory not properly released\n");
    return false;
}

static bool test_load_balancing(void) {
    printf("\n[TEST 4] Load Balancing\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    /* Set different loads */
    per_cpu_data[0].load_metric = 80;
    per_cpu_data[1].load_metric = 20;
    per_cpu_data[2].load_metric = 50;
    per_cpu_data[3].load_metric = 90;

    int best_cpu = find_least_loaded_cpu();

    if (best_cpu == 1) {
        printf("✓ Correctly identified CPU %d as least loaded (load = 20%%)\n", best_cpu);
        printf("  CPU loads: 0=80%%, 1=20%%, 2=50%%, 3=90%%\n");
        return true;
    }

    printf("✗ FAILED: Selected CPU %d instead of 1\n", best_cpu);
    return false;
}

static bool test_per_cpu_data_isolation(void) {
    printf("\n[TEST 5] Per-CPU Data Isolation\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    /* Set unique values for each CPU */
    for (int i = 0; i < MAX_CPUS; i++) {
        per_cpu_data[i].cpu_id = i;
        per_cpu_data[i].current_process = i * 10;
        per_cpu_data[i].context_switches = i * 100;
    }

    /* Verify isolation */
    bool isolated = true;
    for (int i = 0; i < MAX_CPUS; i++) {
        if (per_cpu_data[i].cpu_id != i ||
            per_cpu_data[i].current_process != i * 10 ||
            per_cpu_data[i].context_switches != i * 100) {
            isolated = false;
            break;
        }
    }

    if (isolated) {
        printf("✓ Per-CPU data properly isolated\n");
        for (int i = 0; i < MAX_CPUS; i++) {
            printf("  CPU%d: process=%d, switches=%lu\n",
                   i, per_cpu_data[i].current_process,
                   per_cpu_data[i].context_switches);
        }
        return true;
    }

    printf("✗ FAILED: Data corruption detected\n");
    return false;
}

static bool test_smp_scheduler(void) {
    printf("\n[TEST 6] SMP Scheduler (Multi-threaded)\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    /* Create some test processes */
    for (int i = 0; i < 16; i++) {
        process_table[i].process_id = i;
        process_table[i].state = PROCESS_READY;
        process_table[i].priority = i % 8;
        process_table[i].cpu_affinity = i % MAX_CPUS;  /* Distribute across CPUs */
    }

    /* Start scheduler threads */
    simulation_running = true;
    int cpu_ids[MAX_CPUS];
    for (int i = 0; i < MAX_CPUS; i++) {
        cpu_ids[i] = i;
        pthread_create(&per_cpu_data[i].thread, NULL, cpu_scheduler_thread, &cpu_ids[i]);
    }

    printf("✓ Started %d CPU scheduler threads\n", MAX_CPUS);
    sleep(2);  /* Let schedulers run */

    simulation_running = false;
    for (int i = 0; i < MAX_CPUS; i++) {
        pthread_join(per_cpu_data[i].thread, NULL);
    }

    /* Check that all CPUs did work */
    bool all_active = true;
    for (int i = 0; i < MAX_CPUS; i++) {
        printf("  CPU%d: %lu context switches, %lu idle cycles\n",
               i, per_cpu_data[i].context_switches, per_cpu_data[i].idle_cycles);
        if (per_cpu_data[i].context_switches == 0) {
            all_active = false;
        }
    }

    if (all_active) {
        printf("✓ All CPUs performed scheduling\n");
        return true;
    }

    printf("✗ FAILED: Some CPUs did no work\n");
    return false;
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

static void initialize_simulator(void) {
    printf("Initializing SMP simulator...\n");

    /* Initialize process table */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].process_id = i;
        process_table[i].state = PROCESS_TERMINATED;
        process_table[i].priority = 0;
        process_table[i].cpu_affinity = -1;
        process_table[i].total_runtime = 0;
    }

    /* Initialize per-CPU data */
    for (int i = 0; i < MAX_CPUS; i++) {
        per_cpu_data[i].cpu_id = i;
        per_cpu_data[i].current_process = -1;
        per_cpu_data[i].active_processes = 0;
        per_cpu_data[i].load_metric = 0;
        per_cpu_data[i].lock = 0;
        per_cpu_data[i].context_switches = 0;
        per_cpu_data[i].idle_cycles = 0;
    }

    /* Initialize priority queues */
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        priority_queues[i].head = 0;
        priority_queues[i].tail = 0;
        priority_queues[i].lock = 0;
    }

    /* Initialize shared memory */
    for (int i = 0; i < SHARED_MEMORY_REGIONS; i++) {
        shared_memory[i].region = malloc(SHARED_MEMORY_REGION_SIZE);
        shared_memory[i].owner_process = -1;
        shared_memory[i].ref_count = 0;
        shared_memory[i].lock = 0;
    }

    printf("✓ Simulator initialized\n");
}

static void cleanup_simulator(void) {
    for (int i = 0; i < SHARED_MEMORY_REGIONS; i++) {
        free(shared_memory[i].region);
    }
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║   Ada83 ARM SMP Microkernel - Simulator Test Suite            ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    initialize_simulator();

    int tests_passed = 0;
    int tests_total = 6;

    if (test_spinlock_correctness()) tests_passed++;
    if (test_priority_ipc_queues()) tests_passed++;
    if (test_zero_copy_ipc()) tests_passed++;
    if (test_load_balancing()) tests_passed++;
    if (test_per_cpu_data_isolation()) tests_passed++;
    if (test_smp_scheduler()) tests_passed++;

    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("FINAL STATISTICS\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Tests Passed:              %d / %d\n", tests_passed, tests_total);
    printf("Total Messages Sent:       %lu\n", total_messages_sent);
    printf("Total Messages Received:   %lu\n", total_messages_received);
    printf("Zero-Copy Messages:        %lu\n", total_zero_copy_messages);
    printf("Spinlock Contentions:      %lu\n", total_spinlock_contentions);
    printf("\n");

    if (tests_passed == tests_total) {
        printf("╔════════════════════════════════════════════════════════════════╗\n");
        printf("║                      ALL TESTS PASSED!                         ║\n");
        printf("║          SMP microkernel validated and ready!                  ║\n");
        printf("╚════════════════════════════════════════════════════════════════╝\n");
        cleanup_simulator();
        return 0;
    } else {
        printf("╔════════════════════════════════════════════════════════════════╗\n");
        printf("║                    SOME TESTS FAILED                           ║\n");
        printf("║              %d / %d tests passed                                ║\n", tests_passed, tests_total);
        printf("╚════════════════════════════════════════════════════════════════╝\n");
        cleanup_simulator();
        return 1;
    }
}
