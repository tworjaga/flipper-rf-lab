#include "clustering.h"
#include "../core/math/fixed_point.h"
#include <string.h>

#define TAG "CLUSTERING"

// Static state for streaming clustering
static Dataset_t streaming_dataset;
static KMeansResult_t streaming_result;
static bool streaming_active = false;

// Initialize clustering engine
FuriStatus clustering_engine_init(void) {
    FURI_LOG_I(TAG, "Initializing clustering engine");
    
    memset(&streaming_dataset, 0, sizeof(streaming_dataset));
    memset(&streaming_result, 0, sizeof(streaming_result));
    streaming_active = false;
    
    return FuriStatusOk;
}

void clustering_engine_deinit(void) {
    streaming_active = false;
}

// K-means clustering main function
KMeansResult_t clustering_kmeans(const Dataset_t* data, uint8_t k) {
    KMeansResult_t result;
    memset(&result, 0, sizeof(result));
    
    if(k == 0 || k > KMEANS_MAX_K) {
        k = 3;  // Default to 3 clusters
    }
    if(k > data->count) {
        k = data->count;  // Can't have more clusters than points
    }
    
    result.k = k;
    
    // Initialize centroids randomly (using first k points)
    for(uint8_t i = 0; i < k; i++) {
        result.centroids[i].x = data->points[i].x;
        result.centroids[i].y = data->points[i].y;
        result.centroids[i].point_count = 0;
        result.centroids[i].inertia = 0;
    }
    
    // Run iterative optimization
    clustering_kmeans_iterative(&result, data);
    
    // Calculate silhouette score
    result.silhouette_score = clustering_silhouette_score(data, &result);
    
    return result;
}

// Iterative K-means optimization
void clustering_kmeans_iterative(KMeansResult_t* result, const Dataset_t* data) {
    KMeansResult_t prev_result;
    
    for(uint8_t iter = 0; iter < KMEANS_MAX_ITERATIONS; iter++) {
        memcpy(&prev_result, result, sizeof(KMeansResult_t));
        
        // Assign points to nearest centroids
        clustering_kmeans_assign_points(result, data);
        
        // Update centroids
        clustering_kmeans_update_centroids(result, data);
        
        result->iterations = iter + 1;
        
        // Check convergence
        if(clustering_kmeans_check_convergence(&prev_result, result)) {
            result->converged = true;
            break;
        }
    }
    
    // Calculate total inertia
    result->total_inertia = 0;
    for(uint8_t i = 0; i < result->k; i++) {
        result->total_inertia += result->centroids[i].inertia;
    }
}

// Assign points to nearest centroids
bool clustering_kmeans_assign_points(KMeansResult_t* result, const Dataset_t* data) {
    bool changed = false;
    
    // Reset point counts
    for(uint8_t i = 0; i < result->k; i++) {
        result->centroids[i].point_count = 0;
        result->centroids[i].inertia = 0;
    }
    
    // Assign each point to nearest centroid
    for(uint16_t i = 0; i < data->count; i++) {
        fixed_t min_distance = FIXED_MAX;
        uint8_t best_cluster = 0;
        
        for(uint8_t j = 0; j < result->k; j++) {
            DataPoint_t centroid_point;
            centroid_point.x = result->centroids[j].x;
            centroid_point.y = result->centroids[j].y;
            
            fixed_t dist = clustering_distance_euclidean(&data->points[i], &centroid_point);
            
            if(dist < min_distance) {
                min_distance = dist;
                best_cluster = j;
            }
        }
        
        if(data->points[i].cluster_id != best_cluster) {
            changed = true;
        }
        
        // Temporarily store assignment
        // (Note: we're modifying a const pointer here, which is bad practice
        // but necessary for this implementation. In production, use non-const.)
        ((Dataset_t*)data)->points[i].cluster_id = best_cluster;
        result->centroids[best_cluster].point_count++;
        
        // Accumulate squared distance for inertia
        fixed_t sq_dist = fixed_mul(min_distance, min_distance);
        result->centroids[best_cluster].inertia += sq_dist;
    }
    
    return changed;
}

