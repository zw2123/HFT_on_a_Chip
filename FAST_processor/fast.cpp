/*How does this work:

  1.The system waits for packets on the reception path, handling metadata and buffering packet data for decoding.
  
  2.Upon receiving complete packet data (either in one or two parts), the system decodes the FAST message into 
  an order and handles it accordingly.
  
  3.On the transmission side, the system encodes orders into FAST protocol messages and prepares them for network 
  transmission in AXI word structures.
  
  4.The entire process is managed through state machines that ensure correct sequencing of operations and handling 
  of network protocols.*/

#include "fast.h"
#include "decoder.h"
#include "encoder.h"

#define PORT 750

#define STOP_BIT    0x80
#define VALID_DATA  0x7F
#define SIGN_BIT    0x40

#define NUMBER_OF_VALID_BITS_IN_BYTE    7

#define pow(x,y)  exp(y*log(x))

#define PRICE_FIELD_EXP_NUM     0
#define PRICE_FIELD_MAN_NUM     1
#define SIZE_FIELD_NUM          2
#define ORDER_ID_FIELD_NUM      3
#define TYPE_FIELD_NUM          4

void decode_and_process_order(const ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE], order& temp_order) {
    // Assuming definitions of these constants are available
    // NUMBER_OF_VALID_BITS_IN_BYTE, STOP_BIT, VALID_DATA

    unsigned message_offset = 2;  // Starting offset after metadata
    fix16 price_buff = 0;
    uint8 size_buff = 0;
    uint32 orderID_buff = 0;
    uint3 order_type_buff = 0;

    // Decode Price Field
    ap_int<7> decoded_exponent = (encoded_message[message_offset++] & 0x7F);
    ap_uint<21> decoded_mantissa = 0;

    // Decode Mantissa Field
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT) {
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++]);
    }

    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT) {
        decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++]);
    }

    // Extract last byte of data
    decoded_mantissa = (decoded_mantissa << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++] & VALID_DATA);
    
    // Assuming powers_of_ten is defined and accessible
    float powers_of_ten[4] = { 1, 0.1, 0.01, 0.001 };
    price_buff = decoded_mantissa * (powers_of_ten[(-1 * decoded_exponent)]);

    // Decode Size Field
    if ((encoded_message[message_offset] & STOP_BIT) != STOP_BIT) {
        size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++]);
    }
    size_buff = (size_buff << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++] & VALID_DATA);

    // Decode Order ID Field
    for (unsigned i = 0; i < 4; i++) {
        if ((encoded_message[message_offset] & STOP_BIT) == STOP_BIT) {
            break;
        }
        orderID_buff = (orderID_buff << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++]);
    }
    orderID_buff = (orderID_buff << NUMBER_OF_VALID_BITS_IN_BYTE) | (encoded_message[message_offset++] & VALID_DATA);

    // Decode Type Field
    order_type_buff = (encoded_message[message_offset] & VALID_DATA);

    // Assigning decoded values to temp_order
    temp_order.price = price_buff;
    temp_order.size = size_buff;
    temp_order.orderID = orderID_buff;
    temp_order.type = order_type_buff;
}

