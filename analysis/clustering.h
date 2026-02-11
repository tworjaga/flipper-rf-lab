#ifndef CLUSTERING_H
#define CLUSTERING_H

#include "../core/flipper_rf_lab.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SIGNAL CLUSTERING ENGINE
// K-means and hierarchical clustering for signal classification
// ============================================================================

#define KMEANS_MAX_K            5       // Maximum clusters
#define KMEANS_MAX_ITERATIONS   100     // Max iterations before forced convergence
#define KMEANS_CONVERGENCE      5       // Stop when movement < 0.5% (in fixed-point)
#define DTW_MAX_LENGTH          128     // Max sequence length for DTW

// Distance metric types
typedef enum {
    DISTANCE_EUCLIDEAN = 0,
    DISTANCE_MANHATTAN,
    DISTANCE_COSINE,
    DISTANCE_DTW
} DistanceMetric_t;

// Cluster quality metrics
typedef enum {
    QUALITY_SILHOUETTE = 0,
    QUALITY_CALINSKI_HARABASZ,
    QUALITY_DAVIES_BOULDIN
} QualityMetric_t;

// ============================================================================
// K-MEANS CLUSTERING
// ============================================================================

typedef struct {
    fixed_t x;              // Feature 1: pulse width (fixed-point)
    fixed_t y;              // Feature 2: amplitude or other feature
    uint8_t cluster_id;     // Assigned cluster
    uint8_t frame_id;       // Source frame reference
} DataPoint_t;

typedef struct {
    DataPoint_t points[MAX_PULSE_COUNT];
    uint16_t count;
    uint8_t num_features;
} Dataset_t;

typedef struct {
    fixed_t x;              // Centroid X coordinate
    fixed_t y;              // Centroid Y coordinate
    uint16_t point_count;   // Points in this cluster
    fixed_t inertia;        // Sum of squared distances
} Centroid_t;

typedef struct {
    Centroid_t centroids[KMEANS_MAX_K];
    uint8_t k;              // Number of clusters
    uint8_t iterations;     // Iterations performed
    bool converged;         // Convergence flag
    fixed_t total_inertia;  // Total within-cluster sum of squares
    fixed_t silhouette_score; // Cluster quality score
} KMeansResult_t;

// ============================================================================
// HIERARCHICAL CLUSTERING
// ============================================================================

typedef struct {
    uint16_t left;          // Left child index
    uint16_t right;         // Right child index
    fixed_t distance;       // Merge distance
    uint16_t num_points;    // Points in this cluster
} DendrogramNode_t;

typedef struct {
    DendrogramNode_t nodes[MAX_PULSE_COUNT * 2];
    uint16_t num_nodes;
    uint16_t root_index;
} Dendrogram_t;

// ============================================================================
// DYNAMIC TIME WARPING
// ============================================================================

typedef struct {
    uint16_t path[DTW_MAX_LENGTH][2];   // Warping path coordinates
    uint16_t path_length;
    fixed_t total_distance;
} DTWResult_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Initialization
FuriStatus clustering_engine_init(void);
void clustering_engine_deinit(void);

// K-means clustering
KMeansResult_t clustering_kmeans(const Dataset_t* data, uint8_t k);
void clustering_kmeans_iterative(KMeansResult_t* result, const Dataset_t* data);
bool clustering_kmeans_assign_points(KMeansResult_t* result, const Dataset_t* data);
void clustering_kmeans_update_centroids(KMeansResult_t* result, const Dataset_t* data);
bool clustering_kmeans_check_convergence(const KMeansResult_t* prev, 
                                          const KMeansResult_t* current);

// Hierarchical clustering
Dendrogram_t clustering_hierarchical(const Dataset_t* data, DistanceMetric_t metric);
void clustering_hierarchical_cut(const Dendrogram_t* dendrogram, uint8_t k, 
                                  uint8_t* assignments);

// Dynamic Time Warping
DTWResult_t clustering_dtw(const fixed_t* seq1, uint16_t len1,
                          const fixed_t* seq2, uint16_t len2);
fixed_t clustering_dtw_distance(const Pulse_t* pulses1, uint16_t count1,
                                 const Pulse_t* pulses2, uint16_t count2);

// Cluster quality assessment
fixed_t clustering_silhouette_score(const Dataset_t* data, 
                                     const KMeansResult_t* clusters);
fixed_t clustering_calinski_harabasz(const Dataset_t* data,
                                    const KMeansResult_t* clusters);
fixed_t clustering_davies_bouldin(const Dataset_t* data,
                                   const KMeansResult_t* clusters);

// Optimal k selection
uint8_t clustering_find_optimal_k(const Dataset_t* data, uint8_t k_min, uint8_t k_max);

// Distance calculations
fixed_t clustering_distance_euclidean(const DataPoint_t* a, const DataPoint_t* b);
fixed_t clustering_distance_manhattan(const DataPoint_t* a, const DataPoint_t* b);
fixed_t clustering_distance_cosine(const DataPoint_t* a, const DataPoint_t* b);

// Feature extraction for clustering
void clustering_extract_features(const Frame_t* frame, DataPoint_t* features, 
                                  uint16_t* count);
void clustering_extract_pulse_features(const Pulse_t* pulses, uint16_t pulse_count,
                                        DataPoint_t* features, uint16_t* count);

// Visualization support (for 128x64 display)
void clustering_get_bounds(const Dataset_t* data, fixed_t* min_x, fixed_t* max_x,
                            fixed_t* min_y, fixed_t* max_y);
void clustering_normalize_for_display(const Dataset_t* data, 
                                       uint8_t* x_coords, uint8_t* y_coords,
                                       uint8_t* cluster_ids, uint16_t count);

// Cluster management
void clustering_merge_clusters(KMeansResult_t* clusters, uint8_t c1, uint8_t c2);
void clustering_split_cluster(KMeansResult_t* clusters, const Dataset_t* data,
                               uint8_t cluster_id);

// Real-time clustering for streaming data
void clustering_init_streaming(uint8_t k);
void clustering_add_point_streaming(const DataPoint_t* point);
KMeansResult_t clustering_get_streaming_result(void);
void clustering_reset_streaming(void);

// Cluster persistence
bool clustering_save_clusters(const KMeansResult_t* clusters, const char* filename);
bool clustering_load_clusters(KMeansResult_t* clusters, const char* filename);

// Similarity matrix for cross-session correlation
void clustering_build_similarity_matrix(const Dataset_t* data, 
                                         fixed_t* matrix, uint16_t n);
void clustering_similarity_to_distance(const fixed_t* similarity, 
                                        fixed_t* distance, uint16_t n);

#ifdef __cplusplus
}
#endif

#endif // CLUSTERING_H
