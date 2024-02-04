/*The Fast_Decoder class decodes financial trading messages encoded with the FAST protocol, 
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
#include <ap_fixed.h>


#define STOP_BIT 	0x80
#define VALID_DATA	0x7F
#define SIGN_BIT	0x40
#define FAST_VALID_BITS	7

void Fast_Decoder::decode_message(ap_uint<64> & first_packet,
                                  ap_uint<64> & second_packet,
                                  order & decoded_message)
 {
    // Prepare a buffer for the encoded message, partitioned for efficient access
    ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = {0};
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1
    // Unroll the loop to concurrently process all bytes from the input packets
    for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
    #pragma HLS UNROLL
    // Extract bytes from the first and second packets, storing them in the buffer
        encoded_message[i] = first_packet >> (BYTE * i);
        encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
    }

    // Initialize buffers for decoded fields
    fix16 price_buff = 0;
    ap_uint<8> size_buff = 0;
    ap_uint<32> orderID_buff = 0;
    ap_uint<3> order_type_buff = 0;

    // Start decoding from the third byte (offset = 2)
    unsigned message_offset = 2;

    // Extract and decode the exponent part of the price
    ap_int<7> decoded_exponent = (encoded_message[message_offset++] & VALID_DATA);

    // Decode Mantissa Field
    ap_uint<21> decoded_mantissa = 0;

    for (unsigned i = 0; i < 3; ++i) {
    #pragma HLS UNROLL
    // Check for the stop bit to determine the end of the mantissa field
        if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT) {
            decoded_mantissa = (decoded_mantissa << FAST_VALID_BITS) | 
                                (encoded_message[message_offset] & VALID_DATA);
        } else {
            // Exit the loop if stop bit is encountered
            break;
        }
        message_offset++;
    }

    // Calculate the actual price using fixed-point arithmetic based on decoded exponent and mantissa
    const fix16 powers_of_ten[4] = {1, 0.1, 0.01, 0.001};
#pragma HLS ARRAY_PARTITION variable=powers_of_ten complete dim=1

    // Multiplying mantissa with the corresponding power of ten using fixed-point arithmetic
    price_buff = decoded_mantissa * powers_of_ten[(-1 * decoded_exponent)];
    //print out mantissa result
    std::cout << decoded_mantissa << "*10^" << decoded_exponent << std::endl;


    // Decode Size Field

    // Extract first byte from the message buffer if stop bit is not encountered
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT)
    {
        size_buff = (size_buff << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    size_buff = (size_buff << FAST_VALID_BITS)
            | (encoded_message[message_offset++] & VALID_DATA);

    // Decode Order ID Field

    // Extract bytes from the message buffer until stop bit is encountered
    for (unsigned i = 0; i < 4; i++)
    {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        orderID_buff = (orderID_buff << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    orderID_buff = (orderID_buff << FAST_VALID_BITS)
            | (encoded_message[message_offset++] & VALID_DATA);

    // Decode type Field
    order_type_buff = (encoded_message[message_offset] & VALID_DATA);
    decoded_message.price = price_buff;
    decoded_message.size = size_buff;
    decoded_message.orderID = orderID_buff;
    decoded_message.type = order_type_buff;

}

// Function to decode a FAST-encoded decimal field into a fixed-point number.
void Fast_Decoder::decode_decimal_to_fix16(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                           unsigned & message_offset,
                                           fix16 & decoded_fix16)
{
    // Define a table of powers of ten to convert encoded decimals to fixed-point numbers.
    // This uses fixed-point arithmetic for precision and efficiency.
    const ap_fixed<32, 4> powers_of_ten[8] = {0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001, 0.00000001};
#pragma HLS ARRAY_PARTITION variable=powers_of_ten complete dim=1
    // Check if the exponent is negative by inspecting the SIGN_BIT
    int decoded_exponent = 0;//exponent initialization
    // Check if the exponent is negative by inspecting the SIGN_BIT.
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_exponent = -1;// Set the exponent to negative if SIGN_BIT is set.
    }

    // Decode the exponent value, ensuring we don't read beyond the message buffer.
    for (unsigned i = 0; (i < 2) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {   // Stop decoding if the STOP_BIT is encountered, indicating the end of the exponent field.
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        // Shift and mask to accumulate the exponent value.
        decoded_exponent = (decoded_exponent << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }

    long int decoded_mantissa = 0;// Initialize the mantissa.
    // Check if the mantissa should be negative.
    if ((encoded_message[message_offset] & SIGN_BIT) == SIGN_BIT)
    {
        decoded_mantissa = -1;
    }

    // Decode the mantissa value, with bounds checking.
    for (unsigned i = 0; (i < 6) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {    // Stop decoding if the STOP_BIT is found, marking the end of the mantissa field.
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {
            break;
        }
        // Accumulate the mantissa value by shifting and masking.
        decoded_mantissa = (decoded_mantissa << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }
   // Apply the decoded exponent to the mantissa. If the exponent is negative, multiply the mantissa
   // by the corresponding power of ten. Otherwise, the mantissa remains unchanged.
    if (decoded_exponent < 0)
    {
        decoded_fix16 = decoded_mantissa * powers_of_ten[(-1 * decoded_exponent) - 1];
    }
    else
    {
        decoded_fix16 = decoded_mantissa;
    }
}

// Function to decode a FAST-encoded unsigned integer field into an 8-bit unsigned integer.
void Fast_Decoder::decode_uint_to_uint8(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned & message_offset,
                                        ap_uint<8> & decoded_uint8)
{
    // Iterate through the encoded message to decode the value, with checks to ensure
    // we don't read beyond the buffer size and we respect the FAST encoding rules.
    for (unsigned i = 0; (i < 2) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {   // Check for the STOP_BIT in the current byte. The presence of the STOP_BIT
        // indicates the end of the current field being decoded.
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {// If the STOP_BIT is found, stop decoding further to prevent reading into the next field.
            break;
        }
        // Decode the current byte and accumulate it into the `decoded_uint8` result.
        // Shift the previously accumulated result left by 7 bits (since FAST uses 7 bits for data and 1 bit 
        // for the stop indicator),
        // then OR it with the current byte masked to remove the stop bit, effectively concatenating the bits.
        decoded_uint8 = (decoded_uint8 << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }
}

// Decodes a FAST-encoded message into a 32-bit unsigned integer.
void Fast_Decoder::decode_uint_to_uint32(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                         unsigned & message_offset,
                                         ap_uint<32> & decoded_uint32)
{
    // Loop through encoded message bytes up to a maximum of 5 bytes, ensuring not to exceed the message buffer size.
    for (unsigned i = 0; (i < 5) && (message_offset < MESSAGE_BUFF_SIZE); i++)
    {   // Check if the current byte contains the STOP_BIT indicating the end of this field.
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT)
        {   // If STOP_BIT is found, exit the loop as the field is fully decoded.
            break;
        }
        // Shift the accumulated decoded_uint32 value left by 7 bits to make space for new data,
        // and OR it with the current byte's valid data bits (7 LSBs) to accumulate the decoded value.
        decoded_uint32 = (decoded_uint32 << FAST_VALID_BITS)
                | (encoded_message[message_offset++]);
    }
}
// Decodes a specific field of a FAST-encoded message into a 2-bit unsigned integer.
void Fast_Decoder::decode_uint_to_uint2(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE],
                                        unsigned message_offset,
                                        ap_uint<2> & decoded_uint2)
{   // Directly extract the relevant 2 bits from the encoded_message at the given message_offset.
    // Assumes the relevant data is fully contained within the specific byte at message_offset.
    decoded_uint2 = (encoded_message[message_offset] & VALID_DATA);
}