/*How does this work:

  1.For each field of the order struct (such as price, size, orderID, type), the relevant encoding function is 
  called to convert the field into a series of bytes according to FAST encoding rules. This involves handling 
  sign bits, stop bits, and ensuring that each byte only contains 7 bits of actual data, with the MSB used as 
  a control bit.
  
  2.The encoding process is designed to be efficient, with specific HLS directives like to optimize the storage 
  and access pattern of the encoded message array and to unroll loops for parallel processing.
  
  3.The encoded data is carefully assembled into larger packets for transmission, adhering to the requirements of 
  the FAST protocol and the needs of high-frequency trading systems.*/
#include "encoder.h"
#include <ap_fixed.h>
#include <cstdint> 


#define pow(x,y)  exp(y*log(x))

#define PRICE_FIELD_EXP_NUM		0
#define PRICE_FIELD_MAN_NUM		1
#define SIZE_FIELD_NUM			2
#define ORDER_ID_FIELD_NUM		3
#define TYPE_FIELD_NUM			4

void encode_generic_integer(uint64_t value, unsigned& offset, ap_uint<8>* encoded_message, bool has_stop_bit = true) {
    bool is_non_zero_found = false;
    #pragma HLS INLINE // Suggest inlining to reduce function call overhead
    for (int byte_index = 4; byte_index >= 0; --byte_index) {
        #pragma HLS UNROLL factor=2 // Partial unrolling to balance between speed and resource usage
        ap_uint<8> byte = (value >> (byte_index * 7)) & VALID_DATA2;
        if (byte != 0 || is_non_zero_found || byte_index == 0) {
            if (byte_index == 0 && has_stop_bit) {
                byte |= STOP_BIT2;
            }
            encoded_message[offset++] = byte;
            is_non_zero_found = true;
        }
    }
}

// Function to encode an 8-bit unsigned integer into the encoded_message array.
void Fast_Encoder::encodeUintFromUint8(ap_uint<8> decoded_uint8, unsigned& message_offset, ap_uint<8>* encoded_message) {
    // Directly use the utility function assuming ap_uint<8> can be implicitly cast to uint64_t
    #pragma HLS INLINE
    encode_generic_integer(decoded_uint8.to_uint(), message_offset, encoded_message);
}

// Function to encode a 32-bit unsigned integer into the encoded_message array.
void Fast_Encoder::encodeUintFromUint32(ap_uint<32> decoded_uint32, unsigned& message_offset, ap_uint<8>* encoded_message) {
    // Convert ap_uint<32> to uint64_t for processing
    #pragma HLS INLINE
    encode_generic_integer(decoded_uint32.to_uint64(), message_offset, encoded_message);
}

// Function to encode a 2-bit unsigned integer (stored in an ap_uint<3> for convenience) into the encoded_message array.
void Fast_Encoder::encodeUintFromUint2(ap_uint<3> decoded_uint2, unsigned& message_offset, ap_uint<8>* encoded_message) {
    // Convert ap_uint<3> to uint64_t for processing. Only the lower 2 bits are valid for encoding.
    #pragma HLS INLINE
    encode_generic_integer(decoded_uint2.to_uint64() & 0x03, message_offset, encoded_message);
}

void Fast_Encoder::encode_fast_message(const order& decoded_message,
                                       uint64_t& first_packet,
                                       uint64_t& second_packet) {
    #pragma HLS DATAFLOW // Enable concurrent execution where data dependencies allow
    ap_uint<8> encoded_message[BUFF_SIZE_2] = {0};
    #pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1

    // Initializing presence map and template ID
    encoded_message[0] = 0xFC; // Presence map: all fields are present
    encoded_message[1] = 0x81; // template id = 1
    encoded_message[2] = 0x80; // exponent is always 0
    encoded_message[3] = 0x81; // price is always 1

    unsigned message_offset = 4;
    Fast_Encoder::encodeUintFromUint8(decoded_message.size, message_offset, encoded_message);
    Fast_Encoder::encodeUintFromUint32(decoded_message.orderID, message_offset, encoded_message);
    Fast_Encoder::encodeUintFromUint2(decoded_message.type, message_offset, encoded_message);

    // Packing the encoded message into first_packet and second_packet
    first_packet = 0;
    second_packet = 0;
    for (int i = BYTES_IN_PACK - 1; i >= 0; --i) {
        #pragma HLS UNROLL // Fully unroll this loop for maximum throughput
        first_packet = (first_packet << 8) | encoded_message[i];
        second_packet = (second_packet << 8) | encoded_message[i + BYTES_IN_PACK];
    }
}

// Encoding a decimal value from a fixed-point number
void Fast_Encoder::encode_decimal_from_fix16(const fix16& decoded_fix16,
                                             unsigned exponent_offset,
                                             unsigned& mantissa_offset,
                                             uint8_t encoded_message[BUFF_SIZE_2]) {
    // Encode exponent
    encoded_message[exponent_offset] = 0xF8; // exponent is always -8

    // Encode mantissa
    int64_t mantissa = static_cast<int64_t>(static_cast<float>(decoded_fix16) * std::pow(10, 8));
    encode_signed_int(encoded_message, mantissa_offset, mantissa);
}

// Generic encoding functions for integers with variable length encoding
void encode_generic_integer(uint64_t value, unsigned& offset, uint8_t encoded_message[BUFF_SIZE_2], bool has_stop_bit = true) {
    bool is_non_zero_found = false;
    for (int byte_index = 4; byte_index >= 0; --byte_index) {
        uint8_t byte = (value >> (byte_index * 7)) & VALID_DATA2;
        if (byte != 0 || is_non_zero_found || byte_index == 0) {
            if (byte_index == 0 && has_stop_bit) {
                byte |= STOP_BIT2;
            }
            encoded_message[offset++] = byte;
            is_non_zero_found = true;
        }
    }
}

// Simplifying the encoding functions using the generic integer encoding
void Fast_Encoder::encode_signed_int(uint8_t encoded_message[BUFF_SIZE_2],
                                     unsigned& mantissa_offset,
                                     int64_t mantissa) {
    encode_generic_integer(mantissa, mantissa_offset, encoded_message);
}

void Fast_Encoder::encode_uint_from_uint32(uint32_t decoded_uint32,
                                           unsigned& message_offset,
                                           uint8_t encoded_message[BUFF_SIZE_2]) {
    encode_generic_integer(decoded_uint32, message_offset, encoded_message);
}

void Fast_Encoder::encode_uint_from_uint8(uint8_t decoded_uint8,
                                          unsigned& message_offset,
                                          uint8_t encoded_message[BUFF_SIZE_2]) {
    if (decoded_uint8 & SIGN_BIT2) {
        encoded_message[message_offset++] = 0x01; // Indicate that the high bit was set
    }
    encoded_message[message_offset++] = STOP_BIT2 | (decoded_uint8 & VALID_DATA2);
}

void Fast_Encoder::encode_uint_from_uint2(uint8_t decoded_uint2,
                                          unsigned& message_offset,
                                          uint8_t encoded_message[BUFF_SIZE_2]) {
    encoded_message[message_offset++] = STOP_BIT2 | (decoded_uint2 & 0x03); // Only 2 bits are valid, thus masking with 0x03
}