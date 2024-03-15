open_project project_oder_book -reset
set_top order_book
add_files Order_book/order_book.cpp
add_files Order_book/order_book.hpp
add_files -tb Order_book/tb.cpp
open_solution "solution1"
set_part {xcu50-fsvh2104-2-e} 
create_clock -period 10 -name default
csim_design
exit



