#ifndef BM_SIM_THREAD_MAPPER_H_
#define BM_SIM_THREAD_MAPPER_H_

#include <cstddef>

/** WORKAROUND TO GET DIRECT ACCESS TO THE EGRESS BUFFER FROM THE TM */

struct EgressThreadMapper {
  explicit EgressThreadMapper(size_t nb_threads) : nb_threads(nb_threads) {}

  size_t operator()(size_t egress_port) const {
    return egress_port % nb_threads;
  }

  size_t nb_threads;
};

#endif  // BM_SIM_THREAD_MAPPER_H_