/* How does this work?
Constants Definition: The code defines several constants for bit manipulation, crucial for decoding the FAST-encoded messages. 
These constants include STOP_BIT, VALID_DATA, SIGN_BIT, and NUMBER_OF_VALID_BITS_IN_BYTE, all of which are used to interpret 
the encoded data correctly.

decode_uint8 Function: This inline function decodes an 8-bit unsigned integer from the encoded message. It extracts a byte from 
the encoded message, applying a mask to retrieve only the relevant bits, and advances the message offset.

decode_uint32 Function: This inline function decodes a 32-bit unsigned integer. It iteratively calls decode_uint8 to construct the 
integer, checking for the stop bit to know when the value ends.

decode_decimal_to_fix16 Function: It decodes a fixed-point number from the message. The function first decodes an exponent and a 
mantissa, then combines them to form the decimal number. This function is essential for interpreting price information, which often 
requires high precision.

decode_fast_message Function: The core function that orchestrates the decoding process. It unpacks two 64-bit packets into a byte 
array, interpreting the data based on the FAST protocol. The function then sequentially decodes different parts of the message, 
including price, size, order ID, and type, storing them in a provided order structure.

Error Handling: The decode_fast_message function includes a preliminary check for invalid input packets, ensuring that the decoding 
process only proceeds with valid data.

Efficiency: The code is designed with inline functions and loop unrolling (implicit in the for-loops due to their fixed-size nature), 
aiming to optimize the FPGA's parallel processing capabilities.
 */

#include "decoder.h"
#include <cstdint> 
#include <array>   

// Constants for bit manipulation
#define STOP_BIT    0x80
#define VALID_DATA  0x7F
#define SIGN_BIT    0x40
#define NUMBER_OF_VALID_BITS_IN_BYTE    7
#define BYTE_SHIFT  8  // Define BYTE_SHIFT for clarity

inline uint8_t Fast_Decoder::decode_uint8(const uint8_t *encoded_message, unsigned &message_offset) {
    return encoded_message[message_offset++] & VALID_DATA;
}

inline uint32_t Fast_Decoder::decode_uint32(const uint8_t *encoded_message, unsigned &message_offset) {
    uint32_t value = 0;
    #pragma HLS UNROLL factor=5
    for (int i = 0; i < 5; ++i) {
        uint8_t byte = decode_uint8(encoded_message, message_offset);
        value = (value << NUMBER_OF_VALID_BITS_IN_BYTE) | byte;
        if (encoded_message[message_offset - 1] & STOP_BIT) {
            break;
        }
    }
    return value;
}

inline fix16 Fast_Decoder::decode_decimal_to_fix16(const uint8_t *encoded_message, unsigned &message_offset) {
    #pragma HLS PIPELINE
    int8_t exponent = static_cast<int8_t>(encoded_message[message_offset] & VALID_DATA);
    if (encoded_message[message_offset] & SIGN_BIT) {
        exponent = -exponent;
    }
    message_offset++;

    uint32_t mantissa = 0;
    #pragma HLS UNROLL factor=3
    for (int i = 0; i < 3; i++) {
        mantissa = (mantissa << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset] & VALID_DATA);
        if (encoded_message[message_offset++] & STOP_BIT) {
            break;
        }
    }

    // Simplify or use a lookup table for pow(10, exponent) if possible
    fix16 value = mantissa * pow(10, exponent); // Ensure pow is optimized for fixed-point
    return value;
}

void Fast_Decoder::decode_fast_message(uint64_t &first_packet, uint64_t &second_packet, order &decoded_message) {
    #pragma HLS PIPELINE
    if (first_packet == 0 || second_packet == 0) {
        // Early exit for invalid packets, consider FPGA-friendly logging if necessary
        return;
    }

    uint8_t encoded_message[MESSAGE_BUFF_SIZE] = {0};
    #pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1


    // Efficient byte unpacking
    for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
        #pragma HLS UNROLL
        encoded_message[i] = static_cast<uint8_t>(first_packet >> (BYTE_SHIFT * i));
        encoded_message[i + NUM_BYTES_IN_PACKET] = static_cast<uint8_t>(second_packet >> (BYTE_SHIFT * i));
    }

    // Direct decoding, assuming offset based on protocol specifics
    unsigned message_offset = 2;
    decoded_message.price = decode_decimal_to_fix16(encoded_message, message_offset);
    decoded_message.size = decode_uint8(encoded_message, message_offset);
    decoded_message.orderID = decode_uint32(encoded_message, message_offset);
    decoded_message.type = decode_uint8(encoded_message, message_offset);
}
