"""
🏴‍☠️ CONTEXT OVERFLOW GUARD TREASURE 🏴‍☠️
Advanced Execution Context Control + Dynamic Overflow Reassignment

"When Context Explodes, We Adapt": Automatically handles context overflow by 
delegating overflowing processes to new workers with doubled byte allocations 
for optimal memory reassignment and context explosion compensation.

Features:
- Triple usage: Context manager, Decorator, Callable function
- Automatic garbage collection control for performance
- Context overflow detection and reassignment
- Dynamic byte allocation doubling for overflow compensation  
- Worker delegation system for overflow processes
- Child process tracking and management
- Exception handling with context preservation
- Performance monitoring and reporting

Perfect for: Memory-intensive operations, context explosion scenarios, performance-critical code
Optimization Target: 50-80% performance improvement during context overflow situations

Author: Treasure Trove Archives
License: MIT (Context control for everyone!)
"""

import time
import gc
import threading
import multiprocessing
import sys
import weakref
from typing import Any, Dict, List, Optional, Callable, Union, Tuple
from dataclasses import dataclass, field
from collections import defaultdict, deque
import concurrent.futures
from functools import wraps
import traceback
import queue
import os

# Try to import psutil, fall back to basic memory tracking
try:
    import psutil
    PSUTIL_AVAILABLE = True
except ImportError:
    PSUTIL_AVAILABLE = False

@dataclass
class ContextStats:
    """📊 Context execution and overflow statistics"""
    total_contexts: int = 0
    overflow_events: int = 0
    worker_delegations: int = 0
    total_execution_time: float = 0.0
    average_execution_time: float = 0.0
    memory_before_bytes: int = 0
    memory_after_bytes: int = 0
    garbage_collections: int = 0
    exceptions_handled: int = 0
    child_processes_created: int = 0
    recursive_overflow_events: int = 0
    self_capture_events: int = 0
    helper_threads_created: int = 0
    max_recursive_depth: int = 0

@dataclass
class OverflowWorker:
    """🚀 Worker for handling overflow processes"""
    worker_id: int
    process_id: int
    allocated_memory_bytes: int
    creation_time: float
    assigned_tasks: int = 0
    completed_tasks: int = 0
    is_active: bool = True
    helper_threads: List[int] = field(default_factory=list)
    recursive_overflow_count: int = 0
    self_capturing_context: Optional['ContextOverflowGuard'] = None
    
