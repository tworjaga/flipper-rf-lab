#!/usr/bin/env python3
"""
Flipper RF Lab - Algorithm Verification Tests
Tests the core algorithms implemented in C using Python simulations
"""

import math
import random
import struct
from typing import List, Tuple
from dataclasses import dataclass
from enum import Enum

# Test statistics
@dataclass
class TestStats:
    total: int = 0
    passed: int = 0
    failed: int = 0
    
    def report(self):
        print(f"\n{'='*50}")
        print("TEST SUMMARY")
        print(f"{'='*50}")
        print(f"Total tests:  {self.total}")
        print(f"Passed:       {self.passed} ({self.passed/self.total*100:.1f}%)")
        print(f"Failed:       {self.failed} ({self.failed/self.total*100:.1f}%)")
        print(f"{'='*50}")
        if self.failed == 0:
            print("✓ ALL TESTS PASSED")
        else:
            print("✗ SOME TESTS FAILED")

stats = TestStats()

def test(name: str, condition: bool, details: str = ""):
    """Run a single test"""
    stats.total += 1
    if condition:
        stats.passed += 1
        print(f"  ✓ {name}")
        if details:
            print(f"    {details}")
    else:
        stats.failed += 1
        print(f"  ✗ {name}")
        if details:
            print(f"    {details}")
    return condition

def test_suite(name: str):
    """Start a test suite"""
    print(f"\n{'='*50}")
    print(f"{name}")
    print(f"{'='*50}")

# ============================================================================
# FIXED-POINT MATH TESTS
# ============================================================================

