#!/usr/bin/env bash

# Ensure binaries are compiled with OpenMP:
# gcc classic_formalism.c -o classic_formalism -O2 -fopenmp
# gcc extended_formalism_ff.c -o extended_formalism_ff -O2 -fopenmp

#WARNING ! 
# 1- These sets are just exemples. You should configure your own
# 2- These tests involves random number generators, so variations in results are expected. If you do not
# get a linear output for the extended formalism, it is totaly normal. Try to change the seed used for the random number
# generator


#Multiparameter test exemple
POLICIES=(10 100 1000)
STATES=(10 100)
INVAL_RATES=(0.001 0.01 0.1 0.2 0.6 1.0)
MANDATORY_RATIOS=(0.5)  
TARGET_MODES=(1)
SEED=42
REPS=30
SCHEDULES=("dynamic")


#Another exemple
POLICIES=(40)
STATES=(25)
INVAL_RATES=(0.01)       # Low invalidation, but not zero
MANDATORY_RATIOS=(0.1)   # Only 20% of policies are mandatory
TARGET_MODES=(0)         # Random distribution among mandatory/optional
SEED=118                 # Fixed seed to keep sensor delays & invalidation consistent
REPS=30                  # Enough runs to confirm average times
SCHEDULES=("dynamic")    # Minimizes scheduling randomness if running in parallel



#Only Invalid Remarkable States Ratio Varies
POLICIES=(1000)
STATES=(1000)
INVAL_RATES=(0.001 0.01 0.1 0.2 0.5 1.0) 
MANDATORY_RATIOS=(0.1)  
TARGET_MODES=(0)
SEED=42
REPS=30
SCHEDULES=("dynamic")


mkdir -p results/classic results/extended


# Extended Formalism
for p in "${POLICIES[@]}"; do
    for s in "${STATES[@]}"; do
        for ir in "${INVAL_RATES[@]}"; do
            for mr in "${MANDATORY_RATIOS[@]}"; do
                for tm in "${TARGET_MODES[@]}"; do
                    outfile="results/extended/ext_P${p}_S${s}_IR${ir}_MR${mr}_TM${tm}.csv"
                    echo "ScenarioType,Policies,States,InvalRate,MandatoryRatio,TargetMode,Mode,Run,InvalidCount,Time_ms,Schedule" > "$outfile"

                    # Sequential runs: 1 thread
                    export OMP_NUM_THREADS=1
                    for ((rep=1; rep<=$REPS; rep++)); do
                       ./extended_formalism_ff $p $s $ir $mr $tm "Seq" "static" $rep $SEED >> "$outfile"
                    done

                    # Parallel runs: 10 threads
                    export OMP_NUM_THREADS=10
                    for sched in "${SCHEDULES[@]}"; do
                        for ((rep=1; rep<=$REPS; rep++)); do
                            ./extended_formalism_ff $p $s $ir $mr $tm "Par" $sched $rep $SEED >> "$outfile"
                        done
                    done
                done
            done
        done
    done
done


# Classic Formalism
for p in "${POLICIES[@]}"; do
    for s in "${STATES[@]}"; do
        for ir in "${INVAL_RATES[@]}"; do
            outfile="results/classic/classic_P${p}_S${s}_IR${ir}.csv"
            echo "ScenarioType,Policies,States,InvalRate,MandatoryRatio,TargetMode,Mode,Run,InvalidCount,Time_ms,Schedule" > "$outfile"

            # Sequential runs: 1 thread
            export OMP_NUM_THREADS=1
            for ((rep=1; rep<=$REPS; rep++)); do
                ./classic_formalism $p $s $ir "Seq" "static" $rep $SEED >> "$outfile"
            done

            # Parallel runs: 10 threads
            export OMP_NUM_THREADS=10
            for sched in "${SCHEDULES[@]}"; do
                for ((rep=1; rep<=$REPS; rep++)); do
                    ./classic_formalism $p $s $ir "Par" $sched $rep $SEED >> "$outfile"
                done
            done
        done
    done
done



