R"(

#ifndef OUTIN_KWG
#define OUTIN_KWG 2
#endif

#ifndef OUT_KWG
#define OUT_KWG 32
#endif

#ifndef OUT_BWG
#define OUT_BWG 2
#endif

__constant real Bt[WINOGRAD_ALPHA * WINOGRAD_ALPHA] = \
                   {1.0f,  0.0f,     -5.0f/2.0f,  0.0f,      1.0f, 0.0f,
                    0.0f, -SQ2,      -2.0f,       SQ2/2.0f,  1.0f, 0.0f,
                    0.0f,  SQ2,      -2.0f,      -SQ2/2.0f,  1.0f, 0.0f,
                    0.0f, -SQ2/2.0f, -1.0f/2.0f,  SQ2,       1.0f, 0.0f,
                    0.0f,  SQ2/2.0f, -1.0f/2.0f, -SQ2,       1.0f, 0.0f,
                    0.0f,  1.0f,      0.0f,      -5.0f/2.0f, 0.0f, 1.0f};
void multiply_bt(
    real * o0, real * o1, real * o2, real * o3, real * o4, real * o5,
    real i0, real i1, real i2, real i3, real i4, real i5
) {
    real i3m1 = i1 * -SQ2 + i3 * (SQ2 / 2.0f);
    real i4m2 = i2 * -2.0f + i4 * 1.0f;

    *o0 = i0 + i2 * (-5.0f/2.0f) + i4;
    *o1 = i3m1 + i4m2;
    *o2 = -i3m1 + i4m2;

    real i3m1_2 = i3 * (SQ2) + i1 * (-SQ2/2.0f);
    real i4m2_2 = i2 * (-1.0f/2.0f) + i4;

    *o3 = i3m1_2 + i4m2_2;
    *o4 = -i3m1_2 + i4m2_2;

    *o5 = i1 + i3 * (-5.0f/2.0f) + i5;
}


__constant real At[WINOGRAD_M * WINOGRAD_ALPHA] = \
                   {1.0f, 1.0f,      1.0f,       1.0f,      1.0f,     0.0f,
                    0.0f, SQ2/2.0f, -SQ2/2.0f,   SQ2,      -SQ2,      0.0f,
                    0.0f, 1.0f/2.0f, 1.0f/2.0f,  2.0f,      2.0f,     0.0f,
                    0.0f, SQ2/4.0f, -SQ2/4.0f,   2.0f*SQ2, -2.0f*SQ2, 1.0f};
void multiply_atv(
    real4 * o,
    real i0, real i1, real i2, real i3, real i4, real i5
) {
    real t1p2 = (i1 + i2) * (1.0f / 2.0f);
    real t1m2 = (i1 - i2) * (SQ2/4.0f);
    real t3p4 = i3 + i4;
    real t3m4 = (i3 - i4) * (SQ2);

    (*o).x = i0 + t1p2 + t1p2 + t3p4;
    (*o).y = t1m2 + t1m2 + t3m4;
    (*o).z = t1p2 + t3p4 + t3p4;
    (*o).w = t1m2 + t3m4 + t3m4 + i5;
}


void multiply_at(
    real * o0, real * o1, real * o2, real * o3,
    real i0, real i1, real i2, real i3, real i4, real i5
) {
    real4 o;
    multiply_atv(&o, i0, i1, i2, i3, i4, i5);

    *o0 = o.x;
    *o1 = o.y;
    *o2 = o.z;
    *o3 = o.w;
}