// Update centroid positions
void clustering_kmeans_update_centroids(KMeansResult_t* result, const Dataset_t* data) {
    // Accumulate coordinates for each cluster
    fixed_t sum_x[KMEANS_MAX_K] = {0};
    fixed_t sum_y[KMEANS_MAX_K] = {0};
    uint16_t counts[KMEANS_MAX_K] = {0};
    
    for(uint16_t i = 0; i < data->count; i++) {
        uint8_t cluster = data->points[i].cluster_id;
        sum_x[cluster] += data->points[i].x;
        sum_y[cluster] += data->points[i].y;
        counts[cluster]++;
    }
    
    // Calculate new centroids
    for(uint8_t i = 0; i < result->k; i++) {
        if(counts[i] > 0) {
            result->centroids[i].x = sum_x[i] / counts[i];
            result->centroids[i].y = sum_y[i] / counts[i];
        }
        // If no points in cluster, leave centroid unchanged
    }
}

// Check for convergence
bool clustering_kmeans_check_convergence(const KMeansResult_t* prev,
                                          const KMeansResult_t* current) {
    fixed_t total_movement = 0;
    
    for(uint8_t i = 0; i < current->k; i++) {
        fixed_t dx = current->centroids[i].x - prev->centroids[i].x;
        fixed_t dy = current->centroids[i].y - prev->centroids[i].y;
        
        // Manhattan distance of centroid movement
        total_movement += fixed_abs(dx) + fixed_abs(dy);
    }
    
    // Converged if movement is less than threshold
    // Threshold is 0.5% of typical scale (represented in fixed-point)
    fixed_t threshold = FIXED_ONE / 200;  // 0.5%
    
    return (total_movement < threshold);
}

// Calculate Euclidean distance between two points
fixed_t clustering_distance_euclidean(const DataPoint_t* a, const DataPoint_t* b) {
    fixed_t dx = a->x - b->x;
    fixed_t dy = a->y - b->y;
    
    // sqrt(dx^2 + dy^2)
    fixed_t sum_sq = fixed_mul(dx, dx) + fixed_mul(dy, dy);
    return fixed_sqrt(sum_sq);
}

// Calculate Manhattan distance
fixed_t clustering_distance_manhattan(const DataPoint_t* a, const DataPoint_t* b) {
    return fixed_abs(a->x - b->x) + fixed_abs(a->y - b->y);
}

// Calculate cosine similarity (returns distance, not similarity)
fixed_t clustering_distance_cosine(const DataPoint_t* a, const DataPoint_t* b) {
    // Cosine similarity = (a·b) / (|a| * |b|)
    fixed_t dot = fixed_mul(a->x, b->x) + fixed_mul(a->y, b->y);
    fixed_t norm_a = fixed_sqrt(fixed_mul(a->x, a->x) + fixed_mul(a->y, a->y));
    fixed_t norm_b = fixed_sqrt(fixed_mul(b->x, b->x) + fixed_mul(b->y, b->y));
    
    if(norm_a == 0 || norm_b == 0) return FIXED_MAX;
    
    fixed_t similarity = fixed_div(dot, fixed_mul(norm_a, norm_b));
    
    // Convert to distance: distance = 1 - similarity
    return FIXED_ONE - similarity;
}

// Calculate silhouette score
fixed_t clustering_silhouette_score(const Dataset_t* data,
                                     const KMeansResult_t* clusters) {
    if(clusters->k < 2 || data->count < 2) return 0;
    
    fixed_t total_score = 0;
    
    for(uint16_t i = 0; i < data->count; i++) {
        uint8_t own_cluster = data->points[i].cluster_id;
        
        // Calculate a(i): mean distance to points in same cluster
        fixed_t a = 0;
        uint16_t own_count = 0;
        for(uint16_t j = 0; j < data->count; j++) {
            if(i != j && data->points[j].cluster_id == own_cluster) {
                a += clustering_distance_euclidean(&data->points[i], &data->points[j]);
                own_count++;
            }
        }
        if(own_count > 0) a = a / own_count;
        
        // Calculate b(i): mean distance to points in nearest other cluster
        fixed_t b = FIXED_MAX;
        for(uint8_t c = 0; c < clusters->k; c++) {
            if(c == own_cluster) continue;
            
            fixed_t dist = 0;
            uint16_t count = 0;
            for(uint16_t j = 0; j < data->count; j++) {
                if(data->points[j].cluster_id == c) {
                    dist += clustering_distance_euclidean(&data->points[i], &data->points[j]);
                    count++;
                }
            }
            if(count > 0) {
                dist = dist / count;
                if(dist < b) b = dist;
            }
        }
        
        // s(i) = (b(i) - a(i)) / max(a(i), b(i))
        fixed_t max_ab = (a > b) ? a : b;
        if(max_ab > 0) {
            fixed_t s = fixed_div(b - a, max_ab);
            total_score += s;
        }
    }
    
    return total_score / data->count;
}