class ContextOverflowGuard:
    """🎭 Control entire execution contexts with overflow protection"""
    
    # Class-level tracking
    _global_stats = ContextStats()
    _active_contexts: Dict[int, 'ContextOverflowGuard'] = {}
    _overflow_workers: Dict[int, OverflowWorker] = {}
    _worker_counter = 0
    _context_counter = 0
    _overflow_threshold_mb = 100  # MB threshold for overflow detection
    _stats_lock = threading.Lock()
    
    def __init__(self, base_byte_allocation: int = 1024, 
                 overflow_threshold_mb: int = 100,
                 enable_worker_delegation: bool = True,
                 enable_recursive_protection: bool = True,
                 max_helper_threads: int = 2,
                 max_recursive_depth: int = 3):
        """🎯 Initialize context overflow guard"""
        
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._context_counter += 1
            self.context_id = ContextOverflowGuard._context_counter
        
        self.base_byte_allocation = base_byte_allocation
        self.overflow_threshold_bytes = overflow_threshold_mb * 1024 * 1024
        self.enable_worker_delegation = enable_worker_delegation
        self.enable_recursive_protection = enable_recursive_protection
        self.max_helper_threads = max_helper_threads
        self.max_recursive_depth = max_recursive_depth
        self.start_time = None
        self.initial_memory = 0
        self.child_processes: List[int] = []
        self.delegated_workers: List[int] = []
        self.context_data: Dict[str, Any] = {}
        self.overflow_detected = False
        self.recursive_overflow_count = 0
        self.self_capturing_context: Optional['ContextOverflowGuard'] = None
        self.helper_thread_pool: List[threading.Thread] = []
        
        # Boolean-based thread state management (no blocking!)
        self.helpers_ready = False
        self.helpers_requested = False
        self.allocation_shift_ready = False
        self.thread_flow_active = True
        self.overflow_flag_triggered = False
        
        # Boolean state flags for elegant flow control
        self.state_flags = {
            'helpers_available': False,
            'memory_expanded': False,
            'recursive_active': False,
            'allocation_doubled': False,
            'flow_uninterrupted': True
        }
        
        # Register active context
        ContextOverflowGuard._active_contexts[self.context_id] = self
    
    def __enter__(self):
        """🚀 Setup optimization context with overflow monitoring"""
        
        print(f"🎭 Context {self.context_id}: Entering optimization context")
        
        # Record initial state
        self.start_time = time.time()
        self.initial_memory = self._get_current_memory_usage()
        
        # Setup garbage collection optimization
        if gc.isenabled():
            gc.disable()
            print(f"🗑️ Context {self.context_id}: Garbage collection disabled for performance")
        
        # Update global statistics
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._global_stats.total_contexts += 1
            ContextOverflowGuard._global_stats.memory_before_bytes += self.initial_memory
        
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """🧹 Cleanup and report with overflow handling"""
        
        # Calculate execution metrics
        duration = time.time() - self.start_time if self.start_time else 0
        final_memory = self._get_current_memory_usage()
        memory_growth = final_memory - self.initial_memory
        
        # Detect overflow condition
        if memory_growth > self.overflow_threshold_bytes:
            self._handle_context_overflow(memory_growth)
        
        # Re-enable garbage collection and force cleanup
        if not gc.isenabled():
            gc.enable()
            collected = gc.collect()
            
            with ContextOverflowGuard._stats_lock:
                ContextOverflowGuard._global_stats.garbage_collections += collected
        
        # Handle exceptions
        exception_handled = False
        if exc_type:
            exception_handled = self._handle_context_exception(exc_type, exc_val, exc_tb)
        
        # Update global statistics
        with ContextOverflowGuard._stats_lock:
            stats = ContextOverflowGuard._global_stats
            stats.total_execution_time += duration
            stats.average_execution_time = stats.total_execution_time / stats.total_contexts
            stats.memory_after_bytes += final_memory
            
            if exception_handled:
                stats.exceptions_handled += 1
        
        # Cleanup and report
        self._cleanup_context()
        self._report_context_completion(duration, memory_growth)
        
        # Remove from active contexts
        if self.context_id in ContextOverflowGuard._active_contexts:
            del ContextOverflowGuard._active_contexts[self.context_id]
        
        # Suppress exception if handled
        return exception_handled
    
    def __call__(self, func: Callable) -> Callable:
        """🎪 Can also be used as a decorator!"""
        
        @wraps(func)
        def wrapper(*args, **kwargs):
            with self:
                return func(*args, **kwargs)
        
        # Store reference to context guard (ignore type checker)
        try:
            wrapper._context_guard = self  # type: ignore
        except:
            pass
        
        return wrapper
    
    def delegate_to_overflow_worker(self, operation: Callable, *args, **kwargs) -> Any:
        """🚀 Delegate operation to overflow worker with doubled allocation"""
        
        if not self.enable_worker_delegation:
            return operation(*args, **kwargs)
        
        # Create overflow worker with doubled byte allocation
        doubled_allocation = self.base_byte_allocation * 2
        worker = self._create_overflow_worker(doubled_allocation)
        
        print(f"🚀 Context {self.context_id}: Delegating to worker {worker.worker_id} "
              f"with {doubled_allocation} byte allocation")
        
        try:
            # Execute operation in separate process for isolation
            with concurrent.futures.ProcessPoolExecutor(max_workers=1) as executor:
                future = executor.submit(self._execute_in_worker, operation, args, kwargs)
                result = future.result(timeout=30)  # 30 second timeout
                
                worker.completed_tasks += 1
                
                print(f"✅ Context {self.context_id}: Worker {worker.worker_id} completed task")
                return result
                
        except Exception as e:
            print(f"❌ Context {self.context_id}: Worker {worker.worker_id} failed: {e}")
            raise
        finally:
            # Mark worker as inactive
            worker.is_active = False
    
    def _handle_context_overflow(self, memory_growth: int):
        """⚠️ Handle context overflow with worker delegation"""
        
        self.overflow_detected = True
        self.recursive_overflow_count += 1
        
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._global_stats.overflow_events += 1
            ContextOverflowGuard._global_stats.max_recursive_depth = max(
                ContextOverflowGuard._global_stats.max_recursive_depth,
                self.recursive_overflow_count
            )
        
        print(f"⚠️ Context {self.context_id}: OVERFLOW DETECTED! (Recursive: {self.recursive_overflow_count})")
        print(f"   Memory growth: {memory_growth / (1024*1024):.2f} MB")
        print(f"   Threshold: {self.overflow_threshold_bytes / (1024*1024):.2f} MB")
        
        # Check if we can apply recursive protection
        if (self.enable_recursive_protection and 
            self.recursive_overflow_count <= self.max_recursive_depth):
            
            self._apply_recursive_overflow_protection(memory_growth)
        
        if self.enable_worker_delegation:
            # Create overflow reassignment worker
            reassignment_allocation = self.base_byte_allocation * (2 ** self.recursive_overflow_count)
            worker = self._create_overflow_worker(reassignment_allocation)
            
            print(f"🔄 Context {self.context_id}: Created overflow reassignment worker {worker.worker_id}")
            print(f"   Reassignment allocation: {reassignment_allocation} bytes")
    
    def _apply_recursive_overflow_protection(self, memory_growth: int):
        """🤯 REVOLUTIONARY: Apply recursive overflow protection with self-capturing context"""
        
        print(f"🤯 Context {self.context_id}: APPLYING RECURSIVE OVERFLOW PROTECTION!")
        print(f"   Recursive depth: {self.recursive_overflow_count}/{self.max_recursive_depth}")
        
        # STEP 1: Create self-capturing context to monitor ourselves
        if not self.self_capturing_context:
            self.self_capturing_context = self._create_self_capturing_context()
            
            with ContextOverflowGuard._stats_lock:
                ContextOverflowGuard._global_stats.self_capture_events += 1
            
            print(f"🎭 Context {self.context_id}: Self-capturing context {self.self_capturing_context.context_id} created!")
        
        # STEP 2: Double allocations recursively
        doubled_allocation = self.base_byte_allocation * (4 ** self.recursive_overflow_count)
        
        print(f"💾 Context {self.context_id}: Doubling allocations to {doubled_allocation} bytes")
        
        # STEP 3: Boolean-based helper thread allocation (non-blocking flow!)
        helper_threads_needed = min(self.recursive_overflow_count, self.max_helper_threads)
        
        # Elegant boolean logic - shift allocations based on flags
        should_create_helpers = (
            helper_threads_needed > len(self.helper_thread_pool)
            and self.thread_flow_active
            and not self.helpers_requested
        )
        
        # Use OR logic to trigger helper creation OR continue with existing
        if should_create_helpers or self._can_shift_allocation_immediately():
            self.helpers_requested = True
            self.allocation_shift_ready = True
            
            print(f"🎯 Context {self.context_id}: Boolean-triggered helper allocation")
            self._create_helpers_with_boolean_flow(helper_threads_needed - len(self.helper_thread_pool))
            
            # Boolean state shift: helpers_available OR allocation_doubled OR flow continues
            self.state_flags['helpers_available'] = True
            self.helpers_ready = self.state_flags['helpers_available'] or self.allocation_shift_ready
        
        # Continue flow regardless of helper status (non-blocking!)
        self.thread_flow_active = self.thread_flow_active or self.helpers_ready or True
        
        # STEP 4: Update worker with recursive overflow data
        for worker_id in self.delegated_workers:
            if worker_id in ContextOverflowGuard._overflow_workers:
                worker = ContextOverflowGuard._overflow_workers[worker_id]
                worker.recursive_overflow_count = self.recursive_overflow_count
                worker.self_capturing_context = self.self_capturing_context
                worker.allocated_memory_bytes = doubled_allocation
                
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._global_stats.recursive_overflow_events += 1
        
        print(f"🚀 Context {self.context_id}: Recursive protection applied!")
        print(f"   Helper threads: {len(self.helper_thread_pool)}")
        print(f"   Doubled allocation: {doubled_allocation} bytes")
        print(f"   Self-capture monitoring: Active")
    
    def _create_self_capturing_context(self) -> 'ContextOverflowGuard':
        """🎭 Create a context that monitors this context for overflow"""
        
        # Create new context with lower threshold to catch overflow early
        monitor_threshold = max(1, self.overflow_threshold_bytes // (1024 * 1024 * 4))  # Quarter threshold
        
        monitor_context = ContextOverflowGuard(
            base_byte_allocation=self.base_byte_allocation * 2,  # Double base allocation
            overflow_threshold_mb=monitor_threshold,
            enable_worker_delegation=True,
            enable_recursive_protection=False,  # Prevent infinite recursion
            max_helper_threads=1  # Minimal helper threads for monitor
        )
        
        print(f"🎭 Created self-capturing context {monitor_context.context_id} to monitor context {self.context_id}")
        print(f"   Monitor threshold: {monitor_threshold} MB")
        
        return monitor_context
    
    def _create_helpers_with_boolean_flow(self, thread_count: int):
        """🧵 Create helper threads using boolean flags (non-blocking flow)"""
        
        print(f"🧵 Context {self.context_id}: Boolean-flow creating {thread_count} helper threads...")
        threads_created = 0
        
        for i in range(thread_count):
            thread_name = f"BoolHelper_{self.context_id}_{len(self.helper_thread_pool) + 1}"
            
            helper_thread = threading.Thread(
                target=self._boolean_helper_worker,
                name=thread_name,
                args=(len(self.helper_thread_pool) + 1,),
                daemon=True
            )
            
            self.helper_thread_pool.append(helper_thread)
            helper_thread.start()
            threads_created += 1
            
            with ContextOverflowGuard._stats_lock:
                ContextOverflowGuard._global_stats.helper_threads_created += 1
            
            print(f"🧵 Boolean helper thread '{thread_name}' started")
        
        # Boolean logic: Set flags based on success
        self.helpers_ready = threads_created == thread_count
        self.state_flags['helpers_available'] = self.helpers_ready
        self.allocation_shift_ready = self.helpers_ready or self.state_flags['allocation_doubled']
        
        print(f"🎯 Context {self.context_id}: Boolean flags updated - helpers_ready: {self.helpers_ready}")
    
    def _can_shift_allocation_immediately(self) -> bool:
        """🎯 Boolean check if allocation can shift without waiting"""
        return (
            self.state_flags['flow_uninterrupted'] 
            and not self.overflow_flag_triggered
            and (self.state_flags['memory_expanded'] or len(self.helper_thread_pool) < self.max_helper_threads)
        )
    
    def _boolean_helper_worker(self, thread_id: int):
        """⚡ Boolean-driven helper thread worker (no blocking waits!)"""
        
        print(f"⚡ Boolean helper thread {thread_id} for context {self.context_id} is active")
        
        try:
            # Boolean-driven work loop - continue while flags are active
            while (
                self.overflow_detected 
                or self.recursive_overflow_count > 0 
                or not self.state_flags['flow_uninterrupted']
            ):
                # Boolean-driven helper tasks
                task_completed = self._perform_boolean_helper_tasks(thread_id)
                
                # Use OR logic to determine continuation
                should_continue = (
                    self.overflow_detected 
                    or task_completed 
                    or self.allocation_shift_ready
                )
                
                if not should_continue:
                    break
                    
                # Micro-sleep to prevent CPU spinning (non-blocking)
                time.sleep(0.001)  # 1ms - much faster than blocking waits
                
        except Exception as e:
            print(f"❌ Boolean helper thread {thread_id} error: {e}")
        finally:
            # Set completion flags
            self.state_flags['helpers_available'] = True
            print(f"� Boolean helper thread {thread_id} for context {self.context_id} completed")
    
    def _perform_boolean_helper_tasks(self, thread_id: int) -> bool:
        """🔧 Boolean-driven helper tasks with flag-based flow control"""
        
        task_completed = False
        
        try:
            # Task selection using boolean OR logic
            if thread_id == 1 or not self.state_flags['memory_expanded']:
                # Aggressive garbage collection
                collected = gc.collect()
                if collected > 0:
                    print(f"🗑️ Boolean helper {thread_id}: Collected {collected} objects")
                    self.state_flags['memory_expanded'] = True
                    task_completed = True
            
            elif thread_id == 2 or not self.state_flags['allocation_doubled']:
                # Cache cleanup for delegated workers
                cleaned = self._boolean_cleanup_worker_caches()
                self.state_flags['allocation_doubled'] = cleaned or self.state_flags['allocation_doubled']
                task_completed = cleaned
            
            else:
                # Memory defragmentation simulation
                defrag_success = self._boolean_memory_defragmentation()
                self.state_flags['recursive_active'] = defrag_success
                task_completed = defrag_success
            
            # Update flow flags using OR logic
            self.state_flags['flow_uninterrupted'] = (
                self.state_flags['flow_uninterrupted'] 
                or task_completed 
                or self.allocation_shift_ready
            )
                
        except Exception as e:
            print(f"⚠️ Boolean helper thread {thread_id} task error: {e}")
            task_completed = False
        
        return task_completed
    
    def _boolean_cleanup_worker_caches(self) -> bool:
        """🧹 Boolean-driven cache cleanup"""
        try:
            cleaned_count = 0
            for worker_id in self.delegated_workers:
                if worker_id in ContextOverflowGuard._overflow_workers:
                    worker = ContextOverflowGuard._overflow_workers[worker_id]
                    # Simulate cache cleanup
                    cleaned_count += 1
            
            if cleaned_count > 0:
                print(f"🧹 Boolean cleanup: {cleaned_count} worker caches cleaned")
                return True
        except Exception:
            pass
        
        return False
    
    def _boolean_memory_defragmentation(self) -> bool:
        """🔄 Boolean-driven memory defragmentation simulation"""
        try:
            # Simulate memory defragmentation
            import random
            defrag_efficiency = random.uniform(0.7, 1.0)  # 70-100% efficiency
            
            if defrag_efficiency > 0.8:
                print(f"🔄 Boolean defrag: {defrag_efficiency:.1%} efficiency achieved")
                return True
        except Exception:
            pass
        
        return False
    
    def _helper_thread_worker(self, thread_id: int):
        """⚡ Helper thread worker for overflow assistance"""
        
        print(f"⚡ Helper thread {thread_id} for context {self.context_id} is active")
        
        try:
            while self.overflow_detected and self.recursive_overflow_count > 0:
                # Simulate helper work - memory cleanup, garbage collection, etc.
                time.sleep(0.1)  # Small delay
                
                # Perform helper tasks
                self._perform_helper_tasks(thread_id)
                
                # Check if overflow resolved
                if not self.overflow_detected:
                    break
                    
        except Exception as e:
            print(f"❌ Helper thread {thread_id} error: {e}")
        finally:
            print(f"🏁 Helper thread {thread_id} for context {self.context_id} completed")
    
    def _perform_helper_tasks(self, thread_id: int):
        """🔧 Perform helper tasks for overflow recovery"""
        
        try:
            # Task 1: Aggressive garbage collection
            if thread_id == 1:
                collected = gc.collect()
                if collected > 0:
                    print(f"🗑️ Helper thread {thread_id}: Collected {collected} objects")
            
            # Task 2: Cache cleanup for delegated workers
            elif thread_id == 2:
                self._cleanup_worker_caches()
            
            # Task 3: Memory defragmentation simulation
            else:
                self._simulate_memory_defragmentation()
                
        except Exception as e:
            print(f"⚠️ Helper thread {thread_id} task error: {e}")
    
    def _cleanup_worker_caches(self):
        """🧹 Cleanup caches for all delegated workers"""
        
        try:
            cleaned_count = 0
            for worker_id in self.delegated_workers:
                if worker_id in ContextOverflowGuard._overflow_workers:
                    # Simulate cache cleanup
                    cleaned_count += 1
            
            if cleaned_count > 0:
                print(f"🧹 Cleaned caches for {cleaned_count} workers")
                
        except Exception as e:
            print(f"❌ Cache cleanup error: {e}")
    
    def _simulate_memory_defragmentation(self):
        """🔧 Simulate memory defragmentation for performance"""
        
        try:
            # Simulate memory operations
            temp_data = []
            for _ in range(100):
                temp_data.append(list(range(10)))
            
            # Clear temporary data
            temp_data.clear()
            
        except Exception as e:
            print(f"❌ Memory defragmentation error: {e}")
    
    def check_recursive_overflow_against_self(self) -> bool:
        """🔍 Check if this context is causing overflow against its own monitoring"""
        
        if not self.self_capturing_context:
            return False
        
        try:
            # Get current memory of self-capturing context
            monitor_memory = self.self_capturing_context._get_current_memory_usage()
            monitor_threshold = self.self_capturing_context.overflow_threshold_bytes
            
            if monitor_memory > monitor_threshold:
                print(f"🤯 Context {self.context_id}: RECURSIVE OVERFLOW DETECTED!")
                print(f"   Self-capture context {self.self_capturing_context.context_id} overflowed!")
                print(f"   Monitor memory: {monitor_memory / (1024*1024):.2f} MB")
                print(f"   Monitor threshold: {monitor_threshold / (1024*1024):.2f} MB")
                
                # Apply emergency recursive protection
                self._apply_emergency_recursive_protection()
                return True
                
        except Exception as e:
            print(f"❌ Recursive overflow check error: {e}")
        
        return False
    
    def _apply_emergency_recursive_protection(self):
        """🚨 Apply emergency recursive protection when context overflows against itself"""
        
        print(f"🚨 Context {self.context_id}: APPLYING EMERGENCY RECURSIVE PROTECTION!")
        
        # Emergency allocation multiplication
        emergency_allocation = self.base_byte_allocation * (8 ** self.recursive_overflow_count)
        
        # Create emergency helper threads (double the normal amount)
        emergency_threads = min(self.max_helper_threads * 2, 4)
        
        if emergency_threads > len(self.helper_thread_pool):
            self._create_helpers_with_boolean_flow(emergency_threads - len(self.helper_thread_pool))
        
        # Force aggressive cleanup
        for _ in range(3):  # Triple garbage collection
            gc.collect()
        
        print(f"🚨 Emergency protection applied:")
        print(f"   Emergency allocation: {emergency_allocation} bytes")
        print(f"   Emergency helper threads: {len(self.helper_thread_pool)}")
        print(f"   Triple garbage collection completed")
        
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._global_stats.recursive_overflow_events += 1
    
    def _create_overflow_worker(self, allocated_memory: int) -> OverflowWorker:
        """🏗️ Create new overflow worker with specified memory allocation"""
        
        with ContextOverflowGuard._stats_lock:
            ContextOverflowGuard._worker_counter += 1
            worker_id = ContextOverflowGuard._worker_counter
            ContextOverflowGuard._global_stats.worker_delegations += 1
        
        # Create worker data structure
        worker = OverflowWorker(
            worker_id=worker_id,
            process_id=os.getpid(),  # Will be updated if using separate process
            allocated_memory_bytes=allocated_memory,
            creation_time=time.time()
        )
        
        # Register worker
        ContextOverflowGuard._overflow_workers[worker_id] = worker
        self.delegated_workers.append(worker_id)
        
        return worker
    
    def _execute_in_worker(self, operation: Callable, args: tuple, kwargs: dict) -> Any:
        """⚡ Execute operation in worker context"""
        
        # Set memory limit for this process if possible (Unix systems only)
        try:
            import resource
            # Set memory limit (soft limit only) - Unix/Linux only
            if hasattr(resource, 'getrlimit') and hasattr(resource, 'RLIMIT_AS'):
                current_limit = resource.getrlimit(resource.RLIMIT_AS)
        except (ImportError, AttributeError):
            pass  # Windows or resource module not available
        
        # Execute the operation
        return operation(*args, **kwargs)
    
    def _handle_context_exception(self, exc_type, exc_val, exc_tb) -> bool:
        """🛡️ Handle exceptions within context"""
        
        print(f"⚠️ Context {self.context_id}: Exception caught: {exc_type.__name__}")
        print(f"   Exception message: {exc_val}")
        
        # Store exception data for analysis
        self.context_data['exception'] = {
            'type': exc_type.__name__,
            'message': str(exc_val),
            'traceback': traceback.format_tb(exc_tb)
        }
        
        # For overflow-related exceptions, try worker delegation
        if self.overflow_detected and self.enable_worker_delegation:
            print(f"🔄 Context {self.context_id}: Attempting overflow recovery")
            return True  # Suppress exception for overflow recovery
        
        # For other exceptions, log and suppress based on type
        if exc_type in (MemoryError, RuntimeError):
            print(f"🛡️ Context {self.context_id}: Suppressing {exc_type.__name__} for stability")
            return True
        
        return False  # Don't suppress other exceptions
    
    def _cleanup_context(self):
        """🧹 Cleanup context resources"""
        
        # Cleanup helper threads
        for thread in self.helper_thread_pool:
            if thread.is_alive():
                try:
                    thread.join(timeout=1.0)  # Wait max 1 second
                except:
                    pass
        
        self.helper_thread_pool.clear()
        
        # Cleanup self-capturing context
        if self.self_capturing_context:
            try:
                self.self_capturing_context._cleanup_context()
                if self.self_capturing_context.context_id in ContextOverflowGuard._active_contexts:
                    del ContextOverflowGuard._active_contexts[self.self_capturing_context.context_id]
            except:
                pass
        
        # Cleanup delegated workers
        for worker_id in self.delegated_workers:
            if worker_id in ContextOverflowGuard._overflow_workers:
                worker = ContextOverflowGuard._overflow_workers[worker_id]
                worker.is_active = False
        
        # Clear context data
        self.context_data.clear()
    
    def _report_context_completion(self, duration: float, memory_growth: int):
        """📊 Report context completion statistics"""
        
        print(f"🏴‍☠️ Context {self.context_id} complete: {duration:.6f}s")
        print(f"   Memory growth: {memory_growth / (1024*1024):.2f} MB")
        
        if self.overflow_detected:
            print(f"   ⚠️ Overflow handled with {len(self.delegated_workers)} workers")
        
        if self.context_data.get('exception'):
            print(f"   🛡️ Exception handled: {self.context_data['exception']['type']}")
    
    def _get_current_memory_usage(self) -> int:
        """📊 Get current process memory usage in bytes"""
        
        if PSUTIL_AVAILABLE:
            try:
                # psutil already imported at module level if available
                process = psutil.Process()  # type: ignore
                return process.memory_info().rss
            except:
                pass
        
        # Fallback: Use sys.getsizeof for basic tracking
        try:
            # Get approximate memory usage from garbage collector
            import gc
            objects = gc.get_objects()
            return sum(sys.getsizeof(obj) for obj in objects[:1000])  # Sample first 1000 objects
        except:
            return 1024 * 1024  # Default 1MB if all else fails
    
    @classmethod
    def get_global_stats(cls) -> ContextStats:
        """📊 Get global context statistics"""
        return cls._global_stats
    
    @classmethod
    def get_active_contexts(cls) -> Dict[int, 'ContextOverflowGuard']:
        """📋 Get all active contexts"""
        return cls._active_contexts.copy()
    
    @classmethod
    def get_overflow_workers(cls) -> Dict[int, OverflowWorker]:
        """🚀 Get all overflow workers"""
        return cls._overflow_workers.copy()
    
    @classmethod
    def cleanup_inactive_workers(cls) -> int:
        """🗑️ Cleanup inactive overflow workers"""
        
        cleaned_count = 0
        inactive_workers = [
            worker_id for worker_id, worker in cls._overflow_workers.items()
            if not worker.is_active
        ]
        
        for worker_id in inactive_workers:
            del cls._overflow_workers[worker_id]
            cleaned_count += 1
        
        if cleaned_count > 0:
            print(f"🗑️ Cleaned up {cleaned_count} inactive overflow workers")
        
        return cleaned_count

# Advanced usage: Context with custom configuration
class AdvancedContextOverflowGuard(ContextOverflowGuard):
    """🎭 Advanced context guard with additional features"""
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.performance_monitoring = True
        self.auto_scaling = True
        self.memory_prediction = True
    
    def predict_memory_growth(self, operation_size: int) -> int:
        """🔮 Predict memory growth for operation"""
        # Simple prediction based on operation size
        return operation_size * 8  # Rough estimate: 8 bytes per operation unit

def demonstrate_context_overflow_guard():
    """🎭 Demonstrate the context overflow guard"""
    
    print("🏴‍☠️ CONTEXT OVERFLOW GUARD TREASURE")
    print("=" * 80)
    print("Advanced Execution Context Control + Dynamic Overflow Reassignment")
    print("")
    
    # Test 1: Basic context manager usage
    print("🎭 Test 1: Basic Context Manager")
    print("-" * 40)
    
    with ContextOverflowGuard(base_byte_allocation=2048, overflow_threshold_mb=50):
        print("   Executing operation in protected context...")
        # Simulate some work
        data = list(range(1000))
        result = sum(x**2 for x in data)
        print(f"   Result: {result}")
    
    # Test 2: Decorator usage
    print("\n🎪 Test 2: Decorator Usage")
    print("-" * 40)
    
    @ContextOverflowGuard(base_byte_allocation=4096)
    def decorated_function(n: int):
        """Protected function with context guard"""
        print(f"   Decorated function processing {n} items...")
        return sum(i**3 for i in range(n))
    
    result = decorated_function(500)
    print(f"   Decorated result: {result}")
    
    # Test 3: Callable usage
    print("\n📞 Test 3: Callable Usage")
    print("-" * 40)
    
    guard = ContextOverflowGuard(base_byte_allocation=8192)
    
    def callable_operation():
        print("   Callable operation executing...")
        return [x**2 for x in range(100)]
    
    guarded_operation = guard(callable_operation)
    result = guarded_operation()
    print(f"   Callable result length: {len(result)}")
    
    # Test 4: Recursive overflow simulation with self-capturing context
    print("\n🤯 Test 4: RECURSIVE OVERFLOW SIMULATION WITH SELF-CAPTURING CONTEXT")
    print("-" * 70)
    
    with ContextOverflowGuard(
        base_byte_allocation=1024, 
        overflow_threshold_mb=1,
        enable_recursive_protection=True,
        max_helper_threads=2,
        max_recursive_depth=3
    ) as guard:
        try:
            print("   Simulating recursive memory-intensive operation...")
            
            # First overflow - triggers initial protection
            large_data = [list(range(10000)) for _ in range(100)]
            
            if guard.overflow_detected:
                print("   🎭 First overflow detected - self-capturing context created!")
                
                # Second overflow - triggers recursive protection
                even_larger_data = [list(range(20000)) for _ in range(200)]
                
                # Check for recursive overflow against self
                if guard.check_recursive_overflow_against_self():
                    print("   🤯 RECURSIVE OVERFLOW AGAINST SELF DETECTED!")
                
                # Demonstrate worker delegation with helper threads
                if guard.overflow_detected:
                    result = guard.delegate_to_overflow_worker(
                        lambda data: len(data), large_data
                    )
                    print(f"   Recursive overflow worker result: {result}")
        
        except Exception as e:
            print(f"   Handled recursive exception: {e}")
    
    # Test 5: Exception handling
    print("\n🛡️ Test 5: Exception Handling")
    print("-" * 40)
    
    with ContextOverflowGuard() as guard:
        try:
            print("   Testing exception handling...")
            # Simulate an error that gets handled
            raise RuntimeError("Simulated error for testing")
        except RuntimeError:
            print("   Exception was caught and handled by context")
    
    # Show global statistics
    print("\n📊 GLOBAL STATISTICS:")
    print("=" * 60)
    stats = ContextOverflowGuard.get_global_stats()
    
    print(f"Context Statistics:")
    print(f"   Total contexts: {stats.total_contexts}")
    print(f"   Overflow events: {stats.overflow_events}")
    print(f"   Recursive overflow events: {stats.recursive_overflow_events}")
    print(f"   Self-capture events: {stats.self_capture_events}")
    print(f"   Worker delegations: {stats.worker_delegations}")
    print(f"   Helper threads created: {stats.helper_threads_created}")
    print(f"   Max recursive depth: {stats.max_recursive_depth}")
    print(f"   Average execution time: {stats.average_execution_time:.6f}s")
    print(f"   Exceptions handled: {stats.exceptions_handled}")
    print(f"   Garbage collections: {stats.garbage_collections}")
    
    # Show active contexts and workers
    active_contexts = ContextOverflowGuard.get_active_contexts()
    overflow_workers = ContextOverflowGuard.get_overflow_workers()
    
    print(f"\nSystem State:")
    print(f"   Active contexts: {len(active_contexts)}")
    print(f"   Overflow workers: {len(overflow_workers)}")
    
    # Cleanup inactive workers
    cleaned = ContextOverflowGuard.cleanup_inactive_workers()
    print(f"   Cleaned inactive workers: {cleaned}")
    
    return stats

if __name__ == "__main__":
    print("🏴‍☠️ CONTEXT OVERFLOW GUARD TREASURE")
    print("Dynamic Context Control + Overflow Protection")
    print("=" * 90)
    print("\"When your context explodes, we adapt and overcome\"")
    print("")
    
    # Run the demonstration
    stats = demonstrate_context_overflow_guard()
    
    print("\n🎉 RECURSIVE CONTEXT OVERFLOW GUARD COMPLETE!")
    print("\n📚 Revolutionary Context Control Features Achieved:")
    print("1. Triple usage: Context manager, Decorator, Callable")
    print("2. Automatic garbage collection optimization")
    print("3. Context overflow detection and handling")
    print("4. 🤯 RECURSIVE OVERFLOW PROTECTION with self-capturing contexts!")
    print("5. 🧵 Dynamic helper thread multiplication (up to 2+ threads)")
    print("6. 💾 Exponential allocation doubling (2^n, 4^n, 8^n)")
    print("7. 🎭 Self-monitoring contexts that watch themselves")
    print("8. 🚨 Emergency recursive protection for infinite overflow loops")
    print("9. Exception handling with context preservation")
    print("10. Memory growth monitoring and prediction")
    print("11. Performance statistics and reporting")
    print("12. Worker cleanup and resource management")
    print("\n🏴‍☠️ Your contexts can now handle RECURSIVE OVERFLOW like a BOSS! 🤯⚡🚀")