void __in_transform_eq(real x[WINOGRAD_ALPHA][WINOGRAD_ALPHA], __global net_t * restrict V, int offset, int CPpad) {

    const int W = BOARD_SIZE;
    const int H = BOARD_SIZE;
    const int P = WTILES * WTILES;

    real T1[WINOGRAD_ALPHA][WINOGRAD_ALPHA];
    real T2[WINOGRAD_ALPHA][WINOGRAD_ALPHA];

    // Calculates transpose(B).x.B
#ifdef WINOGRAD_SIMD
    for (int i = 0; i < WINOGRAD_ALPHA; i++){
        for (int j = 0; j < WINOGRAD_ALPHA; j++) {
            real2 acc = {ZERO, ZERO};
            real2 *x2 = (real2 *)&x[j][0];
            for (int k = 0; k < WINOGRAD_ALPHA/2; k++) {
                real2 x1;
                x1.x = Bt[i * WINOGRAD_ALPHA + 2*k];
                x1.y = Bt[i * WINOGRAD_ALPHA + 2*k + 1];
                acc += x1 * x2[k];
            }
            T1[i][j] = acc.x + acc.y;
        }
    }
#else
    for (int j = 0; j < WINOGRAD_ALPHA; j++) {
        multiply_bt(
            &(T1[0][j]), &(T1[1][j]), &(T1[2][j]), &(T1[3][j]), &(T1[4][j]), &(T1[5][j]),
            x[j][0], x[j][1], x[j][2], x[j][3], x[j][4], x[j][5]
        );
    }
#endif

#ifdef WINOGRAD_SIMD
    for (int i = 0; i < WINOGRAD_ALPHA; i++){
        for (int j = 0; j < WINOGRAD_ALPHA; j++) {
            real2 acc = {ZERO, ZERO};
            real2 *x1 = (real2 *)&T1[i][0];
            for (int k = 0; k < WINOGRAD_ALPHA/2; k++) {
                real2 x2;
                x2.x = Bt[j * WINOGRAD_ALPHA + 2*k];
                x2.y = Bt[j * WINOGRAD_ALPHA + 2*k + 1];
                acc += x1[k] * x2;
            }
            T2[i][j] = acc.x + acc.y;
        }
    }
#else
    for (int i = 0; i < WINOGRAD_ALPHA; i++){
        multiply_bt(
            &(T2[i][0]),  &(T2[i][1]),  &(T2[i][2]),  &(T2[i][3]),  &(T2[i][4]),  &(T2[i][5]),
            T1[i][0], T1[i][1], T1[i][2], T1[i][3], T1[i][4], T1[i][5]
        );
    }
#endif

    // Scatter each sub element in tile to separate matrices
    for (int i = 0; i < WINOGRAD_ALPHA; i++) {
        for (int j = 0; j < WINOGRAD_ALPHA; j++) {
            vstore_net_t(T2[i][j], (i*WINOGRAD_ALPHA + j)*CPpad + offset, V);
        }
    }
}
// Kernel per accumulo nel dominio Winograd con batch normalization
__kernel void winograd_add(__global const net_t * restrict source,
                           __global net_t * restrict accumulation,
                           __constant const net_t * restrict means,
                           __constant const net_t * restrict stddivs,
                           const int K,
                           const int batch_size) {
    
    const int P = WTILES * WTILES;
    const int idx = get_global_id(0);
    
    // Calcola gli indici per il buffer Winograd
    // Layout: [WINOGRAD_TILE][K][batch_size * P]
    const int total_elements = WINOGRAD_TILE * K * batch_size * P;
    
    if (idx < total_elements) {
        const int tile_idx = idx / (K * batch_size * P);
        const int remaining = idx - tile_idx * (K * batch_size * P);
        const int k = remaining / (batch_size * P);
        const int batch_tile = remaining - k * (batch_size * P);
        
        // Applica batch normalization e accumula
        const real mean = vload_net_t(k, means);
        const real scale_stddiv = vload_net_t(k, stddivs);
        
        real source_val = vload_net_t(idx, source);
        real acc_val = vload_net_t(idx, accumulation);
        
        // Batch norm + accumulo
        real normalized = (source_val - mean) * scale_stddiv;
        real result = acc_val + normalized;
        
        vstore_net_t(result, idx, accumulation);
    }
}

