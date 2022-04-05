#ifndef SGPS_SPH_CUB_HELPERS
#define SGPS_SPH_CUB_HELPERS

#include <core/utils/ManagedAllocator.hpp>
#include <vector>

void PrefixScanExclusiveCub(std::vector<int, sgps::ManagedAllocator<int>>& d_in,
                            std::vector<int, sgps::ManagedAllocator<int>>& d_out);

void PairRadixSortAscendCub(std::vector<int, sgps::ManagedAllocator<int>>& d_keys_in,
                            std::vector<int, sgps::ManagedAllocator<int>>& d_keys_out,
                            std::vector<int, sgps::ManagedAllocator<int>>& d_values_in,
                            std::vector<int, sgps::ManagedAllocator<int>>& d_values_out);

void PairRadixSortAscendCub(std::vector<int, sgps::ManagedAllocator<int>>& d_keys_in,
                            std::vector<int, sgps::ManagedAllocator<int>>& d_keys_out,
                            std::vector<float, sgps::ManagedAllocator<float>>& d_values_in,
                            std::vector<float, sgps::ManagedAllocator<float>>& d_values_out);

void RunLengthEncodeCub(std::vector<int, sgps::ManagedAllocator<int>>& d_in,
                        std::vector<int, sgps::ManagedAllocator<int>>& d_unique_out,
                        std::vector<int, sgps::ManagedAllocator<int>>& d_counts_out);

void SumReduceByKeyCub(std::vector<int, sgps::ManagedAllocator<int>>& d_keys_in,
                       std::vector<int, sgps::ManagedAllocator<int>>& d_uniques_out,
                       std::vector<float, sgps::ManagedAllocator<float>>& d_values_in,
                       std::vector<float, sgps::ManagedAllocator<float>>& d_aggregates_out);

#endif