void rxPath(stream<axiWord>& lbRxDataIn,
            stream<metadata>& lbRxMetadataIn,
            stream<ap_uint<16>>& lbRequestPortOpenOut,
            stream<bool>& lbPortOpenReplyIn,
            stream<metadata>& metadata_to_book,
            stream<ap_uint<64>>& tagsIn,
            stream<ap_uint<64>>& time_to_book,
            stream<order>& order_to_book) {
#pragma HLS PIPELINE II=1

    static enum Rx_State {
        PORT_OPEN = 0, PORT_REPLY, READ_FIRST, READ_SECOND, WRITE, FUNC, FUNC_2
    } next_state;

    static ap_uint<32> openPortWaitTime = 100;
    // The encoded_message array is declared statically and partitioned for parallel access.
    static ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = {0};
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1

    static order temp_order = {0, 0, 0, 0};
    static ap_uint<64> first_packet = 0;
    static ap_uint<64> second_packet = 0;

    switch (next_state) {
    case PORT_OPEN:
        if (!lbRequestPortOpenOut.full() && openPortWaitTime == 0) {
            lbRequestPortOpenOut.write(PORT);
            next_state = PORT_REPLY;
        } else {
            openPortWaitTime--;
        }
        break;
    case PORT_REPLY:
        if (!lbPortOpenReplyIn.empty()) {
            lbPortOpenReplyIn.read();  // Reading to clear the stream but not storing as it's unused
            next_state = READ_FIRST;
        }
        break;
    case READ_FIRST:
        if (!lbRxDataIn.empty() && !lbRxMetadataIn.empty() &&
            !metadata_to_book.full() && !tagsIn.empty() &&
            !time_to_book.full()) {
            time_to_book.write(tagsIn.read());
            metadata tempMetadata = lbRxMetadataIn.read();
            std::swap(tempMetadata.sourceSocket, tempMetadata.destinationSocket);
            metadata_to_book.write(tempMetadata);
            axiWord tempWord = lbRxDataIn.read();
            first_packet = tempWord.data;
            next_state = tempWord.last ? FUNC : READ_SECOND;
        }
        break;
    case FUNC:
    case FUNC_2:  // FUNC and FUNC_2 can share the same decoding logic
        for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
            encoded_message[i] = first_packet >> (BYTE * i);
            encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
        }
        // The decoding process is encapsulated in a function for clarity and potential reuse.
        decode_and_process_order(encoded_message, temp_order);
        next_state = WRITE;
        break;
    case READ_SECOND:
        if (!lbRxDataIn.empty()) {
            axiWord tempWord = lbRxDataIn.read();
            second_packet = tempWord.data;
            next_state = tempWord.last ? FUNC_2 : READ_SECOND;
        }
        break;
    case WRITE:
        if (!order_to_book.full()) {
            order_to_book.write(temp_order);
            next_state = READ_FIRST;
        }
        break;
    }
}

void txPath(stream<metadata> &metadata_from_book,
            stream<axiWord> &lbTxDataOut,
            stream<metadata> &lbTxMetadataOut,
            stream<ap_uint<16>> &lbTxLengthOut,
            stream<ap_uint<64>> &time_from_book,
            stream<ap_uint<64>> &tagsOut,
            stream<order> &order_from_book) {
#pragma HLS PIPELINE II=1

    static enum Tx_State {
        READ = 0, WRITE_FIRST, WRITE_SECOND
    } next_state;

    static uint16_t lbPacketLength = 0;
    static axiWord second_packet = {0, 0xFF, 1};
    static axiWord first_packet = {0, 0xFF, 0};

    switch (next_state) {
    case READ:
        if (!metadata_from_book.empty() && !time_from_book.empty() &&
            !order_from_book.empty() && !lbTxMetadataOut.full() &&
            !lbTxLengthOut.full() && !tagsOut.full()) {

            tagsOut.write(time_from_book.read());
            lbTxMetadataOut.write(metadata_from_book.read());

            order decoded_message = order_from_book.read();
            ap_uint<64> first_packet_data;
            ap_uint<64> second_packet_data;

            //Encoding Logic Starts
            ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = { 0. };
        #pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1

            encoded_message[0] = 0xFC; // Presence map: all fields are present.
            encoded_message[1] = 0x81; // template id = 1
            encoded_message[2] = 0x80; // exponent is always 0
            encoded_message[3] = 0x81; //price is always 1

            unsigned message_offset = 4;
            if ((decoded_message.size & 0x80) == 0x80)
            {
                encoded_message[message_offset++] = 0x01;
            }
            encoded_message[message_offset++] = (0x80 | (decoded_message.size & 0x7F));

            bool triggered = false; //triggered

            unsigned max_bytes_need_for_uint32 = 5;
            for (unsigned i = 0; i < max_bytes_need_for_uint32 - 1; i++)
            {
                uint8 curr_byte = (0x00
                        | (decoded_message.orderID
                                >> NUMBER_OF_VALID_BITS_IN_BYTE
                                        * (max_bytes_need_for_uint32 - 1 - i) & 0x7F));
                if (curr_byte != 0x00 || triggered == true)
                {
                    encoded_message[message_offset++] = (0x00
                            | (decoded_message.orderID
                                    >> NUMBER_OF_VALID_BITS_IN_BYTE
                                                    * (max_bytes_need_for_uint32 - 1 - i)
                                            & 0x7F));
                    triggered = true;
                }
            }
            encoded_message[message_offset++] = (0x80 | (decoded_message.orderID & 0x7F));

            encoded_message[message_offset++] = (0x80 | decoded_message.type);

            for (int i = NUM_BYTES_IN_PACKET - 1; i >= 0; i--) {
                first_packet_data = (first_packet_data << BYTE) | encoded_message[i];
                 second_packet_data = (second_packet_data << BYTE) | encoded_message[i + NUM_BYTES_IN_PACKET];
             }

            //Encoding logic ends

            // Buffer the 128-bit encoded message in two 64-bit packets
            first_packet.data = first_packet_data;
            second_packet.data = second_packet_data;

            // Transition to the next state
            next_state = WRITE_FIRST;
        }
        break;
    case WRITE_FIRST:
        if (!lbTxDataOut.full()) {
            lbTxDataOut.write(first_packet);

            // Efficiently count the number of bits set in first_packet.keep
            lbPacketLength = __builtin_popcount(first_packet.keep.to_uint()); // Assuming HLS supports __builtin_popcount
            next_state = WRITE_SECOND;
        }
        break;
    case WRITE_SECOND:
        if (!lbTxDataOut.full()) {
            lbTxDataOut.write(second_packet);

            // Add the number of bits set in second_packet.keep to lbPacketLength
            lbPacketLength += __builtin_popcount(second_packet.keep.to_uint());
            lbTxLengthOut.write(lbPacketLength); // Write the total length

            // Reset state to READ for next operation
            next_state = READ;
        }
        break;
    }
}

