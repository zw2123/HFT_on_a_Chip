#ifndef FAST_ENCODER_H
#define FAST_ENCODER_H
#define MANTISSA_SIZE 35
#include "FAST.h"
#include "ap_int.h"  // For ap_int and ap_uint

// Assuming definitions for fixed-point types and MESSAGE_BUFF_SIZE are in FAST.h
using namespace fast_protocol;  // Use only if the entire file needs access to this namespace

class Fast_Encoder {
public:
    static void encode_fast_message(order & decoded_message,
                                    ap_uint<64> & first_packet,
                                    ap_uint<64> & second_packet);

private:
    // Field Encoders
    static void encode_decimal_from_fix16(ap_fixed<16,8> & decoded_fix16,
                                          unsigned exponent_offset,
                                          unsigned & mantissa_offset,
                                          ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]);

    static void encode_signed_int(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                  unsigned & mantissa_offset,
                                  ap_int<MANTISSA_SIZE> mantissa);

    static void encode_uint_from_uint32(ap_uint<32> & decoded_uint32,
                                        unsigned & message_offset,
                                        ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]);

    static void encode_uint_from_uint8(ap_uint<8> & decoded_uint8,
                                       unsigned & message_offset,
                                       ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]);

    static void encode_uint_from_uint2(ap_uint<2> & decoded_uint2,  // Correction from uint3 to uint2 for consistency
                                       unsigned & message_offset,
                                       ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]);
};

#endif // FAST_ENCODER_H
