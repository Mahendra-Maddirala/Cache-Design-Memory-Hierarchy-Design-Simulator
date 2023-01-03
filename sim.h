#ifndef SIM_CACHE_H
#define SIM_CACHE_H
#define ll uint32_t

typedef 
struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

typedef
struct{
   ll valid;
   ll tag;
   ll rank; //for lru purpose
   ll dirty;
}blocks;
// Put additional data structures here as per your requirement.

#endif