// Kernel per trasformazione finale dal dominio Winograd al dominio spaziale
__kernel __attribute__((reqd_work_group_size(32, 2, 1)))
void winograd_final_transform(__global const net_t * restrict M,
                              __global net_t * restrict Y,
                              const int K,
                              const int batch_size) {
    
    const int W = BOARD_SIZE;
    const int H = BOARD_SIZE;
    const int P = WTILES * WTILES;
    
    const int k = get_global_id(0);
    const int block = get_global_id(1);
    
    // Dimensioni locali per cache
    __local real out_buf[32][2][WINOGRAD_M][WINOGRAD_M + 1];
    
    volatile int kid = get_local_id(0);
    volatile int bid = get_local_id(1);
    
    if (k < K && block < batch_size * P) {
        real temp[WINOGRAD_M][WINOGRAD_ALPHA];
        
        // M è nel formato accumulato [WINOGRAD_TILE][K][batch_size * P]
        const int base_offset = k * batch_size * P + block;
        
        // Calcola transpose(A).temp_m
        for (int xn = 0; xn < WINOGRAD_ALPHA; xn++) {
            real temp_m0 = vload_net_t((0 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            real temp_m1 = vload_net_t((1 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            real temp_m2 = vload_net_t((2 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            real temp_m3 = vload_net_t((3 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            real temp_m4 = vload_net_t((4 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            real temp_m5 = vload_net_t((5 * WINOGRAD_ALPHA + xn) * K * batch_size * P + base_offset, M);
            
            multiply_at(
                &(temp[0][xn]), &(temp[1][xn]), &(temp[2][xn]), &(temp[3][xn]),
                temp_m0, temp_m1, temp_m2, temp_m3, temp_m4, temp_m5
            );
        }
        
        // Calcola temp.A e applica ReLU
        for (int i = 0; i < WINOGRAD_M; i++) {
            real4 r;
            multiply_atv(
                &r,
                temp[i][0], temp[i][1], temp[i][2], temp[i][3], temp[i][4], temp[i][5]
            );
            
            // Applica ReLU
            r.x = r.x > ZERO ? r.x : ZERO;
            r.y = r.y > ZERO ? r.y : ZERO;
            r.z = r.z > ZERO ? r.z : ZERO;
            r.w = r.w > ZERO ? r.w : ZERO;
            
            out_buf[kid][bid][i][0] = r.x;
            out_buf[kid][bid][i][1] = r.y;
            out_buf[kid][bid][i][2] = r.z;
            out_buf[kid][bid][i][3] = r.w;
        }
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);
    
    // Scrivi i risultati nel buffer di output
    for (int idx = get_local_id(0) + get_local_size(0) * get_local_id(1); 
         idx < 32 * 2 * WINOGRAD_M * WINOGRAD_M; 
         idx += get_local_size(0) * get_local_size(1)) {
        
        const int k_local = idx / (2 * WINOGRAD_M * WINOGRAD_M);
        const int idx_block = idx - k_local * 2 * WINOGRAD_M * WINOGRAD_M;
        const int row = idx_block / (WINOGRAD_M * 2);
        const int col = idx_block - row * WINOGRAD_M * 2;
        const int block_local = col / WINOGRAD_M;
        const int j = col % WINOGRAD_M;
        const int i = row % WINOGRAD_M;
        
        const int blockt = get_group_id(1) * get_local_size(1) + block_local;
        const int kt = get_group_id(0) * get_local_size(0) + k_local;
        
        const int batch = blockt / P;
        const int blockt_x = (blockt - P * batch) % WTILES;
        const int blockt_y = (blockt - P * batch) / WTILES;
        
        const int x = WINOGRAD_M * blockt_x;
        const int y = WINOGRAD_M * blockt_y;
        const int out_idx = batch * K * NUM_INTERSECTIONS + kt * NUM_INTERSECTIONS + (y + i) * W + (x + j);
        
        if (kt < K && blockt < batch_size * P && y + i < H && x + j < W) {
            real result = out_buf[k_local][block_local][i][j];
            vstore_net_t(result, out_idx, Y);
        }
    }
}
)