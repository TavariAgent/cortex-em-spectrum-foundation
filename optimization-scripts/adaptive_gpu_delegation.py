#!/usr/bin/env python3
"""
🔧 ADAPTIVE GPU DELEGATION SYSTEM
Cross-Architecture GPU Optimization

Dynamically adapts GPU delegation based on hardware capabilities:
- GTX 1060 (Pascal) - Conservative settings for stability
- RTX 4070 Super (Ada Lovelace) - Aggressive optimization
- Universal compatibility across all CUDA architectures
"""

import sys
import os
from typing import Dict, Any, Tuple, Optional, Union
from decimal import Decimal, getcontext
import importlib.util

# Add adaptions directory to path for imports
sys.path.append(os.path.join(os.path.dirname(__file__)))

try:
    from gpu_compatibility_checker import check_gpu_compatibility, create_adaptive_gpu_config
except ImportError:
    print("⚠️ GPU compatibility checker not found - using conservative defaults")


class AdaptiveGPUDelegator:
    """🔧 Adaptive GPU delegation based on hardware capabilities"""
    
    def __init__(self):
        self.gpu_available = False
        self.cupy_available = False
        self.config = self._initialize_config()
        self.stats = {
            'gpu_operations': 0,
            'cpu_fallbacks': 0,
            'total_operations': 0,
            'memory_errors': 0
        }
        
        # Try to initialize CuPy
        try:
            import cupy as cp
            self.cp = cp
            self.cupy_available = True
            self.gpu_available = True
            print(f"✅ CuPy initialized - GPU acceleration enabled")
        except ImportError:
            print("⚠️ CuPy not available - CPU-only mode")
            self.cp = None
    
    def _initialize_config(self) -> Dict[str, Any]:
        """🎯 Initialize configuration based on GPU capabilities"""
        
        # Get hardware-specific configuration
        try:
            compatibility_info = check_gpu_compatibility()
            config = create_adaptive_gpu_config(compatibility_info)
            
            print(f"🎮 GPU Configuration loaded:")
            print(f"   GPU: {compatibility_info.get('gpu_name', 'Unknown')}")
            print(f"   Architecture: {compatibility_info.get('architecture', 'Unknown')}")
            print(f"   Memory Limit: {config.get('gpu_memory_limit_mb', 0)}MB")
            print(f"   Batch Size: {config.get('batch_size', 1000)}")
            print(f"   Complexity Threshold: {config.get('complexity_threshold', 1000.0)}")
            
            return config
            
        except Exception as e:
            print(f"⚠️ Could not load GPU configuration: {e}")
            return self._get_conservative_config()
    
    def _get_conservative_config(self) -> Dict[str, Any]:
        """🛡️ Conservative configuration for unknown hardware"""
        return {
            'enable_gpu': False,
            'cpu_fallback': True,
            'gpu_memory_limit_mb': 1024,
            'batch_size': 1000,
            'complexity_threshold': 2000.0,
            'arithmetic_threshold': 200.0,
            'min_array_size_gpu': 5000,
            'max_array_size_gpu': 1000000,
            'max_memory_usage_ratio': 0.5
        }
    
    def should_use_gpu(self, operation_type: str, array_size: int = 0, 
                       complexity: float = 0.0) -> bool:
        """🤔 Determine if operation should use GPU"""
        
        if not self.gpu_available or not self.config.get('enable_gpu', False):
            return False
        
        # Check array size limits
        min_size = self.config.get('min_array_size_gpu', 5000)
        max_size = self.config.get('max_array_size_gpu', 1000000)
        
        if array_size > 0:
            if array_size < min_size or array_size > max_size:
                return False
        
        # Check operation complexity
        if operation_type in ['exponential', 'power', 'trigonometric']:
            threshold = self.config.get('complexity_threshold', 1000.0)
            if complexity > threshold:
                return True
        
        elif operation_type in ['arithmetic', 'basic']:
            threshold = self.config.get('arithmetic_threshold', 100.0)
            if complexity > threshold:
                return True
        
        return array_size >= min_size
    
    def gpu_exponential(self, base: Union[Decimal, float], 
                       exponent: Union[Decimal, float], 
                       precision: int = 50) -> Decimal:
        """🚀 GPU-accelerated exponential calculation"""
        
        if not self.should_use_gpu('exponential', complexity=float(exponent)):
            return self._cpu_exponential(base, exponent, precision)
        
        try:
            self.stats['gpu_operations'] += 1
            self.stats['total_operations'] += 1
            
            # Convert to GPU arrays
            base_gpu = self.cp.asarray([float(base)])
            exp_gpu = self.cp.asarray([float(exponent)])
            
            # GPU calculation
            result_gpu = self.cp.power(base_gpu, exp_gpu)
            result = float(self.cp.asnumpy(result_gpu)[0])
            
            # Convert back to high precision
            getcontext().prec = precision + 10
            return Decimal(str(result))
            
        except Exception as e:
            self.stats['memory_errors'] += 1
            print(f"⚠️ GPU exponential failed: {e} - using CPU fallback")
            return self._cpu_exponential(base, exponent, precision)
    
    def gpu_power(self, base: Union[Decimal, float], 
                  exponent: Union[Decimal, float], 
                  precision: int = 50) -> Decimal:
        """⚡ GPU-accelerated power calculation"""
        
        if not self.should_use_gpu('power', complexity=float(abs(exponent))):
            return self._cpu_power(base, exponent, precision)
        
        try:
            self.stats['gpu_operations'] += 1
            self.stats['total_operations'] += 1
            
            # Convert to GPU arrays
            base_gpu = self.cp.asarray([float(base)])
            exp_gpu = self.cp.asarray([float(exponent)])
            
            # GPU calculation with error handling
            result_gpu = self.cp.power(base_gpu, exp_gpu)
            result = float(self.cp.asnumpy(result_gpu)[0])
            
            # Validate result
            if self.cp.isnan(result_gpu).any() or self.cp.isinf(result_gpu).any():
                raise ValueError("GPU calculation produced invalid result")
            
            # Convert back to high precision
            getcontext().prec = precision + 10
            return Decimal(str(result))
            
        except Exception as e:
            self.stats['memory_errors'] += 1
            print(f"⚠️ GPU power failed: {e} - using CPU fallback")
            return self._cpu_power(base, exponent, precision)
    
    def gpu_vector_operations(self, array1: list, array2: list, 
                            operation: str = 'add') -> list:
        """🔢 GPU-accelerated vector operations"""
        
        array_size = max(len(array1), len(array2))
        
        if not self.should_use_gpu('arithmetic', array_size=array_size):
            return self._cpu_vector_operations(array1, array2, operation)
        
        try:
            self.stats['gpu_operations'] += 1
            self.stats['total_operations'] += 1
            
            # Convert to GPU arrays
            arr1_gpu = self.cp.asarray([float(x) for x in array1])
            arr2_gpu = self.cp.asarray([float(x) for x in array2])
            
            # GPU vector operations
            if operation == 'add':
                result_gpu = self.cp.add(arr1_gpu, arr2_gpu)
            elif operation == 'subtract':
                result_gpu = self.cp.subtract(arr1_gpu, arr2_gpu)
            elif operation == 'multiply':
                result_gpu = self.cp.multiply(arr1_gpu, arr2_gpu)
            elif operation == 'divide':
                result_gpu = self.cp.divide(arr1_gpu, arr2_gpu)
            else:
                raise ValueError(f"Unsupported operation: {operation}")
            
            # Convert back to CPU
            result = self.cp.asnumpy(result_gpu).tolist()
            return [Decimal(str(x)) for x in result]
            
        except Exception as e:
            self.stats['memory_errors'] += 1
            print(f"⚠️ GPU vector operation failed: {e} - using CPU fallback")
            return self._cpu_vector_operations(array1, array2, operation)
    
    def gpu_trigonometric_series(self, x: Union[Decimal, float], 
                                function: str = 'sin', 
                                terms: int = 50) -> Decimal:
        """📐 GPU-accelerated trigonometric series"""
        
        if not self.should_use_gpu('trigonometric', complexity=float(terms)):
            return self._cpu_trigonometric_series(x, function, terms)
        
        try:
            self.stats['gpu_operations'] += 1
            self.stats['total_operations'] += 1
            
            x_float = float(x)
            x_gpu = self.cp.asarray([x_float])
            
            if function == 'sin':
                result_gpu = self.cp.sin(x_gpu)
            elif function == 'cos':
                result_gpu = self.cp.cos(x_gpu)
            elif function == 'tan':
                result_gpu = self.cp.tan(x_gpu)
            else:
                raise ValueError(f"Unsupported trigonometric function: {function}")
            
            result = float(self.cp.asnumpy(result_gpu)[0])
            return Decimal(str(result))
            
        except Exception as e:
            self.stats['memory_errors'] += 1
            print(f"⚠️ GPU trigonometric failed: {e} - using CPU fallback")
            return self._cpu_trigonometric_series(x, function, terms)
    
    # CPU Fallback Methods
    def _cpu_exponential(self, base: Union[Decimal, float], 
                        exponent: Union[Decimal, float], 
                        precision: int = 50) -> Decimal:
        """🐌 CPU fallback for exponential"""
        self.stats['cpu_fallbacks'] += 1
        self.stats['total_operations'] += 1
        
        getcontext().prec = precision + 10
        base_d = Decimal(str(base))
        exp_d = Decimal(str(exponent))
        return base_d ** exp_d
    
    def _cpu_power(self, base: Union[Decimal, float], 
                   exponent: Union[Decimal, float], 
                   precision: int = 50) -> Decimal:
        """🐌 CPU fallback for power"""
        return self._cpu_exponential(base, exponent, precision)
    
    def _cpu_vector_operations(self, array1: list, array2: list, 
                              operation: str = 'add') -> list:
        """🐌 CPU fallback for vector operations"""
        self.stats['cpu_fallbacks'] += 1
        self.stats['total_operations'] += 1
        
        result = []
        for i in range(max(len(array1), len(array2))):
            a = Decimal(str(array1[i % len(array1)]))
            b = Decimal(str(array2[i % len(array2)]))
            
            if operation == 'add':
                result.append(a + b)
            elif operation == 'subtract':
                result.append(a - b)
            elif operation == 'multiply':
                result.append(a * b)
            elif operation == 'divide':
                result.append(a / b if b != 0 else Decimal('0'))
            
        return result
    
    def _cpu_trigonometric_series(self, x: Union[Decimal, float], 
                                 function: str = 'sin', 
                                 terms: int = 50) -> Decimal:
        """🐌 CPU fallback for trigonometric"""
        self.stats['cpu_fallbacks'] += 1
        self.stats['total_operations'] += 1
        
        import math
        x_float = float(x)
        
        if function == 'sin':
            return Decimal(str(math.sin(x_float)))
        elif function == 'cos':
            return Decimal(str(math.cos(x_float)))
        elif function == 'tan':
            return Decimal(str(math.tan(x_float)))
        else:
            return Decimal('0')
    
    def get_performance_stats(self) -> Dict[str, Any]:
        """📊 Get performance statistics"""
        
        total_ops = self.stats['total_operations']
        if total_ops == 0:
            return self.stats.copy()
        
        gpu_percentage = (self.stats['gpu_operations'] / total_ops) * 100
        cpu_percentage = (self.stats['cpu_fallbacks'] / total_ops) * 100
        error_rate = (self.stats['memory_errors'] / total_ops) * 100
        
        return {
            **self.stats,
            'gpu_usage_percentage': round(gpu_percentage, 2),
            'cpu_fallback_percentage': round(cpu_percentage, 2),
            'error_rate_percentage': round(error_rate, 2),
            'efficiency_score': round(gpu_percentage - error_rate, 2)
        }
    
    def print_performance_report(self):
        """📋 Print performance report"""
        
        stats = self.get_performance_stats()
        
        print(f"\n🎯 ADAPTIVE GPU PERFORMANCE REPORT")
        print("=" * 50)
        print(f"🔢 Total Operations: {stats['total_operations']}")
        print(f"🚀 GPU Operations: {stats['gpu_operations']} ({stats.get('gpu_usage_percentage', 0):.1f}%)")
        print(f"🐌 CPU Fallbacks: {stats['cpu_fallbacks']} ({stats.get('cpu_fallback_percentage', 0):.1f}%)")
        print(f"⚠️  Memory Errors: {stats['memory_errors']} ({stats.get('error_rate_percentage', 0):.1f}%)")
        print(f"⭐ Efficiency Score: {stats.get('efficiency_score', 0):.1f}%")
        
        if self.gpu_available:
            print(f"\n⚙️  Current Configuration:")
            print(f"   Memory Limit: {self.config.get('gpu_memory_limit_mb', 0)}MB")
            print(f"   Batch Size: {self.config.get('batch_size', 1000)}")
            print(f"   Min Array Size: {self.config.get('min_array_size_gpu', 5000)}")
            print(f"   Max Array Size: {self.config.get('max_array_size_gpu', 1000000)}")