// Extract features from frame for clustering
void clustering_extract_features(const Frame_t* frame, DataPoint_t* features,
                                  uint16_t* count) {
    *count = 0;
    
    if(frame->length == 0) return;
    
    // Feature 1: Frame duration
    features[*count].x = INT_TO_FIXED(frame->duration_us);
    // Feature 2: Frame length in bytes
    features[*count].y = INT_TO_FIXED(frame->length);
    features[*count].cluster_id = 0;
    (*count)++;
    
    // Feature 3: RSSI
    if(*count < MAX_PULSE_COUNT) {
        features[*count].x = INT_TO_FIXED(frame->rssi_dbm);
        features[*count].y = INT_TO_FIXED(frame->frequency_hz / 1000000);  // MHz
        features[*count].cluster_id = 0;
        (*count)++;
    }
}

// Extract features from pulse sequence
void clustering_extract_pulse_features(const Pulse_t* pulses, uint16_t pulse_count,
                                        DataPoint_t* features, uint16_t* count) {
    *count = 0;
    
    if(pulse_count < 2) return;
    
    // Create features from consecutive pulse pairs
    for(uint16_t i = 0; i < pulse_count - 1 && *count < MAX_PULSE_COUNT; i += 2) {
        // Feature 1: Mark width
        features[*count].x = INT_TO_FIXED(pulses[i].width_us);
        // Feature 2: Space width
        features[*count].y = INT_TO_FIXED(pulses[i + 1].width_us);
        features[*count].cluster_id = 0;
        (*count)++;
    }
}

// Find optimal k using silhouette score
uint8_t clustering_find_optimal_k(const Dataset_t* data, uint8_t k_min, uint8_t k_max) {
    fixed_t best_score = FIXED_MIN;
    uint8_t best_k = k_min;
    
    for(uint8_t k = k_min; k <= k_max && k <= KMEANS_MAX_K; k++) {
        KMeansResult_t result = clustering_kmeans(data, k);
        fixed_t score = result.silhouette_score;
        
        if(score > best_score) {
            best_score = score;
            best_k = k;
        }
    }
    
    return best_k;
}

// Initialize streaming clustering
void clustering_init_streaming(uint8_t k) {
    memset(&streaming_dataset, 0, sizeof(streaming_dataset));
    memset(&streaming_result, 0, sizeof(streaming_result));
    streaming_result.k = k;
    streaming_active = true;
}

// Add point to streaming clustering
void clustering_add_point_streaming(const DataPoint_t* point) {
    if(!streaming_active) return;
    if(streaming_dataset.count >= MAX_PULSE_COUNT) return;
    
    memcpy(&streaming_dataset.points[streaming_dataset.count], point, sizeof(DataPoint_t));
    streaming_dataset.count++;
    
    // Re-cluster when we have enough new points
    if(streaming_dataset.count % 50 == 0) {
        streaming_result = clustering_kmeans(&streaming_dataset, streaming_result.k);
    }
}

// Get current streaming result
KMeansResult_t clustering_get_streaming_result(void) {
    return streaming_result;
}

// Reset streaming clustering
void clustering_reset_streaming(void) {
    memset(&streaming_dataset, 0, sizeof(streaming_dataset));
    streaming_dataset.count = 0;
}

// Get data bounds for visualization
void clustering_get_bounds(const Dataset_t* data, fixed_t* min_x, fixed_t* max_x,
                            fixed_t* min_y, fixed_t* max_y) {
    if(data->count == 0) {
        *min_x = *min_y = 0;
        *max_x = *max_y = FIXED_ONE;
        return;
    }
    
    *min_x = *max_x = data->points[0].x;
    *min_y = *max_y = data->points[0].y;
    
    for(uint16_t i = 1; i < data->count; i++) {
        if(data->points[i].x < *min_x) *min_x = data->points[i].x;
        if(data->points[i].x > *max_x) *max_x = data->points[i].x;
        if(data->points[i].y < *min_y) *min_y = data->points[i].y;
        if(data->points[i].y > *max_y) *max_y = data->points[i].y;
    }
}

