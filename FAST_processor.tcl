open_project project_FAST -reset
set_top fast_protocol
add_files FAST_processor/decoder.cpp
add_files FAST_processor/decoder.h
add_files FAST_processor/encoder.cpp
add_files FAST_processor/encoder.h
add_files FAST_processor/fast.cpp
add_files FAST_processor/fast.h
add_files -tb FAST_processor/tb.cpp
add_files -tb FAST_processor/in.dat
add_files -tb FAST_processor/out.dat
open_solution "solution1" -reset
set_part {xcu50-fsvh2104-2-e} 
create_clock -period 10 -name default
csim_design  -argv {in.dat out.dat}
exit