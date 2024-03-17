
# HFT_on_a_Chip
This project builds on top of the base project (https://github.com/mustafabbas/ECE1373_2016_hft_on_fpga), and focuses on rebuilding the bare-metal parts of an HFT system: encoder/decoder; order book; and trading logic.

As of 2024.3, I have conducted over 300 interative optimizations, and commited over 50 times.

# Performace profiling
To compare original and this proejcts, here is the performance profiling (2024.3):

|               | Fmax          | Latency |
| ------------- |:-------------:| -----:|
| Protocol Encoder/Decoder (original) | 100.3MHz | 39ns |
| Protocol Encoder/Decoder (this project)      | 186.82MHz      |   35ns |
| Order Book (original) | 203.98MHz     |  710ns |
| Order Book (this project) | 204.78MHz | 200ns |
| Trading Logic (original) | 265.75MHz | 10ns|
|Trading Logic (this project) | 149.74MHz | 580ns|

# Contribution
Below is a detailed breakdown of contribution I made to this project, compared with original code:

Trading Logic:

* Rebuild the whole trading logic by introducing RSI (Relative Strenghth Index) and long/short term SMA (Simple Moving Average). This will of course introduce significant latency, but it is closer to real-world trading.
* Use of arbitrary-precision fixed point arithmetics to replace floating points variables.
* Use of static variables within the 'updateMovingAverages' and 'updateRSI' functions allows the state to be preserved across function calls without needing external storage.
* #pragma HLS ARRAY_PARTITION: This directive instructs the HLS (High-Level Synthesis) compiler to partition arrays fully, allowing parallel access to individual elements. 
* #pragma HLS PIPELINE: This directive allows for the implementation of a hardware pipeline, enabling the processing of different parts of the function in parallel.
* The code uses conditional logic (e.g., in calculating gain or loss in 'updateRSI') in a way that minimizes the use of branching and maximizes the use of arithmetic operations.
* In the 'createOrder' function, the order is initialized directly with conditional logic to minimize logic and operations. This reduces the latency and resource usage in creating orders based on trading signals.

Order Book:

* Refactor a large amount of logic by transferring that into inline functions, and move that logic into compile time.
* Code efficiency and latency have been largely improved, there is over 70% improvement on latency.
* Utilize bitwise operations to calculate logarithms and powers of 2, leveraging the hardware's ability to perform these operations rapidly and efficiently. 
* Employ a heap data structure to maintain the order book. Heaps are particularly suitable for order books due to their ability to quickly insert new orders and remove the top order, essential for maintaining bid and ask queues in an HFT system.
* #pragma HLS INLINE eliminates function call overhead by inlining functions.
* #pragma HLS ARRAY_PARTITION enables parallel access to data structure elements by partitioning arrays, which is crucial for the parallel processing capabilities of FPGAs.
* #pragma HLS PIPELINE allows for loop pipelining, significantly increasing the throughput by overlapping loop iterations.
* The code interfaces with input and output streams (stream<order>, stream<Time>, stream<metadata>) to process incoming and outgoing data efficiently.
* Conditional operations, such as those in add_bid, remove_bid, add_ask, and remove_ask functions, are optimized for the decision-making logic that determines how orders are inserted or removed from the heap.

Protocol Encoder/Decoder:

* 'decode_uint8', 'decode_uint32', and 'decode_decimal_to_fix16' are declared inline to eliminate the function call overhead.
* Use bitwise operations (& for masking, << for shifting) to extract and manipulate data efficiently.
* #pragma HLS UNROLL factor=5 and #pragma HLS UNROLL factor=3: These pragmas unroll loops, effectively removing the loop overhead and allowing parallel processing of the loop's iterations.
* Uses ap_uint<> types for its variables, which are arbitrary precision integers that offer efficient bit-level manipulation capabilities. 
* Implement variable-length encoding optimized for compact representation. This technique ensures that smaller values consume fewer bytes, which is advantageous for reducing the size of transmitted messages in HFT applications.
* Handles fixed-point numbers (fix16) by converting them into an integer mantissa with a fixed exponent, optimizing the representation for network transmission.
* Encoding functions, like 'encode_generic_integer', implement variable-length encoding, which reduces the encoded message size by using only as many bytes as needed to represent a value. 
* The encoder simplifies certain operations, such as using fixed exponents for decimal encoding, which might reduce the flexibility of the encoding scheme but can significantly speed up the encoding process by avoiding complex arithmetic operations.


# Running the project
Please be aware: the project targets Alveo U50 Acceleration Card, before building the project, make sure you have added the board in  Vivado, here (https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#vitis) is how to get started with Alveo U50.

The PDF file: code description contains breakdown of each part of code, and design choices.

Please open Tcl console of Vivado, and move to the project folder before building and running:

Step 1: functional verification.

Step 2: HLS and co-simulation.

Step 3: IP generation and program the Alveo U50.

* Step 1 and 2:
   
   For Trading logic,run: vitis_hls -f trading_logic.tcl

   For Order book,run: vitis_hls -f order_book.tcl

   For FAST processor,run: vitis_hls -f FAST_processor.tcl

Please note: Tcl console does not support viewing of HLS and co-simulation report, to view the details, you have to create a project and run HLS/co-sim, and see the results.

* If you have Alveo Acceleration Card, you can go ahead and do step 3 (unfortunately I do not have), here (https://docs.amd.com/v/u/en-US/ug1370-u50-installation) is how to configure and program the Alveo U50.

   


