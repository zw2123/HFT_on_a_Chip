#ifndef FAST_ENCODER_H
#define FAST_ENCODER_H

#include "fast.h" // Ensure this file defines 'order' and 'fix16' appropriately
#include "ap_int.h" // For ap_uint<>
#include <cstdint> // For standard integer types

// Constants for message encoding
constexpr uint8_t STOP_BIT2 = 0x80;
constexpr uint8_t VALID_DATA2 = 0x7F;
constexpr uint8_t SIGN_BIT2 = 0x40;


constexpr int BUFF_SIZE_2 = 1024; // Adjust based on your needs
constexpr int BYTES_IN_PACK = 8; // Assuming based on context

class Fast_Encoder {
public:
    // Modify to use 'ap_uint<N>' for size, orderID, and type if those are the actual types used
    static void encode_fast_message(const order& decoded_message,
                                    uint64_t& first_packet,
                                    uint64_t& second_packet);

    static void encode_decimal_from_fix16(const fix16& decoded_fix16,
                                          unsigned exponent_offset,
                                          unsigned& mantissa_offset,
                                          ap_uint<8> encoded_message[BUFF_SIZE_2]); // Assuming fix16 and encoded_message types

    static void encodeUintFromUint8(ap_uint<8> decoded_uint8, // Adjusted type
                                    unsigned& message_offset,
                                    ap_uint<8> encoded_message[BUFF_SIZE_2]); // Adjusted type

    static void encodeUintFromUint32(ap_uint<32> decoded_uint32, // Adjusted type
                                     unsigned& message_offset,
                                     ap_uint<8> encoded_message[BUFF_SIZE_2]); // Adjusted type

    static void encodeUintFromUint2(ap_uint<3> decoded_uint2, // Adjusted type
                                    unsigned& message_offset,
                                    ap_uint<8> encoded_message[BUFF_SIZE_2]); // Adjusted type


private:
    // Function prototypes adjusted to match the optimized implementation
    static void encode_decimal_from_fix16(const fix16& decoded_fix16,
                                          unsigned exponent_offset,
                                          unsigned& mantissa_offset,
                                          uint8_t encoded_message[BUFF_SIZE_2]);

    static void encode_signed_int(uint8_t encoded_message[BUFF_SIZE_2],
                                  unsigned& mantissa_offset,
                                  int64_t mantissa);

    static void encode_uint_from_uint32(uint32_t decoded_uint32,
                                        unsigned& message_offset,
                                        uint8_t encoded_message[BUFF_SIZE_2]);

    static void encode_uint_from_uint8(uint8_t decoded_uint8,
                                       unsigned& message_offset,
                                       uint8_t encoded_message[BUFF_SIZE_2]);

    static void encode_uint_from_uint2(uint8_t decoded_uint2,
                                       unsigned& message_offset,
                                       uint8_t encoded_message[BUFF_SIZE_2]);

    // Helper function for generic integer encoding - Consider including if used outside the class
    //static void encode_generic_integer(uint64_t value, unsigned& offset, uint8_t encoded_message[BUFF_SIZE_2], bool has_stop_bit = true);
};

#endif // FAST_ENCODER_H
