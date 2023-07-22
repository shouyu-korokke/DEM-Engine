//  Copyright (c) 2021, SBEL GPU Development Team
//  Copyright (c) 2021, University of Wisconsin - Madison
//
//	SPDX-License-Identifier: BSD-3-Clause

#ifndef DEME_HOST_STRUCTS
#define DEME_HOST_STRUCTS

#include <DEM/Defines.h>
#include <core/utils/ManagedAllocator.hpp>
#include <core/utils/ManagedMemory.hpp>
#include <core/utils/csv.hpp>
#include <core/utils/GpuError.h>
#include <core/utils/Timer.hpp>
#include <core/utils/RuntimeData.h>

#include <sstream>
#include <exception>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <nvmath/helper_math.cuh>
#include <DEM/HostSideHelpers.hpp>
#include <filesystem>
#include <cstring>
#include <cassert>

namespace deme {

// =============================================================================
// SOME HOST-SIDE CONSTANTS
// =============================================================================

const std::string DEME_NUM_CLUMP_NAME = std::string("NULL");
const std::string OUTPUT_FILE_X_COL_NAME = std::string("X");
const std::string OUTPUT_FILE_Y_COL_NAME = std::string("Y");
const std::string OUTPUT_FILE_Z_COL_NAME = std::string("Z");
const std::string OUTPUT_FILE_R_COL_NAME = std::string("r");
const std::string OUTPUT_FILE_CLUMP_TYPE_NAME = std::string("clump_type");
const std::filesystem::path USER_SCRIPT_PATH = RuntimeDataHelper::data_path / "kernel" / "DEMUserScripts";
// Column names for contact pair output file
const std::string OUTPUT_FILE_OWNER_1_NAME = std::string("A");
const std::string OUTPUT_FILE_OWNER_2_NAME = std::string("B");
const std::string OUTPUT_FILE_COMP_1_NAME = std::string("compA");
const std::string OUTPUT_FILE_COMP_2_NAME = std::string("compB");
const std::string OUTPUT_FILE_GEO_ID_1_NAME = std::string("geoA");
const std::string OUTPUT_FILE_GEO_ID_2_NAME = std::string("geoB");
const std::string OUTPUT_FILE_OWNER_NICKNAME_1_NAME = std::string("nameA");
const std::string OUTPUT_FILE_OWNER_NICKNAME_2_NAME = std::string("nameB");
const std::string OUTPUT_FILE_CNT_TYPE_NAME = std::string("contact_type");
const std::string OUTPUT_FILE_FORCE_X_NAME = std::string("f_x");
const std::string OUTPUT_FILE_FORCE_Y_NAME = std::string("f_y");
const std::string OUTPUT_FILE_FORCE_Z_NAME = std::string("f_z");
const std::string OUTPUT_FILE_TORQUE_X_NAME = std::string("torque_x");
const std::string OUTPUT_FILE_TORQUE_Y_NAME = std::string("torque_y");
const std::string OUTPUT_FILE_TORQUE_Z_NAME = std::string("torque_z");
const std::string OUTPUT_FILE_NORMAL_X_NAME = std::string("n_x");
const std::string OUTPUT_FILE_NORMAL_Y_NAME = std::string("n_y");
const std::string OUTPUT_FILE_NORMAL_Z_NAME = std::string("n_z");
const std::string OUTPUT_FILE_SPH_SPH_CONTACT_NAME = std::string("SS");
const std::string OUTPUT_FILE_SPH_ANAL_CONTACT_NAME = std::string("SA");
const std::string OUTPUT_FILE_SPH_MESH_CONTACT_NAME = std::string("SM");
const std::set<std::string> CNT_FILE_KNOWN_COL_NAMES = {
    OUTPUT_FILE_OWNER_1_NAME,          OUTPUT_FILE_OWNER_2_NAME,          OUTPUT_FILE_COMP_1_NAME,
    OUTPUT_FILE_COMP_2_NAME,           OUTPUT_FILE_GEO_ID_1_NAME,         OUTPUT_FILE_GEO_ID_2_NAME,
    OUTPUT_FILE_OWNER_NICKNAME_1_NAME, OUTPUT_FILE_OWNER_NICKNAME_2_NAME, OUTPUT_FILE_CNT_TYPE_NAME,
    OUTPUT_FILE_FORCE_X_NAME,          OUTPUT_FILE_FORCE_Y_NAME,          OUTPUT_FILE_FORCE_Z_NAME,
    OUTPUT_FILE_TORQUE_X_NAME,         OUTPUT_FILE_TORQUE_Y_NAME,         OUTPUT_FILE_TORQUE_Z_NAME,
    OUTPUT_FILE_NORMAL_X_NAME,         OUTPUT_FILE_NORMAL_Y_NAME,         OUTPUT_FILE_NORMAL_Z_NAME,
    OUTPUT_FILE_SPH_SPH_CONTACT_NAME,  OUTPUT_FILE_SPH_ANAL_CONTACT_NAME, OUTPUT_FILE_SPH_MESH_CONTACT_NAME};

// Map contact type identifier to their names
const std::unordered_map<contact_t, std::string> contact_type_out_name_map = {
    {NOT_A_CONTACT, "fake"},
    {SPHERE_SPHERE_CONTACT, OUTPUT_FILE_SPH_SPH_CONTACT_NAME},
    {SPHERE_MESH_CONTACT, OUTPUT_FILE_SPH_MESH_CONTACT_NAME},
    {SPHERE_PLANE_CONTACT, OUTPUT_FILE_SPH_ANAL_CONTACT_NAME},
    {SPHERE_PLATE_CONTACT, OUTPUT_FILE_SPH_ANAL_CONTACT_NAME},
    {SPHERE_CYL_CONTACT, OUTPUT_FILE_SPH_ANAL_CONTACT_NAME},
    {SPHERE_CONE_CONTACT, OUTPUT_FILE_SPH_ANAL_CONTACT_NAME}};

// Possible force model ingredients. This map is used to ensure we don't double-add them.
const std::unordered_map<std::string, bool> force_kernel_ingredient_stats = {
    {"ts", false},        {"time", false},      {"AOwnerFamily", true}, {"BOwnerFamily", true}, {"ALinVel", false},
    {"BLinVel", false},   {"ARotVel", false},   {"BRotVel", false},     {"AOwner", false},      {"BOwner", false},
    {"AOwnerMOI", false}, {"BOwnerMOI", false}, {"AGeo", false},        {"BGeo", false}};

// Structs defined here will be used by some host classes in DEM.
// NOTE: Data structs here need to be those complex ones (such as needing to include ManagedAllocator.hpp), which may
// not be jitifiable.

/// <summary>
/// DEMSolverStateData contains information that pertains the DEM solver worker threads, at a certain point in time. It
/// also contains space allocated as system scratch pad and as thread temporary arrays.
/// </summary>
class DEMSolverStateData {
  private:
    const unsigned int numTempArrays;
    // The vector used by CUB or by anybody else that needs scratch space.
    // Please pay attention to the type the vector stores.
    std::vector<scratch_t, ManagedAllocator<scratch_t>> cubScratchSpace;

