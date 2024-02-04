#include "decoder.h"
#include <ap_fixed.h>


#define STOP_BIT 	0x80
#define VALID_DATA	0x7F
#define SIGN_BIT	0x40
#define NUMBER_OF_VALID_BITS_IN_BYTE	7

void Fast_Decoder::decode_fast_message(ap_uint<64> & first_packet,
                                       ap_uint<64> & second_packet,
                                       order & decoded_message)
 {
    
    ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = {0};
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1

    // Loop unrolling

    for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
        #pragma HLS UNROLL
        encoded_message[i] = first_packet >> (BYTE * i);
        encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
    }

    fix16 price_buff = 0;
    ap_uint<8> size_buff = 0;
    ap_uint<32> orderID_buff = 0;
    ap_uint<3> order_type_buff = 0;

    unsigned message_offset = 2;

    /*Decode Price Field*/

    // Decode Exponent
    ap_int<7> decoded_exponent = (encoded_message[message_offset++] & VALID_DATA);

    // Decode Mantissa Field
    ap_uint<21> decoded_mantissa = 0;

    // Unrolling to facilitate parallel processing of encoded_message
    for (unsigned i = 0; i < 3; ++i) {
        #pragma HLS UNROLL
        if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT) {
            decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE) | 
                                (encoded_message[message_offset] & VALID_DATA);
        } else {
            // Stop bit encountered; prepare to exit the loop
            break;
        }
        message_offset++;
    }
// Array partitioning for the lookup table to enable parallel access
    const fix16 powers_of_ten[4] = {1, 0.1, 0.01, 0.001};
#pragma HLS ARRAY_PARTITION variable=powers_of_ten complete dim=1

    // Multiplying mantissa with the corresponding power of ten using fixed-point arithmetic
    price_buff = decoded_mantissa * powers_of_ten[(-1 * decoded_exponent)];
    //std::cout << decoded_mantissa << "*10^" << decoded_exponent << std::endl;

    /* Decode Size Field*/

    // Extract first byte from the message buffer if stop bit is not encountered
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT)
    {
        size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    /*Decode Order ID Field*/

    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 4; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        orderID_buff = (orderID_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    orderID_buff = (orderID_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    /*Decode type Field*/

    order_type_buff = (encoded_message[message_offset] & VALID_DATA);
    decoded_message.price = price_buff;
    decoded_message.size = size_buff;
    decoded_message.orderID = orderID_buff;
    decoded_message.type = order_type_buff;

}

void Fast_Decoder::decode_decimal_to_fix16(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                           unsigned & message_offset,
                                           fix16 & decoded_fix16)
{
    // Replace float with fixed-point type
    const ap_fixed<32, 4> powers_of_ten[8] = {0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001};
#pragma HLS ARRAY_PARTITION variable=powers_of_ten complete dim=1

    int decoded_exponent = 0;
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_exponent = -1;
    }

    // Bounds check added
    for (unsigned i = 0; (i < 2) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_exponent = (decoded_exponent << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    long int decoded_mantissa = 0;
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_mantissa = -1;
    }

    // Bounds check added
    for (unsigned i = 0; (i < 6) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    if (decoded_exponent < 0)
    {
        decoded_fix16 = decoded_mantissa * powers_of_ten[(-1 * decoded_exponent) - 1];
    }
    else
    {
        decoded_fix16 = decoded_mantissa;
    }
}

void Fast_Decoder::decode_uint_to_uint8(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned & message_offset,
                                        ap_uint<8> & decoded_uint8)
{
    // Bounds check added
    for (unsigned i = 0; (i < 2) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_uint8 = (decoded_uint8 << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }
}

void Fast_Decoder::decode_uint_to_uint32(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                         unsigned & message_offset,
                                         ap_uint<32> & decoded_uint32)
{
    // Bounds check added
    for (unsigned i = 0; (i < 5) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_uint32 = (decoded_uint32 << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }
}

void Fast_Decoder::decode_uint_to_uint2(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned message_offset,
                                        ap_uint<2> & decoded_uint2)
{
    decoded_uint2 = (encoded_message[message_offset] & VALID_DATA);
}