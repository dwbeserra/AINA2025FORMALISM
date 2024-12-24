#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#ifdef _OPENMP
#include <omp.h>
#endif

typedef struct {
    int is_context;
    int is_invalid;
} RemarkableState;

typedef struct {
    int num_states;
    RemarkableState *states;
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

int evaluate_policies_classic_seq(Policy *policies, int num_policies) {
    int invalid_count = 0;
    for (int i = 0; i < num_policies; i++) {
        for (int s = 0; s < policies[i].num_states; s++) {
            evaluate_state(&policies[i].states[s]);
            if (policies[i].states[s].is_invalid) invalid_count++;
        }
    }
    return invalid_count;
}

int evaluate_policies_classic_par(Policy *policies, int num_policies) {
    int invalid_count = 0;
    // Parallel version with runtime schedule
    #pragma omp parallel for reduction(+:invalid_count) schedule(runtime)
    for (int i = 0; i < num_policies; i++) {
        for (int s = 0; s < policies[i].num_states; s++) {
            evaluate_state(&policies[i].states[s]);
            if (policies[i].states[s].is_invalid) {
                invalid_count++;
            }
        }
    }
    return invalid_count;
}

Policy* init_classic_policies(int num_policies, int states_per_policy) {
    Policy *policies = (Policy*)malloc(num_policies*sizeof(Policy));
    for (int i = 0; i < num_policies; i++) {
        policies[i].num_states = states_per_policy;
        policies[i].states = (RemarkableState*)malloc(states_per_policy*sizeof(RemarkableState));
        for (int s = 0; s < states_per_policy; s++) {
            policies[i].states[s].is_context = (rand()%100 < 50) ? 1 : 0;
            policies[i].states[s].is_invalid = 0;
        }
    }
    return policies;
}

void apply_invalidation_classic(Policy *policies, int num_policies, int states_per_policy, double inval_rate) {
    int total_states = num_policies * states_per_policy;
    int to_invalidate = (int)(total_states * inval_rate);

    for (int i = 0; i < to_invalidate; i++) {
        int pi = rand() % num_policies;
        int si = rand() % states_per_policy;
        policies[pi].states[si].is_invalid = 1;
    }
}

void free_classic_policies(Policy *policies, int num_policies) {
    for (int i = 0; i < num_policies; i++) {
        free(policies[i].states);
    }
    free(policies);
}

// CSV output:
// ScenarioType,Policies,States,InvalRate,MandatoryRatio,TargetMode,Mode,Run,InvalidCount,Time_ms
// For Classic: MandatoryRatio = -1, TargetMode = -1
int main(int argc, char **argv) {
    // Usage:
    // classic_formalism <num_policies> <states_per_policy> <inval_rate> <mode: Seq|Par> <schedule: static|dynamic|guided> <run> [seed]
    if (argc < 7) {
        fprintf(stderr, "Usage: %s <num_policies> <states_per_policy> <inval_rate> <mode> <schedule> <run> [seed]\n", argv[0]);
        return 1;
    }

    int num_policies = atoi(argv[1]);
    int states_per_policy = atoi(argv[2]);
    double inval_rate = atof(argv[3]);
    char* mode = argv[4]; // "Seq" or "Par"
    char* schedule_str = argv[5]; // "static", "dynamic", "guided"
    int run = atoi(argv[6]);
    int seed = (argc > 7) ? atoi(argv[7]) : 42;
    srand(seed);

#ifdef _OPENMP
    // Set schedule at runtime
    omp_sched_t sched_kind;
    if (strcmp(schedule_str,"dynamic")==0) sched_kind = omp_sched_dynamic;
    else if (strcmp(schedule_str,"guided")==0) sched_kind = omp_sched_guided;
    else sched_kind = omp_sched_static;
    omp_set_schedule(sched_kind,1);
#endif

    Policy *pol = init_classic_policies(num_policies, states_per_policy);
    apply_invalidation_classic(pol, num_policies, states_per_policy, inval_rate);

    double start = get_time_usec();
    int invalid_count;

    if (strcmp(mode,"Seq")==0) {
        invalid_count = evaluate_policies_classic_seq(pol, num_policies);
    } else {
        // Parallel mode
        invalid_count = evaluate_policies_classic_par(pol, num_policies);
    }

    double end = get_time_usec();
    double time_ms = (end - start)/1000.0;

    printf("Classic,%d,%d,%.2f,-1,-1,%s,%d,%d,%.2f,%s\n",
           num_policies, states_per_policy, inval_rate, mode, run, invalid_count, time_ms, schedule_str);

    free_classic_policies(pol, num_policies);
    return 0;
}