    // The vectors used by threads when they need temporary arrays (very typically, for storing arrays outputted by cub
    // scan or reduce operations).
    std::vector<std::vector<scratch_t, ManagedAllocator<scratch_t>>,
                ManagedAllocator<std::vector<scratch_t, ManagedAllocator<scratch_t>>>>
        threadTempVectors;
    // You can keep more temp arrays if you construct this class with a different initializer

  public:
    // Temp size_t variables that can be reused
    size_t* pTempSizeVar1;
    size_t* pTempSizeVar2;
    size_t* pTempSizeVar3;

    // Number of contacts in this CD step
    size_t* pNumContacts;
    // Number of contacts in the previous CD step
    size_t* pNumPrevContacts;
    // Number of spheres in the previous CD step (in case user added/removed clumps from the system)
    size_t* pNumPrevSpheres;

    DEMSolverStateData(unsigned int nArrays) : numTempArrays(nArrays) {
        DEME_GPU_CALL(cudaMallocManaged(&pNumContacts, sizeof(size_t)));
        DEME_GPU_CALL(cudaMallocManaged(&pTempSizeVar1, sizeof(size_t)));
        DEME_GPU_CALL(cudaMallocManaged(&pTempSizeVar2, sizeof(size_t)));
        DEME_GPU_CALL(cudaMallocManaged(&pTempSizeVar3, sizeof(size_t)));
        DEME_GPU_CALL(cudaMallocManaged(&pNumPrevContacts, sizeof(size_t)));
        DEME_GPU_CALL(cudaMallocManaged(&pNumPrevSpheres, sizeof(size_t)));
        *pNumContacts = 0;
        *pNumPrevContacts = 0;
        *pNumPrevSpheres = 0;
        threadTempVectors.resize(numTempArrays);
    }
    ~DEMSolverStateData() {
        DEME_GPU_CALL(cudaFree(pNumContacts));
        DEME_GPU_CALL(cudaFree(pTempSizeVar1));
        DEME_GPU_CALL(cudaFree(pTempSizeVar2));
        DEME_GPU_CALL(cudaFree(pTempSizeVar3));
        DEME_GPU_CALL(cudaFree(pNumPrevContacts));
        DEME_GPU_CALL(cudaFree(pNumPrevSpheres));

        cubScratchSpace.clear();
        for (unsigned int i = 0; i < numTempArrays; i++) {
            threadTempVectors.at(i).clear();
        }
        threadTempVectors.clear();
    }

    // Return raw pointer to swath of device memory that is at least "sizeNeeded" large
    inline scratch_t* allocateScratchSpace(size_t sizeNeeded) {
        if (cubScratchSpace.size() < sizeNeeded) {
            cubScratchSpace.resize(sizeNeeded);
        }
        return cubScratchSpace.data();
    }

    inline scratch_t* allocateTempVector(unsigned int i, size_t sizeNeeded) {
        if (threadTempVectors.at(i).size() < sizeNeeded) {
            threadTempVectors.at(i).resize(sizeNeeded);
        }
        return threadTempVectors.at(i).data();
    }
};

struct kTStateParams {
    // The `top speed' of the change of bin size
    float binTopChangeRate = 0.05;
    // The `current speed' fo the change of bin size
    float binCurrentChangeRate = 0.0;
    // The `acceleration' of bin size change rate, (0, 1]: 1 means each time a change is applied, it's at top speed
    float binChangeRateAcc = 0.1;
    // Number of CD steps before the solver makes a decision on how to change the bin size
    unsigned int binChangeObserveSteps = 5;
    // Past the point that (this number * error out bin geometry count)-many geometries found in a bin, the solver will
    // force the bin to shrink
    float binChangeUpperSafety = 0.5;
    // Past the point that (this number * max num of bin)-many bins in the domain, the solver will force the bin to
    // expand
    float binChangeLowerSafety = 0.85;

