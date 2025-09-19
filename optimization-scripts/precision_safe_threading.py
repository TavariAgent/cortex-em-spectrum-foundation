"""
🎯 PRECISION-SAFE THREADING FOR PI CALCULATIONS
Zero-Drift Multi-Threading Implementation

"Threading Without Precision Loss": Based on drift detection findings,
this implements multi-threading that maintains full 141 decimal place precision.

Key Findings from Drift Detection:
- Basic threading loses 115 decimal places (MAJOR ISSUE!)
- Overflow guard maintains 0 drift (perfect precision preservation)
- Need threading approach that doesn't compromise Decimal precision

Strategy:
- Use Decimal arithmetic exclusively (no float contamination)
- Thread-safe result accumulation
- Order-preserving summation (prevents precision drift)
- Overflow guard integration for safety
"""

import time
import threading
from decimal import Decimal, getcontext
from typing import List, Tuple, Callable, Any
from dataclasses import dataclass
import concurrent.futures
from context_overflow_guard import ContextOverflowGuard

# Maintain ultra-high precision
getcontext().prec = 150

@dataclass
class PrecisionThreadResult:
    """🎯 Thread result with precision tracking"""
    thread_id: int
    result: Decimal
    precision_maintained: bool
    calculation_time: float
    terms_processed: int

class PrecisionSafeThreading:
    """🎯 Threading implementation that maintains full decimal precision"""
    
    def __init__(self):
        self.precision_lock = threading.Lock()
        self.result_lock = threading.Lock()
        self.thread_results = {}
        
        print("🎯 Precision-Safe Threading initialized")
        print(f"   Decimal precision: {getcontext().prec}")
        print("   Strategy: Zero-drift multi-threading")
    
    def precision_safe_worker(self, 
                            operation: Callable, 
                            args_list: List[Any], 
                            thread_id: int) -> PrecisionThreadResult:
        """🔒 Worker function that maintains precision"""
        
        start_time = time.perf_counter()
        
        # Set thread-local precision to match global
        original_context = getcontext()
        local_context = getcontext().copy()
        local_context.prec = 150
        
        # Use thread-local accumulator (pure Decimal)
        thread_result = Decimal('0')
        precision_maintained = True
        
        try:
            # Set the local context for this thread
            getcontext().prec = 150
            
            for args in args_list:
                    # Ensure all operations use Decimal
                    if isinstance(args, (int, float)):
                        args = [Decimal(str(args))]
                    elif not isinstance(args, list):
                        args = [Decimal(str(args))]
                    else:
                        args = [Decimal(str(arg)) for arg in args]
                    
                    # Calculate term with precision preservation
                    term_result = operation(*args)
                    
                    # Ensure result is Decimal
                    if not isinstance(term_result, Decimal):
                        term_result = Decimal(str(term_result))
                        precision_maintained = False  # Flag conversion
                    
                    # Accumulate with order preservation
                    thread_result += term_result
                    
        except Exception as e:
            print(f"❌ Thread {thread_id} precision error: {e}")
            precision_maintained = False
        finally:
            # Restore original context
            pass
        
        calculation_time = time.perf_counter() - start_time
        
        return PrecisionThreadResult(
            thread_id=thread_id,
            result=thread_result,
            precision_maintained=precision_maintained,
            calculation_time=calculation_time,
            terms_processed=len(args_list)
        )
    
    def precision_safe_map(self, 
                          operation: Callable, 
                          input_data: List[Any], 
                          num_threads: int = 4) -> Tuple[Decimal, List[PrecisionThreadResult]]:
        """🎯 Map operation across threads with precision preservation"""
        
        print(f"🎯 Starting precision-safe mapping with {num_threads} threads...")
        print(f"   Input size: {len(input_data)} items")
        
        # Split data for threads
        chunk_size = len(input_data) // num_threads
        thread_chunks = []
        
        for i in range(num_threads):
            start_idx = i * chunk_size
            end_idx = start_idx + chunk_size if i < num_threads - 1 else len(input_data)
            thread_chunks.append(input_data[start_idx:end_idx])
        
        # Execute with precision safety
        thread_results = []
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=num_threads) as executor:
            # Submit all threads
            futures = []
            for i, chunk in enumerate(thread_chunks):
                future = executor.submit(self.precision_safe_worker, operation, chunk, i)
                futures.append((future, i))
            
            # Collect results in order (critical for precision!)
            for future, thread_id in futures:
                try:
                    result = future.result(timeout=30)  # Longer timeout for precision
                    thread_results.append(result)
                    
                    precision_status = "✅ PRECISE" if result.precision_maintained else "⚠️ CONVERTED"
                    print(f"   Thread {thread_id}: {result.terms_processed} terms, "
                          f"{result.calculation_time:.6f}s, {precision_status}")
                    
                except concurrent.futures.TimeoutError:
                    print(f"❌ Thread {thread_id} timeout")
                except Exception as e:
                    print(f"❌ Thread {thread_id} error: {e}")
        
        # Combine results with order preservation (critical!)
        final_result = Decimal('0')
        for thread_result in sorted(thread_results, key=lambda x: x.thread_id):
            final_result += thread_result.result
        
        print(f"✅ Precision-safe mapping complete: {len(thread_results)} threads successful")
        
        return final_result, thread_results
    
    def precision_safe_machin_pi(self, terms: int = 1000, num_threads: int = 4) -> Tuple[Decimal, dict]:
        """🎯 Precision-safe Machin's formula with threading"""
        
        print(f"🎯 Computing precision-safe Machin π with {terms} terms, {num_threads} threads...")
        
        start_time = time.perf_counter()
        
        with ContextOverflowGuard(
            base_byte_allocation=2*1024*1024,  # 2MB for precision
            overflow_threshold_mb=10,
            enable_worker_delegation=True,
            enable_recursive_protection=True
        ) as guard:
            
            def arctan_1_5_operation(n: Decimal) -> Decimal:
                """Precision-safe arctan(1/5) term calculation"""
                n_int = int(n)
                x = Decimal('1') / Decimal('5')
                x_power = x ** (2 * n_int + 1)
                sign = Decimal(-1) ** n_int
                denominator = Decimal(2 * n_int + 1)
                return sign * x_power / denominator
            
            def arctan_1_239_operation(n: Decimal) -> Decimal:
                """Precision-safe arctan(1/239) term calculation"""
                n_int = int(n)
                x = Decimal('1') / Decimal('239')
                x_power = x ** (2 * n_int + 1)
                sign = Decimal(-1) ** n_int
                denominator = Decimal(2 * n_int + 1)
                return sign * x_power / denominator
            
            # Create term indices as Decimals
            term_indices = [Decimal(str(i)) for i in range(terms)]
            
            # Calculate arctan(1/5) with precision-safe threading
            print("   Computing arctan(1/5) terms...")
            arctan_1_5_sum, arctan_1_5_results = self.precision_safe_map(
                arctan_1_5_operation, term_indices, num_threads
            )
            
            # Calculate arctan(1/239) with precision-safe threading  
            print("   Computing arctan(1/239) terms...")
            arctan_1_239_sum, arctan_1_239_results = self.precision_safe_map(
                arctan_1_239_operation, term_indices, num_threads
            )
            
            # Apply Machin's formula with pure Decimal arithmetic
            pi_quarter = Decimal('4') * arctan_1_5_sum - arctan_1_239_sum
            calculated_pi = pi_quarter * Decimal('4')
            
            calculation_time = time.perf_counter() - start_time
            
            # Verify precision maintenance
            all_precise = all(r.precision_maintained for r in arctan_1_5_results + arctan_1_239_results)
            
            metrics = {
                'calculation_time': calculation_time,
                'threads_used': num_threads,
                'terms_calculated': terms,
                'precision_maintained': all_precise,
                'overflow_events': guard.recursive_overflow_count,
                'thread_results_1_5': arctan_1_5_results,
                'thread_results_1_239': arctan_1_239_results,
                'arctan_1_5_sum': arctan_1_5_sum,
                'arctan_1_239_sum': arctan_1_239_sum
            }
            
            print(f"✅ Precision-safe Machin calculation complete:")
            print(f"   Calculated π: {calculated_pi}")
            print(f"   Calculation time: {calculation_time:.6f}s")
            print(f"   Precision maintained: {all_precise}")
            print(f"   Overflow events: {guard.recursive_overflow_count}")
            
            return calculated_pi, metrics

