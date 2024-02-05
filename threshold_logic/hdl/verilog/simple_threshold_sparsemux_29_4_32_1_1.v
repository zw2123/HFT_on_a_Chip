// ==============================================================
// Generated by Vitis HLS v2023.2.1
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
// ==============================================================
// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
`timescale 1ns / 1ps

module simple_threshold_sparsemux_29_4_32_1_1 (din0,din1,din2,din3,din4,din5,din6,din7,din8,din9,din10,din11,din12,din13,def,sel,dout);

parameter din0_WIDTH = 1;

parameter din1_WIDTH = 1;

parameter din2_WIDTH = 1;

parameter din3_WIDTH = 1;

parameter din4_WIDTH = 1;

parameter din5_WIDTH = 1;

parameter din6_WIDTH = 1;

parameter din7_WIDTH = 1;

parameter din8_WIDTH = 1;

parameter din9_WIDTH = 1;

parameter din10_WIDTH = 1;

parameter din11_WIDTH = 1;

parameter din12_WIDTH = 1;

parameter din13_WIDTH = 1;

parameter def_WIDTH = 1;
parameter sel_WIDTH = 1;
parameter dout_WIDTH = 1;

parameter [sel_WIDTH-1:0] CASE0 = 1;

parameter [sel_WIDTH-1:0] CASE1 = 1;

parameter [sel_WIDTH-1:0] CASE2 = 1;

parameter [sel_WIDTH-1:0] CASE3 = 1;

parameter [sel_WIDTH-1:0] CASE4 = 1;

parameter [sel_WIDTH-1:0] CASE5 = 1;

parameter [sel_WIDTH-1:0] CASE6 = 1;

parameter [sel_WIDTH-1:0] CASE7 = 1;

parameter [sel_WIDTH-1:0] CASE8 = 1;

parameter [sel_WIDTH-1:0] CASE9 = 1;

parameter [sel_WIDTH-1:0] CASE10 = 1;

parameter [sel_WIDTH-1:0] CASE11 = 1;

parameter [sel_WIDTH-1:0] CASE12 = 1;

parameter [sel_WIDTH-1:0] CASE13 = 1;

parameter ID = 1;
parameter NUM_STAGE = 1;



input [din0_WIDTH-1:0] din0;

input [din1_WIDTH-1:0] din1;

input [din2_WIDTH-1:0] din2;

input [din3_WIDTH-1:0] din3;

input [din4_WIDTH-1:0] din4;

input [din5_WIDTH-1:0] din5;

input [din6_WIDTH-1:0] din6;

input [din7_WIDTH-1:0] din7;

input [din8_WIDTH-1:0] din8;

input [din9_WIDTH-1:0] din9;

input [din10_WIDTH-1:0] din10;

input [din11_WIDTH-1:0] din11;

input [din12_WIDTH-1:0] din12;

input [din13_WIDTH-1:0] din13;

input [def_WIDTH-1:0] def;
input [sel_WIDTH-1:0] sel;

output [dout_WIDTH-1:0] dout;



reg [dout_WIDTH-1:0] dout_tmp;

always @ (*) begin
case (sel)
    
    CASE0 : dout_tmp = din0;
    
    CASE1 : dout_tmp = din1;
    
    CASE2 : dout_tmp = din2;
    
    CASE3 : dout_tmp = din3;
    
    CASE4 : dout_tmp = din4;
    
    CASE5 : dout_tmp = din5;
    
    CASE6 : dout_tmp = din6;
    
    CASE7 : dout_tmp = din7;
    
    CASE8 : dout_tmp = din8;
    
    CASE9 : dout_tmp = din9;
    
    CASE10 : dout_tmp = din10;
    
    CASE11 : dout_tmp = din11;
    
    CASE12 : dout_tmp = din12;
    
    CASE13 : dout_tmp = din13;
    
    default : dout_tmp = def;
endcase
end


assign dout = dout_tmp;



endmodule