    // The max num of geometries in a bin that appeared in the CD process
    size_t maxSphFoundInBin;
    size_t maxTriFoundInBin;

    // Num of bins, currently
    size_t numBins = 0;

    // Current average num of contacts per sphere has.
    float avgCntsPerSphere = 0.;
};

inline std::string pretty_format_bytes(size_t bytes) {
    // set up byte prefixes
    constexpr size_t KIBI = 1024;
    constexpr size_t MEBI = KIBI * KIBI;
    constexpr size_t GIBI = KIBI * KIBI * KIBI;
    float gibival = float(bytes) / GIBI;
    float mebival = float(bytes) / MEBI;
    float kibival = float(bytes) / KIBI;
    std::stringstream ret;
    if (gibival > 1) {
        ret << gibival << " GiB";
    } else if (mebival > 1) {
        ret << mebival << " MiB";
    } else if (kibival > 1) {
        ret << kibival << " KiB";
    } else {
        ret << bytes << " B";
    }
    return ret.str();
}

// =============================================================================
// SOME HOST-SIDE ENUMS
// =============================================================================

// Types of entities (can be either owner or geometry entity) that can be inspected by inspection methods
enum class INSPECT_ENTITY_TYPE { SPHERE, CLUMP, MESH, MESH_FACET, EVERYTHING };
// Which reduce operation is needed in an inspection
enum class CUB_REDUCE_FLAVOR { NONE, MAX, MIN, SUM };
// Format of the output files
enum class OUTPUT_FORMAT { CSV, BINARY, CHPF };
// Mesh output format
enum class MESH_FORMAT { VTK, OBJ };
// Adaptive time step size methods
enum class ADAPT_TS_TYPE { NONE, MAX_VEL, INT_DIFF };

// =============================================================================
// NOW DEFINING MACRO COMMANDS USED BY THE DEM MODULE
// =============================================================================

#define DEME_PRINTF(...)                    \
    {                                       \
        if (verbosity > VERBOSITY::QUIET) { \
            printf(__VA_ARGS__);            \
        }                                   \
    }

#define DEME_ERROR(...)                      \
    {                                        \
        char error_message[1024];            \
        sprintf(error_message, __VA_ARGS__); \
        std::string out = error_message;     \
        out += "\n";                         \
        out += "This happened in ";          \
        out += __func__;                     \
        out += ".\n";                        \
        throw std::runtime_error(out);       \
    }

#define DEME_WARNING(...)                       \
    {                                           \
        if (verbosity >= VERBOSITY::WARNING) {  \
            char warn_message[1024];            \
            sprintf(warn_message, __VA_ARGS__); \
            std::string out = "\nWARNING! ";    \
            out += warn_message;                \
            out += "\n\n";                      \
            std::cerr << out;                   \
        }                                       \
    }

#define DEME_INFO(...)                      \
    {                                       \
        if (verbosity >= VERBOSITY::INFO) { \
            printf(__VA_ARGS__);            \
            printf("\n");                   \
        }                                   \
    }

#define DEME_STEP_ANOMALY(...)                                        \
    {                                                                 \
        if (verbosity >= VERBOSITY::STEP_ANOMALY) {                   \
            char warn_message[1024];                                  \
            sprintf(warn_message, __VA_ARGS__);                       \
            std::string out = "\n-------- SIM ANOMALY!!! --------\n"; \
            out += warn_message;                                      \
            out += "\n\n";                                            \
            std::cerr << out;                                         \
        }                                                             \
    }

#define DEME_STEP_METRIC(...)                      \
    {                                              \
        if (verbosity >= VERBOSITY::STEP_METRIC) { \
            printf(__VA_ARGS__);                   \
            printf("\n");                          \
        }                                          \
    }

#define DEME_DEBUG_PRINTF(...)               \
    {                                        \
        if (verbosity >= VERBOSITY::DEBUG) { \
            printf(__VA_ARGS__);             \
            printf("\n");                    \
        }                                    \
    }

#define DEME_DEBUG_EXEC(...)                 \
    {                                        \
        if (verbosity >= VERBOSITY::DEBUG) { \
            __VA_ARGS__;                     \
        }                                    \
    }

#define DEME_STEP_DEBUG_PRINTF(...)               \
    {                                             \
        if (verbosity >= VERBOSITY::STEP_DEBUG) { \
            printf(__VA_ARGS__);                  \
            printf("\n");                         \
        }                                         \
    }

#define DEME_STEP_DEBUG_EXEC(...)                 \
    {                                             \
        if (verbosity >= VERBOSITY::STEP_DEBUG) { \
            __VA_ARGS__;                          \
        }                                         \
    }

// Jitify options include suppressing variable-not-used warnings, but since we don't use CUB block primitives, we don't
// need std::string(CUDA_TOOLKIT_HEADERS).
#define DEME_JITIFY_OPTIONS                                                                       \
    {                                                                                             \
        "-I" + (JitHelper::KERNEL_INCLUDE_DIR).string(), "-I" + (JitHelper::KERNEL_DIR).string(), \
            "-diag-suppress=550", "-diag-suppress=177"                                            \
    }

// I wasn't able to resolve a decltype problem with vector of vectors, so I have to create another macro for this kind
// of tracked resize... not ideal.
#define DEME_TRACKED_RESIZE_FLOAT(vec, newsize, val)               \
    {                                                              \
        size_t old_size = vec.size();                              \
        vec.resize(newsize, val);                                  \
        size_t new_size = vec.size();                              \
        size_t byte_delta = sizeof(float) * (new_size - old_size); \
        m_approx_bytes_used += byte_delta;                         \
    }

#define DEME_TRACKED_RESIZE(vec, newsize, val)                 \
    {                                                          \
        size_t item_size = sizeof(decltype(vec)::value_type);  \
        size_t old_size = vec.size();                          \
        vec.resize(newsize, val);                              \
        size_t new_size = vec.size();                          \
        size_t byte_delta = item_size * (new_size - old_size); \
        m_approx_bytes_used += byte_delta;                     \
    }

#define DEME_TRACKED_RESIZE_DEBUGPRINT(vec, newsize, name, val)                                                      \
    {                                                                                                                \
        size_t item_size = sizeof(decltype(vec)::value_type);                                                        \
        size_t old_size = vec.size();                                                                                \
        vec.resize(newsize, val);                                                                                    \
        size_t new_size = vec.size();                                                                                \
        size_t byte_delta = item_size * (new_size - old_size);                                                       \
        m_approx_bytes_used += byte_delta;                                                                           \
        DEME_DEBUG_PRINTF("Resizing vector %s, old size %zu, new size %zu, byte delta %s", name, old_size, new_size, \
                          pretty_format_bytes(byte_delta).c_str());                                                  \
    }

//// TODO: this is currently not tracked...
// ptr being a reference to a pointer is crucial
template <typename T>
inline void DEME_DEVICE_PTR_ALLOC(T*& ptr, size_t size) {
    cudaPointerAttributes attrib;
    DEME_GPU_CALL(cudaPointerGetAttributes(&attrib, ptr));

    if (attrib.type != cudaMemoryType::cudaMemoryTypeUnregistered)
        DEME_GPU_CALL(cudaFree(ptr));
    DEME_GPU_CALL(cudaMalloc((void**)&ptr, size * sizeof(T)));
}

// Managed advise doesn't seem to do anything...
#define DEME_ADVISE_DEVICE(vec, device) \
    { advise(vec, ManagedAdvice::PREFERRED_LOC, device); }
#define DEME_MIGRATE_TO_DEVICE(vec, device, stream) \
    { migrate(vec, device, stream); }

// Use (void) to silence unused warnings.
#define assertm(exp, msg) assert(((void)msg, exp))

// #define OUTPUT_IF_GPU_FAILS(res) \
//     { gpu_assert((res), __FILE__, __LINE__, false); throw std::runtime_error("GPU Assertion Failed!");}

// =============================================================================
// NOW SOME HOST-SIDE SIMPLE STRUCTS USED BY THE DEM MODULE
// =============================================================================

// Anomalies log
class WorkerAnomalies {
  public:
    WorkerAnomalies() {}