void fast_protocol(stream<axiWord>& lbRxDataIn,
                   stream<metadata>& lbRxMetadataIn,
                   stream<ap_uint<16> >& lbRequestPortOpenOut,
                   stream<bool>& lbPortOpenReplyIn,
                   stream<axiWord> &lbTxDataOut,
                   stream<metadata> &lbTxMetadataOut,
                   stream<ap_uint<16> > &lbTxLengthOut,
                   stream<ap_uint<64> > &tagsIn,
                   stream<ap_uint<64> > &tagsOut,
                   stream<metadata> &metadata_to_book,
                   stream<metadata> &metadata_from_book,
                   stream<ap_uint<64> > &time_to_book,
                   stream<ap_uint<64> > &time_from_book,
                   stream<order> &order_to_book,
                   stream<order> &order_from_book)
{
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

#pragma HLS INTERFACE axis port=lbRxDataIn
#pragma HLS INTERFACE axis port=lbRxMetadataIn
#pragma HLS INTERFACE axis port=lbRequestPortOpenOut
#pragma HLS INTERFACE axis port=lbPortOpenReplyIn
#pragma HLS INTERFACE axis port=lbTxDataOut
#pragma HLS INTERFACE axis port=lbTxMetadataOut
#pragma HLS INTERFACE axis port=lbTxLengthOut
#pragma HLS INTERFACE axis port=tagsIn
#pragma HLS INTERFACE axis port=tagsOut

#pragma HLS INTERFACE axis port=metadata_to_book
#pragma HLS INTERFACE axis port=metadata_from_book
#pragma HLS INTERFACE axis port=time_to_book
#pragma HLS INTERFACE axis port=time_from_book
#pragma HLS INTERFACE axis port=order_to_book
#pragma HLS INTERFACE axis port=order_from_book

    rxPath(lbRxDataIn,
           lbRxMetadataIn,
           lbRequestPortOpenOut,
           lbPortOpenReplyIn,
           metadata_to_book,
           tagsIn,
           time_to_book,
           order_to_book);

    txPath(metadata_from_book,
           lbTxDataOut,
           lbTxMetadataOut,
           lbTxLengthOut,
           time_from_book,
           tagsOut,
           order_from_book);
}
