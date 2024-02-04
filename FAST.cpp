#include "FAST.h"
#include "decoder.h"
#include "encoder.h"

#define PORT 641

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

void handleIncomingPacket(hls::stream<axiWord>& lbRxDataIn, hls::stream<metadata>& lbRxMetadataIn, hls::stream<ap_uint<64> >& tagsIn, hls::stream<ap_uint<64> >& time_to_book, hls::stream<metadata>& metadata_to_book, ap_uint<64>& first_packet, fast_protocol::Rx_State& next_state) {
    // Pass-through for time
    time_to_book.write(tagsIn.read());

    // Switch the source and destination port for the metadata
    metadata tempMetadata = lbRxMetadataIn.read();
    sockaddr_in tempSocket = tempMetadata.sourceSocket;
    tempMetadata.sourceSocket = tempMetadata.destinationSocket;
    tempMetadata.destinationSocket = tempSocket;
    metadata_to_book.write(tempMetadata);

    // Buffer input packet for later use by the decoder
    axiWord tempWord = lbRxDataIn.read();
    first_packet = tempWord.data;

    // Decide next state based on whether the current packet is marked as the last packet
    next_state = tempWord.last ? PROCESS_PACKET : READ_SECOND;
}

void processPacket(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE], ap_uint<64>& first_packet, ap_uint<64>& second_packet, hls::stream<order>& order_to_book) {
    // Assuming Fast_Decoder::decode_fast_message is a static method capable of decoding the message
    // and populating an 'order' structure. Adjust accordingly if your implementation differs.

    // Prepare the encoded message array from the packets
    for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
        encoded_message[i] = first_packet >> (BYTE * i);
        encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
    }

    // Decode the message
   order decoded_order;
    Fast_Decoder::decode_fast_message(first_packet, second_packet, decoded_order);

    // Write the decoded order to the output stream
    if (!order_to_book.full()) {
        order_to_book.write(decoded_order);
    }
}

void preparePackets(const ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE], axiWord& first_packet, axiWord& second_packet) {
    // Assuming axiWord.data is 64 bits and encoded_message fits within two axiWord packets
    first_packet.data = 0;
    second_packet.data = 0;
    first_packet.keep = 0xFF; // Assuming all bytes are valid for simplicity
    second_packet.keep = 0xFF; // Adjust based on actual message size

    // Load the first part of the encoded message into first_packet.data
    for (int i = 0; i < 8; i++) { // Assuming 8 bytes per axiWord
        first_packet.data.range((i+1)*8-1, i*8) = encoded_message[i];
    }

    // Load the second part of the encoded message into second_packet.data
    for (int i = 0; i < 8; i++) { // Adjust loop range based on actual encoded message size
        second_packet.data.range((i+1)*8-1, i*8) = encoded_message[i+8];
    }

    // Set TLAST signal based on your protocol's packet framing requirements
    first_packet.last = 0; // Not the last packet
    second_packet.last = 1; // Indicate the last packet of the frame
}

void rxPath(hls::stream<axiWord>& lbRxDataIn,
            hls::stream<metadata>& lbRxMetadataIn,
            hls::stream<ap_uint<16> >& lbRequestPortOpenOut,
            hls::stream<bool>& lbPortOpenReplyIn,
            hls::stream<metadata> &metadata_to_book,
            hls::stream<ap_uint<64> > &tagsIn,
            hls::stream<ap_uint<64> > &time_to_book,
            hls::stream<order> & order_to_book)
{
#pragma HLS PIPELINE II=1

    static fast_protocol::Rx_State next_state = fast_protocol::PORT_OPEN;
    static ap_uint<32> openPortWaitTime = 100;
    static ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE] = { 0 };
#pragma HLS ARRAY_PARTITION variable=encoded_message complete dim=1

    static ap_uint<64> first_packet = 0;
    static ap_uint<64> second_packet = 0;
    
    
    // Simplifying the state machine to focus on key actions
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
            lbPortOpenReplyIn.read(); // Assuming successful port opening
            next_state = READ_FIRST;
        }
        break;
    case READ_FIRST:
        if (!lbRxDataIn.empty() && !lbRxMetadataIn.empty()) {
            handleIncomingPacket(lbRxDataIn, lbRxMetadataIn, tagsIn, time_to_book, metadata_to_book, first_packet, next_state);
        }
        break;
    case READ_SECOND:
        if (!lbRxDataIn.empty()) {
            axiWord tempWord = lbRxDataIn.read();
            second_packet = tempWord.data;
            next_state = tempWord.last ? PROCESS_PACKET : READ_FIRST; // Move to processing if last packet
        }
        break;
    case PROCESS_PACKET:
        processPacket(encoded_message, first_packet, second_packet, order_to_book);
        next_state = READ_FIRST; // Ready to read the next packet
        break;
    }
}