    bool over_max_vel = false;

    void Clear() { over_max_vel = false; }
};

// Timers used by kT and dT
class SolverTimers {
  private:
    const unsigned int num_timers;
    std::unordered_map<std::string, Timer<double>> m_timers;

  public:
    SolverTimers(const std::vector<std::string>& names) : num_timers(names.size()) {
        for (unsigned int i = 0; i < num_timers; i++) {
            m_timers[names.at(i)] = Timer<double>();
        }
    }
    Timer<double>& GetTimer(const std::string& name) { return m_timers.at(name); }
};

// Manager of the collabortation between the main thread and worker threads
class WorkerReportChannel {
  public:
    std::atomic<bool> userCallDone;
    std::mutex mainCanProceed;
    std::condition_variable cv_mainCanProceed;
    WorkerReportChannel() noexcept { userCallDone = false; }
    ~WorkerReportChannel() {}
};

struct familyPrescription_t {
    unsigned int family;
    std::string linPosX = "none";
    std::string linPosY = "none";
    std::string linPosZ = "none";
    std::string linVelX = "none";
    std::string linVelY = "none";
    std::string linVelZ = "none";

    std::string oriQ = "none";
    std::string rotVelX = "none";
    std::string rotVelY = "none";
    std::string rotVelZ = "none";
    // Is this prescribed motion dictating the motion of the entities (true), or should still accept the influence from
    // other contact forces (false)
    bool linVelXPrescribed = false;
    bool linVelYPrescribed = false;
    bool linVelZPrescribed = false;
    bool rotVelXPrescribed = false;
    bool rotVelYPrescribed = false;
    bool rotVelZPrescribed = false;
    bool rotPosPrescribed = false;
    bool linPosPrescribed = false;
    // Prescribed acc and ang acc; they are added to entities, sort of like gravity
    std::string accX = "none";
    std::string accY = "none";
    std::string accZ = "none";
    std::string angAccX = "none";
    std::string angAccY = "none";
    std::string angAccZ = "none";
    // A switch to mark if there is any prescription going on for this family at all
    bool used = false;
};

struct familyPair_t {
    unsigned int ID1;
    unsigned int ID2;
};

enum class VAR_TS_STRAT { CONST, MAX_VEL, INT_GAP };

class ClumpTemplateFlatten {
  public:
    std::vector<float>& mass;
    std::vector<float3>& MOI;
    std::vector<std::vector<unsigned int>>& matIDs;
    std::vector<std::vector<float>>& spRadii;
    std::vector<std::vector<float3>>& spRelPos;
    std::vector<float>& volume;