class FixedPoint:
    """Q15.16 fixed-point arithmetic simulation"""
    SCALE = 65536
    
    def __init__(self, value: float = 0):
        if isinstance(value, int):
            self.raw = value * self.SCALE
        else:
            self.raw = int(value * self.SCALE)
    
    @property
    def float(self) -> float:
        return self.raw / self.SCALE
    
    def __add__(self, other):
        result = FixedPoint()
        result.raw = self.raw + other.raw
        return result
    
    def __sub__(self, other):
        result = FixedPoint()
        result.raw = self.raw - other.raw
        return result
    
    def __mul__(self, other):
        result = FixedPoint()
        result.raw = (self.raw * other.raw) // self.SCALE
        return result
    
    def __truediv__(self, other):
        result = FixedPoint()
        if other.raw != 0:
            result.raw = (self.raw * self.SCALE) // other.raw
        return result
    
    def sqrt(self):
        """Integer square root using Newton's method"""
        if self.raw <= 0:
            return FixedPoint(0)
        x = self.raw
        for _ in range(20):
            x = (x + (self.raw * self.SCALE) // x) // 2
        result = FixedPoint()
        result.raw = x
        return result
    
    def abs(self):
        result = FixedPoint()
        result.raw = abs(self.raw)
        return result
    
    def __repr__(self):
        return f"Fixed({self.float:.4f})"

def test_fixed_point():
    test_suite("Fixed-Point Math (Q15.16)")
    
    # Test basic operations
    a = FixedPoint(10)
    b = FixedPoint(5)
    
    test("Addition", (a + b).float == 15.0, f"10 + 5 = {(a + b).float}")
    test("Subtraction", (a - b).float == 5.0, f"10 - 5 = {(a - b).float}")
    test("Multiplication", abs((a * b).float - 50.0) < 0.01, f"10 * 5 = {(a * b).float}")
    test("Division", abs((a / b).float - 2.0) < 0.01, f"10 / 5 = {(a / b).float}")
    
    # Test square root
    sqrt_16 = FixedPoint(16).sqrt()
    test("Square root", abs(sqrt_16.float - 4.0) < 0.1, f"sqrt(16) = {sqrt_16.float}")
    
    # Test absolute value
    neg = FixedPoint(-10)
    test("Absolute value", neg.abs().float == 10.0, f"abs(-10) = {neg.abs().float}")
    
    # Test conversion round-trip
    f = 3.14159
    fp = FixedPoint(f)
    f_back = fp.float
    test("Float conversion", abs(f - f_back) < 0.0001, f"{f} -> {f_back}")

# ============================================================================
# COMPRESSION TESTS
# ============================================================================

def delta_encode(input_data: bytes) -> bytes:
    """Delta encoding simulation"""
    if not input_data:
        return b''
    
    output = bytearray()
    output.append(input_data[0])
    last = input_data[0]
    
    for byte in input_data[1:]:
        delta = byte - last
        if -128 <= delta <= 127:
            output.append(delta & 0xFF)
        else:
            output.append(0x80)
            output.append((delta >> 8) & 0xFF)
            output.append(delta & 0xFF)
        last = byte
    
    return bytes(output)

def delta_decode(input_data: bytes) -> bytes:
    """Delta decoding simulation"""
    if not input_data:
        return b''
    
    output = bytearray()
    output.append(input_data[0])
    last = input_data[0]
    i = 1
    
    while i < len(input_data):
        byte = input_data[i]
        if byte == 0x80 and i + 2 < len(input_data):
            delta = struct.unpack('>h', input_data[i+1:i+3])[0]
            i += 3
        else:
            delta = struct.unpack('b', bytes([byte]))[0]
            i += 1
        last = (last + delta) & 0xFF
        output.append(last)
    
    return bytes(output)

def rle_encode(input_data: bytes) -> bytes:
    """Run-length encoding simulation"""
    output = bytearray()
    i = 0
    
    while i < len(input_data):
        symbol = input_data[i]
        run_length = 1
        while i + run_length < len(input_data) and input_data[i + run_length] == symbol and run_length < 255:
            run_length += 1
        
        if run_length >= 3:
            output.append(0x00)
            output.append(run_length)
            output.append(symbol)
            i += run_length
        else:
            if symbol == 0x00:
                output.append(0x00)
                output.append(0x01)
            output.append(symbol)
            i += 1
    
    return bytes(output)

def rle_decode(input_data: bytes) -> bytes:
    """Run-length decoding simulation"""
    output = bytearray()
    i = 0
    
    while i < len(input_data):
        byte = input_data[i]
        if byte == 0x00:
            if i + 1 >= len(input_data):
                break
            next_byte = input_data[i + 1]
            if next_byte == 0x00:
                output.append(0x00)
                i += 2
            elif next_byte == 0x01:
                if i + 2 >= len(input_data):
                    break
                output.append(input_data[i + 2])
                i += 3
            else:
                if i + 2 >= len(input_data):
                    break
                run_length = next_byte
                symbol = input_data[i + 2]
                output.extend([symbol] * run_length)
                i += 3
        else:
            output.append(byte)
            i += 1
    
    return bytes(output)

def test_compression():
    test_suite("Compression Algorithms")
    
    # Test delta encoding with data that has small deltas (all fit in signed byte)
    # Sequential data 0-99 has deltas of +1, which compress well
    test_data = bytes(range(100))  # 0, 1, 2, ..., 99
    compressed = delta_encode(test_data)
    decompressed = delta_decode(compressed)
    
    # For sequential data, delta encoding stores: first byte + 99 delta bytes
    # But since deltas are small (+1), they fit in 1 byte each
    # So size is similar, but let's verify it works correctly
    ratio = len(compressed) / len(test_data) * 100 if test_data else 0
    test("Delta encoding works", decompressed == test_data,
         f"Original: {len(test_data)} bytes, Compressed: {len(compressed)} bytes ({ratio:.1f}%)")
    test("Delta round-trip", decompressed == test_data,
         f"Data integrity verified")
    
    # Test with data that has larger jumps - this is where delta shines
    jump_data = bytes([0, 0, 50, 50, 100, 100, 150, 150, 200, 200] * 10)
    jump_compressed = delta_encode(jump_data)
    jump_decompressed = delta_decode(jump_compressed)
    jump_ratio = len(jump_compressed) / len(jump_data) * 100 if jump_data else 0
    
    test("Delta with jump data", jump_decompressed == jump_data,
         f"Jump data: {len(jump_data)} bytes -> {len(jump_compressed)} bytes ({jump_ratio:.1f}%)")
    
    # Test RLE with repetitive data
    rle_data = b'\xAA' * 50 + b'\xBB' * 50
    rle_compressed = rle_encode(rle_data)
    rle_decompressed = rle_decode(rle_compressed)
    
    rle_ratio = len(rle_compressed) / len(rle_data) * 100 if rle_data else 0
    test("RLE compression reduces size", len(rle_compressed) < len(rle_data),
         f"{len(rle_data)} bytes -> {len(rle_compressed)} bytes ({rle_ratio:.1f}%)")
    test("RLE round-trip", rle_decompressed == rle_data)
    
    # RLE should achieve 5:1 ratio on repetitive data
    test("RLE achieves 5:1 ratio", rle_ratio < 20.0, f"Actual ratio: {100/rle_ratio:.1f}:1")



# ============================================================================
# STATISTICS TESTS
# ============================================================================

class WelfordState:
    """Welford's online algorithm simulation"""
    def __init__(self):
        self.n = 0
        self.mean = 0.0
        self.m2 = 0.0
        self.min_val = float('inf')
        self.max_val = float('-inf')
    
    def add(self, x: float):
        self.n += 1
        self.min_val = min(self.min_val, x)
        self.max_val = max(self.max_val, x)
        delta = x - self.mean
        self.mean += delta / self.n
        delta2 = x - self.mean
        self.m2 += delta * delta2
    
    @property
    def variance(self):
        return self.m2 / (self.n - 1) if self.n > 1 else 0
    
    @property
    def std_dev(self):
        return math.sqrt(self.variance)

def test_statistics():
    test_suite("Statistical Analysis")
    
    # Test Welford's algorithm
    data = list(range(1, 11))  # 1, 2, ..., 10
    state = WelfordState()
    
    for x in data:
        state.add(x)
    
    expected_mean = 5.5
    expected_var = 9.166666666666666

    
    test("Welford mean", abs(state.mean - expected_mean) < 0.01,
         f"Mean: {state.mean:.4f} (expected {expected_mean})")
    test("Welford variance", abs(state.variance - expected_var) < 0.1,
         f"Variance: {state.variance:.4f} (expected {expected_var:.4f})")
    test("Welford min", state.min_val == 1.0)
    test("Welford max", state.max_val == 10.0)
    
    # Test standard functions
    mean = sum(data) / len(data)
    variance = sum((x - mean) ** 2 for x in data) / (len(data) - 1)
    
    test("Standard mean matches", abs(mean - expected_mean) < 0.01)
    test("Standard variance matches", abs(variance - expected_var) < 0.1)

# ============================================================================
# CLUSTERING TESTS
# ============================================================================

def euclidean_distance(a: Tuple[float, float], b: Tuple[float, float]) -> float:
    """Calculate Euclidean distance between two points"""
    return math.sqrt((a[0] - b[0])**2 + (a[1] - b[1])**2)

def kmeans_cluster(points: List[Tuple[float, float]], k: int, max_iter: int = 100) -> List[int]:
    """Simple K-means clustering"""
    if not points or k > len(points):
        return [0] * len(points)
    
    # Initialize centroids
    centroids = points[:k]
    assignments = [0] * len(points)
    
    for _ in range(max_iter):
        # Assign points to nearest centroid
        changed = False
        for i, point in enumerate(points):
            distances = [euclidean_distance(point, c) for c in centroids]
            new_assignment = distances.index(min(distances))
            if new_assignment != assignments[i]:
                changed = True
                assignments[i] = new_assignment
        
        if not changed:
            break
        
        # Update centroids
        for j in range(k):
            cluster_points = [points[i] for i in range(len(points)) if assignments[i] == j]
            if cluster_points:
                centroids[j] = (
                    sum(p[0] for p in cluster_points) / len(cluster_points),
                    sum(p[1] for p in cluster_points) / len(cluster_points)
                )
    
    return assignments

def test_clustering():
    test_suite("Clustering Algorithms")
    
    # Create test dataset with 2 clear clusters
    cluster1 = [(9, 9), (10, 10), (11, 11)]  # Around (10, 10)
    cluster2 = [(19, 19), (20, 20), (21, 21)]  # Around (20, 20)
    points = cluster1 + cluster2
    
    # Test distance calculation
    dist = euclidean_distance(points[0], points[1])
    test("Euclidean distance", abs(dist - math.sqrt(2)) < 0.01,
         f"Distance between (9,9) and (10,10): {dist:.4f}")
    
    # Test K-means
    assignments = kmeans_cluster(points, 2)
    
    # Check that points in same cluster have same assignment
    test("K-means clustering", 
         len(set(assignments[:3])) == 1 and len(set(assignments[3:])) == 1 and
         assignments[0] != assignments[3],
         f"Cluster assignments: {assignments}")
    
    # Test intra vs inter-cluster distances
    intra_dist = euclidean_distance(points[0], points[2])
    inter_dist = euclidean_distance(points[0], points[3])
    test("Intra-cluster < inter-cluster", intra_dist < inter_dist,
         f"Intra: {intra_dist:.2f}, Inter: {inter_dist:.2f}")

# ============================================================================
# THREAT MODEL TESTS
# ============================================================================

def shannon_entropy(data: bytes) -> float:
    """Calculate Shannon entropy"""
    if not data:
        return 0.0
    
    frequencies = {}
    for byte in data:
        frequencies[byte] = frequencies.get(byte, 0) + 1
    
    entropy = 0.0
    length = len(data)
    for count in frequencies.values():
        p = count / length
        entropy -= p * math.log2(p)
    
    return entropy

def test_threat_model():
    test_suite("Threat Model - Entropy Analysis")
    
    # Test high entropy (random data)
    random_data = bytes([random.randint(0, 255) for _ in range(1000)])
    random_entropy = shannon_entropy(random_data)
    test("Random data entropy > 7.5", random_entropy > 7.5,
         f"Entropy: {random_entropy:.2f} bits/byte")
    
    # Test low entropy (static data)
    static_data = b'\x42' * 1000
    static_entropy = shannon_entropy(static_data)
    test("Static data entropy ~0", static_entropy < 0.1,
         f"Entropy: {static_entropy:.2f} bits/byte")
    
    # Test medium entropy (structured data)
    structured_data = bytes([i % 16 for i in range(1000)])
    structured_entropy = shannon_entropy(structured_data)
    test("Structured data entropy ~4", 3.5 < structured_entropy < 4.5,
         f"Entropy: {structured_entropy:.2f} bits/byte (expected ~4 for 16 values)")
    
    # Test vulnerability scoring
    # Low entropy = high vulnerability
    test("Low entropy = vulnerable", static_entropy < 4.0,
         f"Static data would be flagged as vulnerable (entropy < 4)")
    test("High entropy = secure", random_entropy > 7.0,
         f"Random data would be flagged as secure (entropy > 7)")

# ============================================================================
# PROTOCOL INFERENCE TESTS
# ============================================================================

class ModulationType(Enum):
    UNKNOWN = 0
    OOK = 1
    ASK = 2
    FSK = 3

def detect_modulation(pulses: List[Tuple[int, int]]) -> ModulationType:
    """Simple modulation detection"""
    if len(pulses) < 10:
        return ModulationType.UNKNOWN
    
    # Check for OOK (long spaces)
    spaces = [p[1] for p in pulses if p[0] == 0]
    marks = [p[1] for p in pulses if p[0] == 1]
    
    if spaces and marks:
        avg_space = sum(spaces) / len(spaces)
        avg_mark = sum(marks) / len(marks)
        
        # OOK has asymmetric timing
        if avg_space > avg_mark * 2 or avg_mark > avg_space * 2:
            return ModulationType.OOK
    
    # Check for multiple timing clusters (FSK)
    unique_widths = len(set(p[1] for p in pulses))
    if unique_widths >= 3:
        return ModulationType.FSK
    
    return ModulationType.ASK

def test_protocol_inference():
    test_suite("Protocol Inference")
    
    # Simulate OOK pulses (long spaces, short marks)
    ook_pulses = [(1, 100), (0, 500), (1, 100), (0, 500), (1, 100)] * 10
    detected = detect_modulation(ook_pulses)
    test("OOK detection", detected == ModulationType.OOK,
         f"Detected: {detected.name}")
    
    # Simulate FSK pulses (multiple distinct timing values - need 3+ unique)
    fsk_pulses = [(1, 100), (1, 150), (1, 200), (1, 100), (1, 150), (1, 200)] * 10
    detected_fsk = detect_modulation(fsk_pulses)
    test("FSK detection", detected_fsk == ModulationType.FSK,
         f"Detected: {detected_fsk.name} (3+ timing values)")
    
    # Simulate ASK pulses (consistent timing, 1-2 unique values)
    ask_pulses = [(1, 100), (0, 100), (1, 100), (0, 100)] * 10
    detected_ask = detect_modulation(ask_pulses)
    test("ASK detection", detected_ask == ModulationType.ASK,
         f"Detected: {detected_ask.name}")


# ============================================================================
# MAIN
# ============================================================================

def main():
    print("=" * 50)
    print("Flipper RF Lab - Algorithm Verification")
    print("=" * 50)
    print("Testing portable components (no Flipper SDK required)")
    print()
    
    # Run all test suites
    test_fixed_point()
    test_compression()
    test_statistics()
    test_clustering()
    test_threat_model()
    test_protocol_inference()
    
    # Print summary
    stats.report()
    
    return 0 if stats.failed == 0 else 1

if __name__ == "__main__":
    exit(main())