# Global adaptive delegator instance
adaptive_gpu = AdaptiveGPUDelegator()


def exponential_with_adaptive_gpu(base: Union[Decimal, float], 
                                 exponent: Union[Decimal, float], 
                                 precision: int = 50) -> Decimal:
    """🚀 Adaptive GPU exponential function"""
    return adaptive_gpu.gpu_exponential(base, exponent, precision)


def power_with_adaptive_gpu(base: Union[Decimal, float], 
                           exponent: Union[Decimal, float], 
                           precision: int = 50) -> Decimal:
    """⚡ Adaptive GPU power function"""
    return adaptive_gpu.gpu_power(base, exponent, precision)


def vector_operations_with_adaptive_gpu(array1: list, array2: list, 
                                       operation: str = 'add') -> list:
    """🔢 Adaptive GPU vector operations"""
    return adaptive_gpu.gpu_vector_operations(array1, array2, operation)


if __name__ == "__main__":
    print("🔧 BoneKey Adaptive GPU Delegation System")
    print("Testing GPU compatibility and performance...")
    print()
    
    # Performance test
    import time
    
    print("🧪 Running performance tests...")
    
    # Test exponential operations
    start_time = time.time()
    for i in range(10):
        result = exponential_with_adaptive_gpu(2.718281828, i + 1, 50)
    
    # Test vector operations
    test_array1 = [i for i in range(1000)]
    test_array2 = [i * 2 for i in range(1000)]
    vector_result = vector_operations_with_adaptive_gpu(test_array1, test_array2, 'add')
    
    end_time = time.time()
    
    print(f"✅ Performance test completed in {end_time - start_time:.3f} seconds")
    adaptive_gpu.print_performance_report()