    ClumpTemplateFlatten(std::vector<float>& ref_mass,
                         std::vector<float3>& ref_MOI,
                         std::vector<std::vector<unsigned int>>& ref_matIDs,
                         std::vector<std::vector<float>>& ref_spRadii,
                         std::vector<std::vector<float3>>& ref_spRelPos,
                         std::vector<float>& ref_volume)
        : mass(ref_mass),
          MOI(ref_MOI),
          matIDs(ref_matIDs),
          spRadii(ref_spRadii),
          spRelPos(ref_spRelPos),
          volume(ref_volume) {}
    ~ClumpTemplateFlatten() {}
};

struct SolverFlags {
    // Sort contact pair arrays (based on contact type) before sending to dT
    bool should_sort_pairs = true;
    // This run is historyless
    bool isHistoryless = false;
    // This run uses contact detection in an async fashion (kT and dT working at different points in simulation time)
    bool isAsync = true;
    // If family number can potentially change (at each time step) during the simulation, because of user intervention
    bool canFamilyChange = false;
    // If mesh will deform in the next kT-update cycle
    std::atomic<bool> willMeshDeform = false;
    // Some output-related flags
    unsigned int outputFlags = OUTPUT_CONTENT::QUAT | OUTPUT_CONTENT::ABSV;
    unsigned int cntOutFlags;
    // Time step constant-ness and expand factor constant-ness
    bool isStepConst = true;
    bool isExpandFactorFixed = false;
    // The strategy for selecting the variable time step size
    VAR_TS_STRAT stepSizeStrat = VAR_TS_STRAT::CONST;
    // Whether instructed to use jitification for mass properties and clump components (default to no and it is
    // recommended)
    bool useClumpJitify = false;
    bool useMassJitify = false;
    // Whether the simulation involves meshes
    bool hasMeshes = false;
    // Whether the force collection (acceleration calc and reduction) process should be using CUB
    bool useCubForceCollect = false;
    // Does not record contact forces, contact point etc.
    bool useNoContactRecord = false;
    // Collect force (reduce to acc) right in the force calculation kernel
    bool useForceCollectInPlace = false;
    // Max number of steps dT is allowed to be ahead of kT, even when auto-adapt is enabled
    unsigned int upperBoundFutureDrift = 5000;
    // (targetDriftMoreThanAvg + targetDriftMultipleOfAvg * actual_dT_steps_per_kT_step) is used to calculate contact
    // margin size
    float targetDriftMoreThanAvg = 4.;
    float targetDriftMultipleOfAvg = 1.1;

    // Whether the solver auto-update those sim params
    bool autoBinSize = true;
    bool autoUpdateFreq = true;

    // The max number of average contacts per sphere has before the solver errors out. The reason why I didn't use the
    // number of contacts for the sphere that has the most is that, well, we can have a huge sphere and it just will
    // have more contacts. But if avg cnt is high, that means probably the contact margin is out of control now.
    float errOutAvgSphCnts = 100.;
};

class DEMMaterial {
  public:
    // Material name--value pairs
    std::unordered_map<std::string, float> mat_prop;

    DEMMaterial(const std::unordered_map<std::string, float>& prop) { mat_prop = prop; }
    ~DEMMaterial() {}
    // float E = 1e8;    // Young's modulus
    // float nu = 0.3;   // Poission's ratio
    // float CoR = 0.5;  // Coeff of Restitution
    // float mu = 0.5;   // Static friction coeff
    // float Crr = 0.0;  // Rolling resistance coeff

