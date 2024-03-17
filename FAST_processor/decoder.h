#ifndef FAST_DECODER_H
#define FAST_DECODER_H

#include "fast.h"

// Constants for bit manipulation
#define BYTE_SHIFT 8  
#define STOP_BIT 0x80
#define VALID_DATA 0x7F
#define SIGN_BIT 0x40
#define NUMBER_OF_VALID_BITS_IN_BYTE 7

class Fast_Decoder {
public:
    // Decodes the FAST message
    static void decode_fast_message(uint64_t & first_packet,
                                    uint64_t & second_packet,
                                    order & decoded_message);

private:
    // Decodes a uint8_t value from the encoded message
    static inline uint8_t decode_uint8(const uint8_t *encoded_message, unsigned &message_offset);
    
    // Decodes a uint32_t value from the encoded message
    static inline uint32_t decode_uint32(const uint8_t *encoded_message, unsigned &message_offset);
    
    // Decodes a fix16 decimal value from the encoded message
    static inline fix16 decode_decimal_to_fix16(const uint8_t *encoded_message, unsigned &message_offset);
    
    // Decodes a 2-bit unsigned integer value from the encoded message
    static void decode_uint_to_uint2(const uint8_t encoded_message[MESSAGE_BUFF_SIZE],
                                     unsigned message_offset,
                                     uint8_t & decoded_uint2); // Adjusted for uint8_t
};

#endif  // FAST_DECODER_H
