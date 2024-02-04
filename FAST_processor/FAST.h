#pragma once

#include <hls_stream.h>
#include "ap_int.h"
#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>


namespace fast_protocol {

enum Rx_State { PORT_OPEN = 0, PORT_REPLY, READ_FIRST, READ_SECOND, PROCESS_PACKET };

const uint16_t REQUEST = 0x0100;
const uint16_t REPLY = 0x0200;
const ap_uint<32> replyTimeOut = 65536;

const ap_uint<48> MY_MAC_ADDR = 0xE59D02350A00;  // LSB first, 00:0A:35:02:9D:E5
const ap_uint<48> BROADCAST_MAC = 0xFFFFFFFFFFFF;   // Broadcast MAC Address

const uint8_t noOfArpTableEntries = 8;

struct axiWord {
    ap_uint<64> data = 0;
    ap_uint<8> keep = 0xFF;
    ap_uint<1> last = 0;
};

struct sockaddr_in {
    ap_uint<16> port = 0;   // port in network byte order
    ap_uint<32> addr = 0;   // Internet address
};

struct metadata {
    sockaddr_in sourceSocket;
    sockaddr_in destinationSocket;
};

enum class FieldNumber {
    PRICE_FIELD_EXPO = 0,
    PRICE_FIELD_MAN,
    SIZE_FIELD_NUM,
    ORDER_ID_FIELD,
    TYPE_FIELD_NUM
};

// Constants for encoding/decoding
constexpr size_t MESSAGE_BUFF_SIZE = 16;  // size in bytes
constexpr size_t NUMBER_OF_FIELDS = 6;    // including decimal mantissa and exponent as separate fields
constexpr size_t PACKET_SIZE = 64;
constexpr size_t NUM_BYTES_IN_PACKET = PACKET_SIZE / 8;
constexpr size_t BYTE = 8;

// Data types
using fix16 = ap_fixed<16, 8>;   // 16-bit fixed point with 8 fractional bits
using uint3 = ap_uint<3>;        // 3-bit unsigned integer
using uint8 = ap_uint<8>;        // 8-bit unsigned integer
using uint32 = ap_uint<32>;      // 32-bit unsigned integer
using uint64 = ap_uint<64>;      // 64-bit unsigned integer
using uint16 = ap_uint<16>;      // 16-bit unsigned integer

struct order {
    fix16 price;
    uint8 size;
    uint32 orderID;
    uint3 type;
};

void fast_protocol(hls::stream<axiWord>& lbRxDataIn,
                   hls::stream<metadata>& lbRxMetadataIn,
                   hls::stream<ap_uint<16> >& lbRequestPortOpenOut,
                   hls::stream<bool>& lbPortOpenReplyIn,
                   hls::stream<axiWord> &lbTxDataOut,
                   hls::stream<metadata> &lbTxMetadataOut,
                   hls::stream<ap_uint<16> > &lbTxLengthOut,
                   hls::stream<ap_uint<64> > &tagsIn,
                   hls::stream<ap_uint<64> > &tagsOut,
                   hls::stream<metadata> &metadata_to_book,
                   hls::stream<metadata> &metadata_from_book,
                   hls::stream<ap_uint<64> > &time_to_book,
                   hls::stream<ap_uint<64> > &time_from_book,
                   hls::stream<order> &order_to_book,
                   hls::stream<order> &order_from_book);

} // namespace fast_protocol
