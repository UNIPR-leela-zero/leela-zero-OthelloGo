// =================================================================================================
// This file is part of Leela Zero OpenCL kernels
// Element-wise addition kernel for residual connections
// =================================================================================================

// Enables loading of this file using the C++ pre-processor's #include (C++11 standard raw string
// literal). Comment-out this line for syntax-highlighting when developing.
R"(

// =================================================================================================

// Kernel per l'addizione elemento per elemento di due buffer
// Usato per implementare le skip connections nei residual blocks
__kernel void add_buffer(__global const net_t * restrict source,
                        __global net_t * restrict dest,
                        const int size) {
    const int gid = get_global_id(0);
    
    if (gid < size) {
        // ✅ Usa le macro definite in common.opencl per gestire fp16
        real source_val = vload_net_t(gid, source);
        real dest_val = vload_net_t(gid, dest);
        real result;
        Add(result, dest_val, source_val);
        vstore_net_t(result, gid, dest);
    }
}

// Versione ottimizzata che usa work group size multiple
__kernel void add_buffer_optimized(__global const net_t* restrict source,
                                  __global net_t* restrict dest,
                                  const int size) {
    const int gid = get_global_id(0);
    const int local_id = get_local_id(0);
    const int group_size = get_local_size(0);
    
    // Processa più elementi per thread se possibile
    const int elements_per_thread = 4;
    const int start_idx = gid * elements_per_thread;
    
    for (int i = 0; i < elements_per_thread && (start_idx + i) < size; i++) {
        const int idx = start_idx + i;
        real source_val = vload_net_t(idx, source);
        real dest_val = vload_net_t(idx, dest);
        real result;
        Add(result, dest_val, source_val);
        vstore_net_t(result, idx, dest);
    }
}

// Versione vettorizzata per float (non funziona con half)
#if PRECISION == 32
__kernel void add_buffer_vec4(__global const float4* restrict source,
                             __global float4* restrict dest,
                             const int vec_size,
                             const int total_size) {
    const int gid = get_global_id(0);
    
    // Processa gruppi di 4 elementi
    if (gid < vec_size) {
        dest[gid] += source[gid];
    }
    
    // Gestisci gli elementi rimanenti
    const int remainder_start = vec_size * 4;
    if (gid < (total_size - remainder_start)) {
        const int remainder_idx = remainder_start + gid;
        if (remainder_idx < total_size) {
            __global const float* src_scalar = (__global const float*)source;
            __global float* dst_scalar = (__global float*)dest;
            dst_scalar[remainder_idx] += src_scalar[remainder_idx];
        }
    }
}
#endif

// =================================================================================================

// End of the C++11 raw string literal
)"