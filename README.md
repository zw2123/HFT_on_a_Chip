Please be aware: the project targets Alveo U50 Acceleration Card, before building the project, make sure you have added the board in  Vivado, here (https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#vitis) is how to get started wit Alveo U50.


Please open Tcl console of Vivado, and move to the project folder before building and running:

Step 1: functional verification.
Step 2: HLS and co-simulation.
Step 3: IP generation and program the Alveo U50.

Please note: Tcl console does not support viewing of HLS and co-simulation report, to view the details, you have to create a project and run HLS/co-sim, and see the results.

Step 1 and 2: 
   For Trading logic: run: vitis_hls -f trading_logic.tcl
   For Order book: run: vitis_hls -f order_book.tcl
   For FAST processor: run: vitis_hls -f FAST_processor.tcl
The latency after co-sim are (10ns clock period):
  Trading logic:94-125 ns;
  Order book: 350-400 ns;
  Fast processor: 300ns.

If you have Alveo Acceleration Card, you can go ahead and do step 3 (unfortunately I do not have), here (https://docs.amd.com/v/u/en-US/ug1370-u50-installation) is how to configure and program the Alveo U50.
