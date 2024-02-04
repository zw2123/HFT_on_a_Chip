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

using mantissa_t = ap_int<32>;

#define STOP_BIT     0x80
#define VALID_DATA   0x7F
#define SIGN_BIT     0x40
#define FAST_VALID_BITS 7

void Fast_Encoder::encode_fast_message(order & decoded_message,
                                       uint64 & first_packet,
                                       uint64 & second_packet)
{
    // Initialize an array to store the encoded message bytes, with HLS optimizations for parallel access
    ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = { 0 };
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1
    
    // Setting up the FAST protocol headers including presence map and template ID
    encoded_message[0] = 0xFC; // Indicates all optional fields are present in the message.
    encoded_message[1] = 0x81; // Specifies the template ID used for the message
    encoded_message[2] = 0x80; // Exponent for price encoding, set to 0 indicating no scaling
    encoded_message[3] = 0x81; // Mantissa for price, hardcoded to 1 (example purpose)

    // Start encoding fields from the fourth byte
    unsigned message_offset = 4;

    // Encoding the size field, handling cases where the most significant bit is set
    if ((decoded_message.size & 0x80) == 0x80)
    {   // If MSB is set, encode an additional byte
        encoded_message[message_offset++] = 0x01;
    }
    // Encode the size field with the stop bit set to indicate the end of this field
    encoded_message[message_offset++] = (0x80 | (decoded_message.size & 0x7F));

    // Prepare for encoding the order ID field, using a flag to track when encoding starts
    bool triggered = false;
    // Compute the maximum bytes needed for encoding a uint32, considering 7 valid bits per byte
    unsigned max_bytes_need_for_uint32 = 5;

    // Encode each byte of the order ID, excluding leading zeros until a non-zero byte is encountered
    for (unsigned i = 0; i < max_bytes_need_for_uint32 - 1; i++)
    {
        #pragma HLS UNROLL factor=4
        uint8 curr_byte = (decoded_message.orderID >> (FAST_VALID_BITS * (max_bytes_need_for_uint32 - 1 - i))) & 0x7F;
        if (curr_byte != 0x00 || triggered)
        {   // Once a non-zero byte is encountered, set the triggered flag and start encoding bytes
            encoded_message[message_offset++] = curr_byte;
            triggered = true;
        }
    }
    // Encode the last byte of the order ID with the stop bit set
    encoded_message[message_offset++] = (0x80 | (decoded_message.orderID & 0x7F));

    // Encode the type field with the stop bit set
    encoded_message[message_offset++] = (0x80 | decoded_message.type);

    // Assemble the 64-bit packets from the encoded message bytes for transmission
#pragma HLS PIPELINE
    for (int i = NUM_BYTES_IN_PACKET - 1; i >= 0; i--)
    {   // Assemble the first packet from the first half of the encoded message
        first_packet = (first_packet << BYTE) | encoded_message[i];
        // Assemble the second packet from the second half
        second_packet = (second_packet << BYTE) | encoded_message[i + NUM_BYTES_IN_PACKET];
    }
}

void Fast_Encoder::encode_decimal_from_fix16(fix16 & decoded_fix16,
                                             unsigned exponent_offset,
                                             unsigned & mantissa_offset,
                                             uint8 encoded_message[MESSAGE_BUFF_SIZE])
{
    // Encode the exponent for the fixed-point number, setting it to a predefined value (-8) to represent decimal scaling
    encoded_message[exponent_offset] = 0xF8; // exponent is always -8, Fixed exponent value to adjust decimal places


    // Multiply the decoded fixed-point number by a scale factor to convert it into an integer mantissa
   const ap_fixed<16, 8> scaleFactor = 100000000; // Scale factor to convert the fixed-point number
   mantissa_t mantissa = static_cast<mantissa_t>(decoded_fix16 * scaleFactor);
   // Encode the scaled mantissa as a signed integer
   encode_signed_int(encoded_message, mantissa_offset, mantissa);
}

const unsigned MANTISSA_NUM_ENCODING_BYTES = 4;
// Function to encode the mantissa of a decimal number into the FAST protocol format
void Fast_Encoder::encode_signed_int(uint8 encoded_message[MESSAGE_BUFF_SIZE],
                                     unsigned & mantissa_offset,
                                     ap_int<MANTISSA_SIZE> mantissa)
{
    // Loop to encode each byte of the mantissa, leaving the most significant bit (MSB) for the stop bit
    for (unsigned i = 0; i < MANTISSA_NUM_ENCODING_BYTES - 1; i++)
    {
        #pragma HLS UNROLL
        // Shift and mask the mantissa to encode 7 bits at a time
        encoded_message[mantissa_offset++] = (mantissa >> (FAST_VALID_BITS * (MANTISSA_NUM_ENCODING_BYTES - 1 - i)) & 0x7F);
    }
    // Encode the last byte with the stop bit set to indicate the end of the mantissa field
    encoded_message[mantissa_offset++] = (0x80 | (mantissa & 0x7F));
}

// Function to encode a 32-bit unsigned integer into the FAST protocol format
void Fast_Encoder::encode_uint_from_uint32(uint32 & decoded_uint32,
                                           unsigned & message_offset,
                                           uint8 encoded_message[MESSAGE_BUFF_SIZE])
{
    bool triggered = false;// Flag to start encoding once a non-zero byte is found
    unsigned max_bytes_need_for_uint32 = 5;// Maximum bytes needed to encode a 32-bit number

    // Loop to encode the 32-bit unsigned integer, excluding leading zeros
    for (unsigned i = 0; i < max_bytes_need_for_uint32 - 1; i++)
    {
        #pragma HLS UNROLL
        // Shift and mask the integer to encode 7 bits at a time
        uint8 curr_byte = (decoded_uint32 >> (FAST_VALID_BITS * (max_bytes_need_for_uint32 - 1 - i))) & 0x7F;
        if (curr_byte != 0x00 || triggered)
        {
            // Once a non-zero byte is encountered, set the triggered flag and encode the byte
            encoded_message[message_offset++] = curr_byte;
            triggered = true;
        }
    }
    encoded_message[message_offset++] = (0x80 | (decoded_uint32 & 0x7F));
}

// Function to encode an 8-bit unsigned integer into the FAST protocol format
void Fast_Encoder::encode_uint_from_uint8(uint8 & decoded_uint8,
                                          unsigned & message_offset,
                                          uint8 encoded_message[MESSAGE_BUFF_SIZE])
{
    // Check if the most significant bit (MSB) is set, which requires an additional encoding byte
    if ((decoded_uint8 & 0x80) == 0x80)
    {
        encoded_message[message_offset++] = 0x01;
    }
    // Encode the 8-bit integer with the stop bit set
    encoded_message[message_offset++] = (0x80 | (decoded_uint8 & 0x7F));
}

// Function to encode a 2-bit unsigned integer into the FAST protocol format
void Fast_Encoder::encode_uint_from_uint2(ap_uint<2> & decoded_uint2,
                                          unsigned & message_offset,
                                          uint8 encoded_message[MESSAGE_BUFF_SIZE])
{
    // Encode the 2-bit integer with the stop bit set to indicate the end of this field
    encoded_message[message_offset++] = (0x80 | decoded_uint2);
}
