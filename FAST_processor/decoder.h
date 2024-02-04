#ifndef FAST_DECODER_H
#define FAST_DECODER_H

#include "FAST.h"  
#include "ap_int.h"

#define STRING_SIZE 10


using namespace fast_protocol;  // Use only if the entire file needs access to this namespace

class Fast_Decoder {
public:
    static void decode_message(ap_uint<64> & first_packet,
                                    ap_uint<64> & second_packet,
                                    order & decoded_message);  

private:
    
    // Field Decoders
    static void decode_decimal_to_fix16(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned & message_offset,
                                        ap_fixed<16,8> & decoded_fix16);  // Example for fix16 replacement

    static void decode_uint_to_uint32(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                      unsigned & message_offset,
                                      ap_uint<32> & decoded_uint32);

    static void decode_uint_to_uint8(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                     unsigned & message_offset,
                                     ap_uint<8> & decoded_uint8);

    static void decode_uint_to_uint2(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                     unsigned message_offset,
                                     ap_uint<2> & decoded_uint2);  
};

#endif // FAST_DECODER_H
