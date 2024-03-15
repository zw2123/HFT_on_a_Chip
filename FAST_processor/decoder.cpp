/* How does this work?
 The Fast_Decoder class decodes financial trading messages encoded with the FAST protocol, 
 transforming compressed binary data into a structured format understandable by trading systems.
 
 1. Prepares an array to hold the decoded segments of the incoming message, split across two 
 64-bit chunks (first_packet and second_packet).
 
 2.Extracts bytes from the 64-bit inputs and places them into an array for easier access during 
 the decoding process. This step involves separating the encoded message into its constituent bytes
 and reassembling them into a format that can be processed to extract meaningful financial data.
 
 3. Field decoding: Decodes the price from the message, using a combination of exponent and mantissa 
 extraction, followed by a fixed-point multiplication to reconstruct the original price value. Extracts
 the order size (quantity), handling the FAST protocol's encoding scheme to combine multiple bytes into
 the final size value. Similar to size decoding, it extracts the unique identifier for an order from
 the encoded message.Determines the type of order (e.g., buy or sell) from the encoded message.
 
 4.Includes specialized functions for decoding numerical values from the encoded message into various 
 data types (fix16, ap_uint<8>, ap_uint<32>, and ap_uint<2>). These functions deal with the nuances of 
 FAST encoding, such as handling stop bits and sign bits, to accurately reconstruct the original values.*/
#include "decoder.h"

#define STOP_BIT 	0x80
#define VALID_DATA	0x7F
#define SIGN_BIT	0x40

#define NUMBER_OF_VALID_BITS_IN_BYTE	7

void Fast_Decoder::decode_fast_message(uint64 & first_packet,
                                       uint64 & second_packet,
                                       order & decoded_message)
{
    // Prepare a buffer for the encoded message, partitioned for efficient access
    uint8 encoded_message[MESSAGE_BUFF_SIZE] = { 0. };
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1
    // Unroll the loop to concurrently process all bytes from the input packets
    for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++)
    {
        encoded_message[i] = first_packet >> (BYTE * i);
        encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
    }

    // Initialize buffers for decoded fields
    fix16 price_buff = 0;
    uint8 size_buff = 0;
    uint32 orderID_buff = 0;
    uint3 order_type_buff = 0;
    
    // Start decoding from the third byte (offset = 2)
    unsigned message_offset = 2;

    // Decode Exponent
    ap_int<7> decoded_exponent = (encoded_message[message_offset++] & 0x7F);

    // Decode Mantissa Field
    ap_uint<21> decoded_mantissa = 0;

    // Extract first byte from the message buffer if stop bit is not encountered
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT)
    {
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT)
    {
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    float powers_of_ten[4] = { 1, 0.1, 0.01, 0.001};
    price_buff = decoded_mantissa * (powers_of_ten[(-1 * decoded_exponent)]);

    //std::cout << decoded_mantissa << "*10^" << decoded_exponent << std::endl;

    /*
     * Decode Size Field
     */

    // Extract first byte from the message buffer if stop bit is not encountered
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT)
    {
        size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    /*
     * Decode Order ID Field
     */

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

    /*
     * Decode type Field
     */

    order_type_buff = (encoded_message[message_offset] & VALID_DATA);

    decoded_message.price = price_buff;
    decoded_message.size = size_buff;
    decoded_message.orderID = orderID_buff;
    decoded_message.type = order_type_buff;

}

/*
 * decimal numbers are represented by two signed integers, an exponent and mantissa.
 * The numerical value of a decimal data type is obtained by extracting the
 * encoded signed integer exponent followed by the signed integer mantissa values
 * from the encoded message and multiplying the mantissa by base 10 power of the exponent.
 */
void Fast_Decoder::decode_decimal_to_fix16(uint8 encoded_message[MESSAGE_BUFF_SIZE],
                                           unsigned & message_offset,
                                           fix16 & decoded_fix16)
{
    float powers_of_ten[8] = { 0.1,
                               0.01,
                               0.001,
                               0.0001,
                               0.00001,
                               0.000001,
                               0.0000001,
                               0.00000001 };
    /*
     * Decode Exponent Field
     */
    int decoded_exponent = 0;

    // check if field is negative
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_exponent = -1;
    }

    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 2; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_exponent = (decoded_exponent << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_exponent = (decoded_exponent << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    /*
     * Decode Mantissa Field
     */
    long int decoded_mantissa = 0;

    // check if field is negative
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_mantissa = -1;
    }

    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 6; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

    //cout << decoded_mantissa << "*10^" << decoded_exponent << endl;

    if (decoded_exponent < 0)
    {
        decoded_fix16 = decoded_mantissa
                * (powers_of_ten[(-1 * decoded_exponent) - 1]);
    }
    else
    {
        decoded_fix16 = decoded_mantissa;
    }

}

/*
 * Integers are represented as stop bit encoded entities.
 * The stop bit decoding process of integer fields is:
 * Determine the length by the stop bit algorithm.
 * Remove stop bit from each byte.
 * Combine 7-bit words (without stop bits) to determine actual integer.
 */
void Fast_Decoder::decode_uint_to_uint8(uint8 encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned & message_offset,
                                        uint8 & decoded_uint8)
{
    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 2; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_uint8 = (decoded_uint8 << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_uint8 = (decoded_uint8 << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);
}

void Fast_Decoder::decode_uint_to_uint32(uint8 encoded_message[MESSAGE_BUFF_SIZE],
                                         unsigned & message_offset,
                                         uint32 & decoded_uint32)
{

    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 5; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        decoded_uint32 = (decoded_uint32 << NUMBER_OF_VALID_BITS_IN_BYTE)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_uint32 = (decoded_uint32 << NUMBER_OF_VALID_BITS_IN_BYTE)
            | (encoded_message[message_offset++] & VALID_DATA);

}

void Fast_Decoder::decode_uint_to_uint2(uint8 encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned message_offset,
                                        uint3 & decoded_uint2)
{
    decoded_uint2 = (encoded_message[message_offset] & VALID_DATA);
}

