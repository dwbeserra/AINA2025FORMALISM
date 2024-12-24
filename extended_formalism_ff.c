#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif

typedef enum {
    SUBSET_CPU,
    SUBSET_MEMORY,
    SUBSET_NETWORK
} SubsetType;

typedef struct {
    int is_context; 
    int is_invalid; 
    SubsetType subset;
} RemarkableState;

typedef struct {
    int num_states;
    RemarkableState *states;
    int is_mandatory; // 1 or 0
} Policy;

static inline double get_time_usec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000.0 + tv.tv_usec;
}

void evaluate_state(const RemarkableState *st) {
    if (st->is_context) {
        int delay_ms = 1 + (rand() % 10);
        usleep(delay_ms * 1000);
    } else {
        usleep(10);
    }
}

static volatile int stop_all = 0;

int evaluate_policies_extended_sequential(Policy *policies, int num_policies) {
    int invalid_count = 0;
    stop_all = 0;
    for (int i = 0; i < num_policies && !stop_all; i++) {
        for (int s = 0; s < policies[i].num_states && !stop_all; s++) {
            evaluate_state(&policies[i].states[s]);
            if (policies[i].states[s].is_invalid) {
                invalid_count++;
                if (policies[i].is_mandatory) {
                    stop_all = 1;
                }
            }
        }
    }
    return invalid_count;
}

int evaluate_policies_extended_parallel(Policy *policies, int num_policies) {
    int invalid_count = 0;
    stop_all = 0;
#ifdef _OPENMP
    #pragma omp parallel for reduction(+:invalid_count) schedule(runtime)
    for (int i = 0; i < num_policies; i++) {
        int stop_policy = 0;
        for (int s = 0; s < policies[i].num_states && !stop_policy && !stop_all; s++) {
            if (stop_all) break;
            evaluate_state(&policies[i].states[s]);
            if (policies[i].states[s].is_invalid) {
                invalid_count++;
                if (policies[i].is_mandatory) {
                    #pragma omp atomic write
                    stop_all = 1;
                    stop_policy = 1;
                }
            }
        }
    }
#endif
    return invalid_count;
}

Policy* init_extended_policies(int num_policies, int states_per_policy, double mandatory_ratio) {
    Policy *policies = (Policy*) malloc(num_policies*sizeof(Policy));
    int mandatory_thresh = (int)(num_policies * mandatory_ratio);
    for (int i = 0; i < num_policies; i++) {
        policies[i].num_states = states_per_policy;
        policies[i].states = (RemarkableState*)malloc(states_per_policy*sizeof(RemarkableState));
        policies[i].is_mandatory = (i < mandatory_thresh) ? 1 : 0;
        for (int s = 0; s < states_per_policy; s++) {
            policies[i].states[s].is_context = (rand()%100 < 50) ? 1 : 0;
            policies[i].states[s].is_invalid = 0;
            int r = rand()%3;
            if (r == 0) policies[i].states[s].subset = SUBSET_CPU;
            else if (r == 1) policies[i].states[s].subset = SUBSET_MEMORY;
            else policies[i].states[s].subset = SUBSET_NETWORK;
        }
    }
    return policies;
}

void apply_invalidation_extended(Policy *policies, int num_policies, int states_per_policy, double inval_rate, int target_mode) {
    int total_states = num_policies * states_per_policy;
    int to_invalidate = (int)(total_states * inval_rate);
    int attempts = 0;
    while (to_invalidate > 0 && attempts < total_states*10) {
        attempts++;
        int pi = rand()%num_policies;
        if ((target_mode == 1 && policies[pi].is_mandatory == 0) ||
            (target_mode == -1 && policies[pi].is_mandatory == 1)) {
            continue; 
        }
        int si = rand()%policies[pi].num_states;
        if (!policies[pi].states[si].is_invalid) {
            policies[pi].states[si].is_invalid = 1;
            to_invalidate--;
        }
    }
}

void free_extended_policies(Policy *policies, int num_policies) {
    for (int i = 0; i < num_policies; i++) {
        free(policies[i].states);
    }
    free(policies);
}

// CSV fields:
// ScenarioType,Policies,States,InvalRate,MandatoryRatio,TargetMode,Mode,Run,InvalidCount,Time_ms,Schedule
int main(int argc, char **argv) {
    // Usage:
    // extended_formalism_ff <num_policies> <states_per_policy> <inval_rate> <mandatory_ratio> <target_mode> <mode: Seq|Par> <schedule: static|dynamic|guided> <run> [seed]
    if (argc < 9) {
        fprintf(stderr, "Usage: %s <num_policies> <states_per_policy> <inval_rate> <mandatory_ratio> <target_mode> <mode> <schedule> <run> [seed]\n", argv[0]);
        return 1;
    }

    int num_policies = atoi(argv[1]);
    int states_per_policy = atoi(argv[2]);
    double inval_rate = atof(argv[3]);
    double mandatory_ratio = atof(argv[4]);
    int target_mode = atoi(argv[5]);
    char *mode = argv[6];     // "Seq" or "Par"
    char *schedule_str = argv[7]; // "static", "dynamic", "guided"
    int run = atoi(argv[8]);
    int seed = (argc > 9) ? atoi(argv[9]) : 42;
    srand(seed);

#ifdef _OPENMP
    omp_sched_t sched_kind;
    if (strcmp(schedule_str,"dynamic")==0) sched_kind = omp_sched_dynamic;
    else if (strcmp(schedule_str,"guided")==0) sched_kind = omp_sched_guided;
    else sched_kind = omp_sched_static;
    omp_set_schedule(sched_kind,1);
#endif

    Policy *pol = init_extended_policies(num_policies, states_per_policy, mandatory_ratio);
    apply_invalidation_extended(pol, num_policies, states_per_policy, inval_rate, target_mode);

    double start = get_time_usec();
    int invalid_count;
    if (strcmp(mode,"Seq")==0) {
        invalid_count = evaluate_policies_extended_sequential(pol, num_policies);
    } else {
        invalid_count = evaluate_policies_extended_parallel(pol, num_policies);
    }

    double end = get_time_usec();
    double time_ms = (end - start)/1000.0;

    printf("Extended,%d,%d,%.2f,%.2f,%d,%s,%d,%d,%.2f,%s\n",
           num_policies, states_per_policy, inval_rate, mandatory_ratio, target_mode,
           mode, run, invalid_count, time_ms, schedule_str);

    free_extended_policies(pol, num_policies);
    return 0;
}