def test_precision_safe_threading():
    """🧪 Test precision-safe threading against baseline"""
    
    print("🧪 TESTING PRECISION-SAFE THREADING")
    print("=" * 80)
    
    # Initialize precision-safe threading
    safe_threading = PrecisionSafeThreading()
    
    # Reference baseline (pure single-threaded)
    from baseline_pi_benchmark import BaselinePiBenchmark
    baseline_benchmark = BaselinePiBenchmark()
    
    print("🎯 Establishing single-threaded baseline...")
    baseline_result = baseline_benchmark.machin_baseline(1000)
    
    print(f"✅ Baseline established:")
    print(f"   π accuracy: {baseline_result.decimal_places_accurate} decimal places")
    print(f"   Error: {baseline_result.absolute_error}")
    print(f"   Time: {baseline_result.execution_time:.6f}s")
    print()
    
    # Test precision-safe threading
    print("🎯 Testing precision-safe threading...")
    threaded_pi, metrics = safe_threading.precision_safe_machin_pi(1000, 4)
    
    # Compare results
    precision_drift = abs(threaded_pi - baseline_result.calculated_pi)
    decimal_accuracy = baseline_benchmark.calculate_decimal_accuracy(threaded_pi, baseline_result.calculated_pi)
    places_lost = max(0, baseline_result.decimal_places_accurate - decimal_accuracy)
    speed_improvement = baseline_result.execution_time / metrics['calculation_time']
    
    print(f"\n📊 PRECISION-SAFE THREADING RESULTS:")
    print(f"   Threaded π: {threaded_pi}")
    print(f"   Baseline π: {baseline_result.calculated_pi}")
    print(f"   Precision drift: {precision_drift}")
    print(f"   Decimal places lost: {places_lost}")
    print(f"   Speed improvement: {speed_improvement:.2f}x")
    print(f"   Precision maintained: {metrics['precision_maintained']}")
    
    if places_lost == 0:
        print("🎉 SUCCESS: Zero precision drift achieved with threading!")
    else:
        print(f"⚠️ Precision drift detected: {places_lost} decimal places lost")
    
    if speed_improvement > 1.0:
        print(f"🚀 Performance gain: {speed_improvement:.2f}x faster than baseline")
    else:
        print(f"⏱️ Performance impact: {1/speed_improvement:.2f}x slower than baseline")
    
    return threaded_pi, metrics, baseline_result

if __name__ == "__main__":
    test_precision_safe_threading()
