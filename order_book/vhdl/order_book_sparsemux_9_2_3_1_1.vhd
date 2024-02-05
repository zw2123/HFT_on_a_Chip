-- ==============================================================
-- Generated by Vitis HLS v2023.2.1
-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
-- ==============================================================
-- 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity order_book_sparsemux_9_2_3_1_1 is
generic (

    din0_WIDTH : INTEGER := 1;

    din1_WIDTH : INTEGER := 1;

    din2_WIDTH : INTEGER := 1;

    din3_WIDTH : INTEGER := 1;

    def_WIDTH : INTEGER := 1;
    sel_WIDTH : INTEGER := 1;
    dout_WIDTH : INTEGER := 1;
    
    CASE0 : std_logic_vector(1 downto 0);
    
    CASE1 : std_logic_vector(1 downto 0);
    
    CASE2 : std_logic_vector(1 downto 0);
    
    CASE3 : std_logic_vector(1 downto 0);
    
    ID : INTEGER := 1;
    NUM_STAGE : INTEGER := 1
);
port (


    din0 : in std_logic_vector (din0_WIDTH-1 downto 0);

    din1 : in std_logic_vector (din1_WIDTH-1 downto 0);

    din2 : in std_logic_vector (din2_WIDTH-1 downto 0);

    din3 : in std_logic_vector (din3_WIDTH-1 downto 0);

    def   : in std_logic_vector (def_WIDTH-1 downto 0);
    sel   : in std_logic_vector (sel_WIDTH-1 downto 0);
    dout  : out std_logic_vector (dout_WIDTH-1 downto 0)
);
end entity;

architecture behav of order_book_sparsemux_9_2_3_1_1 is
    signal dout_tmp : std_logic_vector (dout_WIDTH-1 downto 0);

begin

    process(din0, din1, din2, din3, sel) is
    begin
        case sel is
            
            when CASE0 =>
                dout_tmp <= din0;
            
            when CASE1 =>
                dout_tmp <= din1;
            
            when CASE2 =>
                dout_tmp <= din2;
            
            when CASE3 =>
                dout_tmp <= din3;
            
            when others =>
                dout_tmp <= def;
        end case;
    end process;


    dout <= dout_tmp;




end architecture;