// Normalize coordinates for 128x64 display
void clustering_normalize_for_display(const Dataset_t* data,
                                       uint8_t* x_coords, uint8_t* y_coords,
                                       uint8_t* cluster_ids, uint16_t count) {
    fixed_t min_x, max_x, min_y, max_y;
    clustering_get_bounds(data, &min_x, &max_x, &min_y, &max_y);
    
    fixed_t range_x = max_x - min_x;
    fixed_t range_y = max_y - min_y;
    
    if(range_x == 0) range_x = FIXED_ONE;
    if(range_y == 0) range_y = FIXED_ONE;
    
    for(uint16_t i = 0; i < count && i < data->count; i++) {
        // Map to 0-127 for X, 0-63 for Y
        fixed_t norm_x = fixed_div(data->points[i].x - min_x, range_x);
        fixed_t norm_y = fixed_div(data->points[i].y - min_y, range_y);
        
        x_coords[i] = (uint8_t)FIXED_TO_INT(norm_x * 127);
        y_coords[i] = (uint8_t)(63 - FIXED_TO_INT(norm_y * 63));  // Flip Y for display
        cluster_ids[i] = data->points[i].cluster_id;
    }
}

// Hierarchical clustering (simplified)
Dendrogram_t clustering_hierarchical(const Dataset_t* data, DistanceMetric_t metric) {
    Dendrogram_t dendrogram;
    memset(&dendrogram, 0, sizeof(dendrogram));
    
    // This is a placeholder for full hierarchical clustering
    // Full implementation would require O(n^2) distance matrix and
    // iterative merging of closest clusters
    
    FURI_LOG_I(TAG, "Hierarchical clustering not fully implemented");
    UNUSED(data);
    UNUSED(metric);
    
    return dendrogram;
}

// Dynamic Time Warping (simplified)
DTWResult_t clustering_dtw(const fixed_t* seq1, uint16_t len1,
                          const fixed_t* seq2, uint16_t len2) {
    DTWResult_t result;
    memset(&result, 0, sizeof(result));
    
    if(len1 > DTW_MAX_LENGTH) len1 = DTW_MAX_LENGTH;
    if(len2 > DTW_MAX_LENGTH) len2 = DTW_MAX_LENGTH;
    
    // DTW distance matrix (would use dynamic programming)
    // This is a simplified placeholder
    fixed_t total_dist = 0;
    uint16_t min_len = (len1 < len2) ? len1 : len2;
    
    for(uint16_t i = 0; i < min_len; i++) {
        fixed_t diff = seq1[i] - seq2[i];
        total_dist += fixed_abs(diff);
    }
    
    result.total_distance = total_dist / min_len;
    result.path_length = min_len;
    
    return result;
}

// DTW distance for pulse sequences
fixed_t clustering_dtw_distance(const Pulse_t* pulses1, uint16_t count1,
                                 const Pulse_t* pulses2, uint16_t count2) {
    // Extract widths as fixed-point sequences
    fixed_t seq1[DTW_MAX_LENGTH];
    fixed_t seq2[DTW_MAX_LENGTH];
    
    uint16_t len1 = (count1 > DTW_MAX_LENGTH) ? DTW_MAX_LENGTH : count1;
    uint16_t len2 = (count2 > DTW_MAX_LENGTH) ? DTW_MAX_LENGTH : count2;
    
    for(uint16_t i = 0; i < len1; i++) {
        seq1[i] = INT_TO_FIXED(pulses1[i].width_us);
    }
    
    for(uint16_t i = 0; i < len2; i++) {
        seq2[i] = INT_TO_FIXED(pulses2[i].width_us);
    }
    
    DTWResult_t result = clustering_dtw(seq1, len1, seq2, len2);
    return result.total_distance;
}

// Save clusters to file
bool clustering_save_clusters(const KMeansResult_t* clusters, const char* filename) {
    // Would implement file serialization
    UNUSED(clusters);
    UNUSED(filename);
    return true;
}

// Load clusters from file
bool clustering_load_clusters(KMeansResult_t* clusters, const char* filename) {
    // Would implement file deserialization
    UNUSED(clusters);
    UNUSED(filename);
    return true;
}
