

/**
 * The following constants have been passed to the OpenCL program by the Host.
 * INVALID
 * MASKBITS
 * MASK
 * WORK_GROUP_SIZE
 * MAX_PATTERN_SIZE
 * WARP_SIZE
 * WARP_SHIFT
 */

/**
 * Structure to hold the inclusive scan (prefix sum) state information shared
 * across Work Groups. An array of these items is created in global Device
 * memory and used to share state between Work Groups in the pfacCompact
 * Kernel so we can avoid the need to launch multiple Kernels.
 */
typedef struct WorkGroupSum_t {
    int workGroupSum;
    int inclusivePrefix;
} WorkGroupSum;

/**
 * The pfacCompact scan returns an array where each element represents a match
 * and comprises the index within the input and the pattern ID of each match.
 */
typedef struct MatchEntry_t {
    int index;
    int value;
} MatchEntry;

/**
 * 257 is the prime number used in the hash function and has the useful
 * property that we can do reduction modulo 257 using (x & 255) - (x >> 8)
 * http://mymathforum.com/number-theory/11914-calculate-10-7-mod-257-a.html
 */
static inline int mod257(int x) {
    int mod = (x & 255) - (x >> 8);
    if (mod < 0) {
        mod += 257;
    }
    return mod;
}

/**
 * Look up the next state in the hash table given the current state and the
 * transition (input) character. The hash table is held in image1d_buffer_t
 * objects accessed via read_imagei. Note that the initial transition is
 * accessed separately via the initialTransitionsCache in the main Kernel code.
 */
static inline int lookup(image1d_buffer_t hashRow,
                         image1d_buffer_t hashVal,
                         int state,
                         int inputChar) {
    const int2 row = read_imagei(hashRow, state).xy; // hashRow[state]
    const int offset  = row.x;
    int nextState = INVALID;
    if (offset >= 0) {
        const int k_sminus1 = row.y;
        const int sminus1 = k_sminus1 & MASK;
        const int k = k_sminus1 >> MASKBITS; 

        const int p = mod257(k * inputChar) & sminus1;
        const int2 value = read_imagei(hashVal, offset + p).xy; // hashVal[offset + p]
        if (inputChar == value.x) {
            nextState = value.y;
        }
    }
    return nextState;
}

/**
 * Simple PFAC Kernel. Copies WORK_GROUP_SIZE + MAX_PATTERN_SIZE integers from
 * global memory to local (shared) memory for each Work Group (thread block)
 * then transitions the state machine. The state machine holds the initial
 * transition in an array in local memory and the remainder in image1d_buffer_t
 * objects in order to make use of GPU texture memory, which is cached.
 */

__kernel void pfac(image1d_buffer_t initialTransitions, image1d_buffer_t hashRow, image1d_buffer_t hashVal, int initialState, global int* input, global int* output, int inputSize, int n){ 

// Calculate the index of the first character in the Work Group.
    const int firstCharInWorkGroup = get_group_id(0) * WORK_GROUP_SIZE * sizeof(int);

    // Calculate remaining characters, starting from firstCharInWorkGroup.
    const int remaining = inputSize - firstCharInWorkGroup;

    // Calculate the local memory buffer size in bytes, noting that the last
    // work-group may contain fewer characters than the maximum buffer size.
    const int MAX_BUFFER_SIZE = (WORK_GROUP_SIZE + MAX_PATTERN_SIZE) * sizeof(int);
    const int bufferSize = min(remaining, MAX_BUFFER_SIZE);

    const int tid = get_local_id(0); // Thread (Work Item) ID

    int inputIndex  = get_global_id(0);
    int outputIndex = firstCharInWorkGroup + tid;

    // Local (i.e. shared by all threads in the Work Group) memory arrays.
    local int initialTransitionsCache[WORK_GROUP_SIZE];
    local int cache[WORK_GROUP_SIZE + MAX_PATTERN_SIZE];
    local unsigned char* buffer = (local unsigned char*)cache;

    // Load the initialTransitions table to local (shared) memory.
    initialTransitionsCache[tid] = read_imagei(initialTransitions, tid).x;

    // Read input data from global memory to local (shared) memory, n is the
    // number of OpenCL integers that would completely contain the input bytes.
    if (inputIndex < n) {
        cache[tid] = input[inputIndex];
    }

    // Read extra input data as we need overlaps to mitigate boundary condition.
    inputIndex += WORK_GROUP_SIZE;
    if ((inputIndex < n) && (tid < MAX_PATTERN_SIZE)) {
        cache[tid + WORK_GROUP_SIZE] = input[inputIndex];
    }

    // Block until all Work Items in the Work Group have reached this point
    // to ensure correct ordering of memory operations to local memory. 
 	barrier(CLK_LOCAL_MEM_FENCE);

    // Perform state machine look-up with each thread processing four characters.
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        const int j = tid + i * WORK_GROUP_SIZE;
        int pos = j;

        if (pos >= bufferSize) return;

        int match = -1;
        int inputChar = buffer[pos];
        int nextState = initialTransitionsCache[inputChar];
        if (nextState != INVALID) {
            if (nextState < initialState) {
                match = nextState;
//printf("xx matched pattern %d at %d\n", nextState, j);
            }
            pos = pos + 1;
            while (pos < bufferSize) {
                inputChar = buffer[pos];
                nextState = lookup(hashRow, hashVal, nextState, inputChar);
                if (nextState == INVALID) {
                    break;
                }

                if (nextState < initialState) {
                    match = nextState;
//printf("matched pattern %d at %d\n", nextState, j);
                }
                pos = pos + 1;
            }
        }

        // Output results to global memory
        output[outputIndex] = match;
        outputIndex += WORK_GROUP_SIZE;
    }
}

