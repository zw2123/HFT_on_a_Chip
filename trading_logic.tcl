open_project -reset project_trading_logic
set_top trading_logic
add_files Trading_logic/trading_logic.cpp
add_files Trading_logic/trading_logic.hpp
add_files -tb Trading_logic/tb.cpp
open_solution "solution1" -reset
set_part {xcu50-fsvh2104-2-e} 
create_clock -period {10} -name default 
csim_design
exit