    // Its offset when this obj got loaded into the API-level user raw-input array
    unsigned int load_order;
};

class DEMTriangle {
  public:
    DEMTriangle(float3 pnt1, float3 pnt2, float3 pnt3) : p1(pnt1), p2(pnt2), p3(pnt3) {}
    DEMTriangle() {}
    ~DEMTriangle() {}
    float3 p1;
    float3 p2;
    float3 p3;
};

// A struct that defines a `clump' (one of the core concepts of this solver). A clump is typically small which consists
// of several sphere components, but it can be as large as having thousands of spheres.
class DEMClumpTemplate {
  private:
    void assertLength(size_t len, const std::string name) {
        if (nComp == 0) {
            std::cerr << "The settings at the " << name
                      << " call were applied to 0 sphere components.\nPlease consider using " << name
                      << " only after loading the clump template." << std::endl;
        }
        if (len != nComp) {
            std::stringstream ss;
            ss << name << " input argument must have length " << nComp << " (not " << len
               << "), same as the number of sphere components in the clump template." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

  public:
    float mass = 0;
    float3 MOI = make_float3(0);
    std::vector<float> radii;
    std::vector<float3> relPos;
    std::vector<std::shared_ptr<DEMMaterial>> materials;
    unsigned int nComp = 0;  // Number of components

    /// Set mass.
    void SetMass(float mass) { this->mass = mass; }
    /// Set MOI (in principal frame).
    void SetMOI(float3 MOI) { this->MOI = MOI; }
    /// Set MOI (in principal frame).
    void SetMOI(const std::vector<float>& MOI) {
        assertThreeElements(MOI, "SetMOI", "MOI");
        SetMOI(host_make_float3(MOI[0], MOI[1], MOI[2]));
    }

    /// Set material types for the mesh. Technically, you can set that for each individual mesh facet.
    void SetMaterial(const std::vector<std::shared_ptr<DEMMaterial>>& input) {
        assertLength(input.size(), "SetMaterial");
        materials = input;
    }
    /// Set material types for the mesh. Technically, you can set that for each individual mesh facet.
    void SetMaterial(const std::shared_ptr<DEMMaterial>& input) {
        SetMaterial(std::vector<std::shared_ptr<DEMMaterial>>(nComp, input));
    }

    // Position of this clump's CoM, in the frame which is used to report the positions of this clump's component
    // spheres. It is usually all 0, unless the user specifies it, in which case we need to process relPos such that
    // when the system is initialized, everything is still in the clump's CoM frame.
    // float3 CoM = make_float3(0);
    // CoM frame's orientation quaternion in the frame which is used to report the positions of this clump's component
    // spheres. Usually unit quaternion.
    // float4 CoM_oriQ = host_make_float4(0, 0, 0, 1);

    // Each clump template will have a unique mark number. When clumps are loaded to the system, this mark will help
    // find their type offset.
    unsigned int mark;
    // Whether this is a big clump (not used; jitifiability is determined automatically)
    bool isBigClump = false;
    // A name given by the user. It will be outputted to file to indicate the type of a clump.
    std::string m_name = DEME_NUM_CLUMP_NAME;
    // The volume of this type of clump.
    //// TODO: Add a method to automatically compute its volume
    float volume = 0.0;

    /// Set the volume of this clump template. It is needed before you query the void ratio.
    void SetVolume(float vol) { volume = vol; }

    /// Retrieve clump's sphere component information from a file
    int ReadComponentFromFile(const std::string filename,
                              const std::string x_id = "x",
                              const std::string y_id = "y",
                              const std::string z_id = "z",
                              const std::string r_id = "r") {
        io::CSVReader<4> in(filename);
        in.read_header(io::ignore_missing_column, x_id, y_id, z_id, r_id);
        float r = 0;
        float3 pos;
        unsigned int count = 0;
        while (in.read_row(pos.x, pos.y, pos.z, r)) {
            radii.push_back(r);
            relPos.push_back(pos);
            count++;
        }
        nComp += count;

        //// TODO: If there is an error while loading, we should report it
        return 0;
    }

    /// If this clump's component sphere relPos is not reported by the user in its CoM frame, then the user needs to
    /// call this method immediately to report this clump's Volume Centroid and Principal Axes, and relPos will be
    /// adjusted by this call.
    void InformCentroidPrincipal(float3 center, float4 prin_Q) {
        // Getting to Centroid and Principal is a translation then a rotation (local), so the undo order to undo
        // transltion then rotation
        for (auto& pos : relPos) {
            applyFrameTransformGlobalToLocal(pos, center, prin_Q);
        }
    }
    void InformCentroidPrincipal(const std::vector<float>& center, const std::vector<float>& prin_Q) {
        assertThreeElements(center, "InformCentroidPrincipal", "center");
        assertFourElements(prin_Q, "InformCentroidPrincipal", "prin_Q");
        InformCentroidPrincipal(host_make_float3(center[0], center[1], center[2]),
                                host_make_float4(prin_Q[0], prin_Q[1], prin_Q[2], prin_Q[3]));
    }

    /// The opposite of InformCentroidPrincipal, and it is another way to align this clump's coordinate system with its
    /// centroid and principal system: rotate then move this clump, so that at the end of this operation, the original
    /// `origin' point should hit the CoM of this clump.
    void Move(float3 vec, float4 rot_Q) {
        for (auto& pos : relPos) {
            applyFrameTransformLocalToGlobal(pos, vec, rot_Q);
        }
    }
    void Move(const std::vector<float>& vec, const std::vector<float>& rot_Q) {
        assertThreeElements(vec, "Move", "vec");
        assertFourElements(rot_Q, "Move", "rot_Q");
        Move(host_make_float3(vec[0], vec[1], vec[2]), host_make_float4(rot_Q[0], rot_Q[1], rot_Q[2], rot_Q[3]));
    }

    /// Scale all geometry component of this clump
    void Scale(float s) {
        for (auto& pos : relPos) {
            pos *= s;
        }
        for (auto& rad : radii) {
            rad *= s;
        }
        mass *= (double)s * (double)s * (double)s;
        MOI *= (double)s * (double)s * (double)s * (double)s * (double)s;
        volume *= (double)s * (double)s * (double)s;
    }

    void AssignName(const std::string& some_name) { m_name = some_name; }
};

// Initializer includes batch of clumps, a mesh, a analytical object, and a tracked object. But this parent class is
// small, and is mainly there for the purpose of pyDEME entry point.
class DEMInitializer {
  public:
    // The type of a clump batch is CLUMP (it is used by tracker objs)
    OWNER_TYPE obj_type;
    // Its offset when this obj got loaded into the API-level user raw-input array
    unsigned int load_order;
};

// API-(Host-)side struct that holds cached user-input batches of clumps
class DEMClumpBatch : public DEMInitializer {
  private:
    size_t nExistContacts = 0;
    void assertLength(size_t len, const std::string name) {
        if (len != nClumps) {
            std::stringstream ss;
            ss << name << " input argument must have length " << nClumps << " (not " << len
               << "), same as the number of clumps you originally added via AddClumps." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

  public:
    size_t nClumps = 0;
    size_t nSpheres = 0;
    bool family_isSpecified = false;

    std::vector<std::shared_ptr<DEMClumpTemplate>> types;
    std::vector<unsigned int> families;
    std::vector<float3> vel;
    std::vector<float3> angVel;
    std::vector<float3> xyz;
    std::vector<float4> oriQ;
    // Existing contact/contact wildcard info. If it is a new simulation, they should be empty; but if it is a restarted
    // one, it can have some existing contacts/wildcards. Note that all of them are "SS" type of contact. The contact
    // pair IDs are relative to this batch (starting from 0, up to num of this batch - 1, that is).
    std::vector<std::pair<bodyID_t, bodyID_t>> contact_pairs;
    std::unordered_map<std::string, std::vector<float>> contact_wildcards;
    // Initial owner wildcard that this batch of clumps should have
    std::unordered_map<std::string, std::vector<float>> owner_wildcards;
    // Initial geometry wildcard that this batch of clumps should have
    std::unordered_map<std::string, std::vector<float>> geo_wildcards;

    DEMClumpBatch(size_t num) : nClumps(num) {
        types.resize(num);
        families.resize(num, DEFAULT_CLUMP_FAMILY_NUM);
        vel.resize(num, make_float3(0));
        angVel.resize(num, make_float3(0));
        xyz.resize(num);
        oriQ.resize(num, host_make_float4(0, 0, 0, 1));
        obj_type = OWNER_TYPE::CLUMP;
    }
    ~DEMClumpBatch() {}
    size_t GetNumClumps() const { return nClumps; }
    size_t GetNumSpheres() const { return nSpheres; }

    void SetTypes(const std::vector<std::shared_ptr<DEMClumpTemplate>>& input) {
        assertLength(input.size(), "SetTypes");
        types = input;
    }
    void SetTypes(const std::shared_ptr<DEMClumpTemplate>& input) {
        SetTypes(std::vector<std::shared_ptr<DEMClumpTemplate>>(nClumps, input));
    }
    void SetType(const std::shared_ptr<DEMClumpTemplate>& input) {
        SetTypes(std::vector<std::shared_ptr<DEMClumpTemplate>>(nClumps, input));
    }

    void SetPos(const std::vector<float3>& input) {
        assertLength(input.size(), "SetPos");
        xyz = input;
    }
    void SetPos(float3 input) { SetPos(std::vector<float3>(nClumps, input)); }
    void SetPos(const std::vector<float>& input) {
        assertThreeElements(input, "SetPos", "input");
        SetPos(host_make_float3(input[0], input[1], input[2]));
    }
    void SetPos(const std::vector<std::vector<float>>& input) {
        assertThreeElementsVector(input, "SetPos", "input");
        std::vector<float3> pos_xyz(input.size());
        for (size_t i = 0; i < input.size(); i++) {
            pos_xyz[i] = host_make_float3(input[i][0], input[i][1], input[i][2]);
        }
        SetPos(pos_xyz);
    }

    void SetVel(const std::vector<float3>& input) {
        assertLength(input.size(), "SetVel");
        vel = input;
    }
    void SetVel(float3 input) { SetVel(std::vector<float3>(nClumps, input)); }
    void SetVel(const std::vector<float>& input) {
        assertThreeElements(input, "SetVel", "input");
        SetVel(host_make_float3(input[0], input[1], input[2]));
    }
    void SetVel(const std::vector<std::vector<float>>& input) {
        assertThreeElementsVector(input, "SetVel", "input");
        std::vector<float3> vel_xyz(input.size());
        for (size_t i = 0; i < input.size(); i++) {
            vel_xyz[i] = host_make_float3(input[i][0], input[i][1], input[i][2]);
        }
        SetVel(vel_xyz);
    }

    void SetAngVel(const std::vector<float3>& input) {
        assertLength(input.size(), "SetAngVel");
        angVel = input;
    }
    void SetAngVel(float3 input) { SetAngVel(std::vector<float3>(nClumps, input)); }
    void SetAngVel(const std::vector<float>& input) {
        assertThreeElements(input, "SetAngVel", "input");
        SetAngVel(host_make_float3(input[0], input[1], input[2]));
    }
    void SetAngVel(const std::vector<std::vector<float>>& input) {
        assertThreeElementsVector(input, "SetAngVel", "input");
        std::vector<float3> vel_xyz(input.size());
        for (size_t i = 0; i < input.size(); i++) {
            vel_xyz[i] = host_make_float3(input[i][0], input[i][1], input[i][2]);
        }
        SetAngVel(vel_xyz);
    }

    void SetOriQ(const std::vector<float4>& input) {
        assertLength(input.size(), "SetOriQ");
        oriQ = input;
    }
    void SetOriQ(float4 input) { SetOriQ(std::vector<float4>(nClumps, input)); }
    void SetOriQ(const std::vector<float>& input) {
        assertFourElements(input, "SetOriQ", "input");
        SetOriQ(host_make_float4(input[0], input[1], input[2], input[3]));
    }
    void SetOriQ(const std::vector<std::vector<float>>& input) {
        assertFourElementsVector(input, "SetOriQ", "input");
        std::vector<float4> Q(input.size());
        for (size_t i = 0; i < input.size(); i++) {
            Q[i] = host_make_float4(input[i][0], input[i][1], input[i][2], input[i][3]);
        }
        SetOriQ(Q);
    }

    /// Specify the `family' code for each clump. Then you can specify if they should go with some prescribed motion or
    /// some special physics (for example, being fixed). The default behavior (without specification) for every family
    /// is using `normal' physics.
    void SetFamilies(const std::vector<unsigned int>& input) {
        assertLength(input.size(), "SetFamilies");
        if (any_of(input.begin(), input.end(),
                   [](unsigned int i) { return i > std::numeric_limits<family_t>::max(); })) {
            std::stringstream ss;
            ss << "Some clumps are instructed to have a family number larger than the max allowance "
               << std::numeric_limits<family_t>::max() << std::endl;
            throw std::runtime_error(ss.str());
        }
        families = input;
        family_isSpecified = true;
    }
    void SetFamilies(unsigned int input) { SetFamilies(std::vector<unsigned int>(nClumps, input)); }
    void SetFamily(unsigned int input) { SetFamilies(std::vector<unsigned int>(nClumps, input)); }
    void SetExistingContacts(const std::vector<std::pair<bodyID_t, bodyID_t>>& pairs) {
        contact_pairs = pairs;
        nExistContacts = pairs.size();
    }
    void SetExistingContactWildcards(const std::unordered_map<std::string, std::vector<float>>& wildcards) {
        if (wildcards.begin()->second.size() != nExistContacts) {
            std::stringstream ss;
            ss << "SetExistingContactWildcards needs to be called after SetExistingContacts, with each wildcard array "
                  "having the same length as the number of contact pairs.\nThis way, each wildcard will have an "
                  "associated contact pair."
               << std::endl;
            throw std::runtime_error(ss.str());
        }
        contact_wildcards = wildcards;
    }
    void AddExistingContactWildcard(const std::string& name, const std::vector<float>& vals) {
        if (vals.size() != nClumps) {
            std::stringstream ss;
            ss << "AddExistingContactWildcard needs to be called after SetExistingContacts, with the input wildcard "
                  "array having the same length as the number of contact pairs.\nThis way, each wildcard will have an "
                  "associated contact pair."
               << std::endl;
            throw std::runtime_error(ss.str());
        }
        contact_wildcards[name] = vals;
    }

    void SetOwnerWildcards(const std::unordered_map<std::string, std::vector<float>>& wildcards) {
        if (wildcards.begin()->second.size() != nClumps) {
            std::stringstream ss;
            ss << "Input owner wildcard arrays in a SetOwnerWildcards call must all have the same size as the number "
                  "of clumps in this batch.\nHere, the input array has length "
               << wildcards.begin()->second.size() << " but this batch has " << nClumps << " clumps." << std::endl;
            throw std::runtime_error(ss.str());
        }
        owner_wildcards = wildcards;
    }
    void AddOwnerWildcard(const std::string& name, const std::vector<float>& vals) {
        if (vals.size() != nClumps) {
            std::stringstream ss;
            ss << "Input owner wildcard array in a AddOwnerWildcard call must have the same size as the number of "
                  "clumps in this batch.\nHere, the input array has length "
               << vals.size() << " but this batch has " << nClumps << " clumps." << std::endl;
            throw std::runtime_error(ss.str());
        }
        owner_wildcards[name] = vals;
    }
    void AddOwnerWildcard(const std::string& name, float val) {
        AddOwnerWildcard(name, std::vector<float>(nClumps, val));
    }

    void SetGeometryWildcards(const std::unordered_map<std::string, std::vector<float>>& wildcards) {
        if (wildcards.begin()->second.size() != nSpheres) {
            std::stringstream ss;
            ss << "Input gemometry wildcard arrays in a SetGeometryWildcards call must all have the same size as the "
                  "number of spheres in this batch.\nHere, the input array has length "
               << wildcards.begin()->second.size() << " but this batch has " << nSpheres << " spheres." << std::endl;
            throw std::runtime_error(ss.str());
        }
        geo_wildcards = wildcards;
    }
    void AddGeometryWildcard(const std::string& name, const std::vector<float>& vals) {
        if (vals.size() != nSpheres) {
            std::stringstream ss;
            ss << "Input gemometry wildcard array in a AddGeometryWildcard call must have the same size as the number "
                  "of spheres in this batch.\nHere, the input array has length "
               << vals.size() << " but this batch has " << nSpheres << " spheres." << std::endl;
            throw std::runtime_error(ss.str());
        }
        geo_wildcards[name] = vals;
    }
    void AddGeometryWildcard(const std::string& name, float val) {
        AddGeometryWildcard(name, std::vector<float>(nSpheres, val));
    }

    size_t GetNumContacts() const { return nExistContacts; }
};

// A struct to get or set tracked owner entities
class DEMTrackedObj : public DEMInitializer {
  public:
    DEMTrackedObj() {}
    ~DEMTrackedObj() {}

    // ownerID will be updated by dT on initialization
    bodyID_t ownerID = NULL_BODYID;
    // Number of owners that are covered by this tracker. This exists because if you track a batch of clumps, ownerID is
    // but the first owner of that batch.
    size_t nSpanOwners = 1;
    // If this tracked object is broken b/c the owner it points to has been removed from the simulation system
    bool isBroken = false;
    // The offset for its first geometric compoent in the tracked objects. For example, if it is mesh, then this is the
    // first triangle ID.
    size_t geoID;
    // The number of geometric entities (sphere components, triangles or analytical components) the tracked objects
    // have.
    size_t nGeos;
};

}  // namespace deme

#endif