void encodeOrderToMessage( order& decoded_message, ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]) {
    ap_uint<64> first_packet;
    ap_uint<64> second_packet;

    // Encode the message into two 64-bit packets
    Fast_Encoder::encode_fast_message(decoded_message, first_packet, second_packet);

    // Assuming MESSAGE_BUFF_SIZE is at least big enough to hold both packets (16 bytes total)
    // Split each packet into the encoded_message array
    for (int i = 0; i < 8; i++) {
        encoded_message[i] = first_packet.range(8 * i + 7, 8 * i);
    }
    for (int i = 0; i < 8; i++) {
        encoded_message[i + 8] = second_packet.range(8 * i + 7, 8 * i);}
}
//helper functions of txPath
void encodeOrderToMessage(const order& decoded_message, ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE]);
uint16_t calculatePacketLength(const axiWord& packet);

void txPath(hls::stream<metadata>& metadata_from_book,
            hls::stream<axiWord>& lbTxDataOut,
            hls::stream<metadata>& lbTxMetadataOut,
            hls::stream<ap_uint<16>>& lbTxLengthOut,
            hls::stream<ap_uint<64>>& time_from_book,
            hls::stream<ap_uint<64>>& tagsOut,
            hls::stream<order>& order_from_book) {
#pragma HLS PIPELINE II=1

    static enum Tx_State { READ = 0, WRITE_FIRST, WRITE_SECOND } next_state;
    static axiWord first_packet, second_packet;

    switch (next_state) {
        case READ:
            if (!metadata_from_book.empty() && !time_from_book.empty() && !order_from_book.empty() &&
                !lbTxMetadataOut.full() && !lbTxLengthOut.full() && !tagsOut.full()) {
                
                // Metadata and Time pass-through
                tagsOut.write(time_from_book.read());
                lbTxMetadataOut.write(metadata_from_book.read());

                // Encode the order
                order decoded_message = order_from_book.read();
                ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE];

                // Prepare packets for transmission
                preparePackets(encoded_message, first_packet, second_packet);

                next_state = WRITE_FIRST;
            }
            break;
        case WRITE_FIRST:
            if (!lbTxDataOut.full()) {
                lbTxDataOut.write(first_packet);
                next_state = WRITE_SECOND;
            }
            break;
        case WRITE_SECOND:
            if (!lbTxDataOut.full()) {
                lbTxDataOut.write(second_packet);
                lbTxLengthOut.write(calculatePacketLength(first_packet) + calculatePacketLength(second_packet));
                next_state = READ;
            }
            break;
    }
}



uint16_t calculatePacketLength(const axiWord& packet) {
    uint16_t length = 0;
    for (int i = 0; i < 8; ++i) { // Assuming 8 bytes per axiWord
        if (packet.keep.bit(i)) ++length;
    }
    return length * 8; // Length in bits
}

void fast_protocol_ns(hls::stream<axiWord>& lbRxDataIn,
                   hls::stream<metadata>& lbRxMetadataIn,
                   hls::stream<ap_uint<16> >& lbRequestPortOpenOut,
                   hls::stream<bool>& lbPortOpenReplyIn,
                   hls::stream<axiWord> &lbTxDataOut,
                   hls::stream<metadata> &lbTxMetadataOut,
                   hls::stream<ap_uint<16> > &lbTxLengthOut,
                   hls::stream<ap_uint<64> > &tagsIn,
                   hls::stream<ap_uint<64> > &tagsOut,
                   hls::stream<metadata> &metadata_to_book,
                   hls::stream<metadata> &metadata_from_book,
                   hls::stream<ap_uint<64> > &time_to_book,
                   hls::stream<ap_uint<64> > &time_from_book,
                   hls::stream<order> &order_to_book,
                   hls::stream<order> &order_from_book)
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