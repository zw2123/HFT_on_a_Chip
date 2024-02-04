/*How does this work:

  1.The system waits for packets on the reception path, handling metadata and buffering packet data for decoding.
  
  2.Upon receiving complete packet data (either in one or two parts), the system decodes the FAST message into 
  an order and handles it accordingly.
  
  3.On the transmission side, the system encodes orders into FAST protocol messages and prepares them for network 
  transmission in AXI word structures.
  
  4.The entire process is managed through state machines that ensure correct sequencing of operations and handling 
  of network protocols.*/

#include "FAST.h"
#include "decoder.h"
#include "encoder.h"

#define PORT 49152//Dynamic port

// Handles the initial processing of incoming network packets.
void handleIncomingPacket(hls::stream<axiWord>& lbRxDataIn, //incoming packet data
                          hls::stream<metadata>& lbRxMetadataIn, //incoming packet metadata
                          hls::stream<ap_uint<64> >& tagsIn, //incomg tags
                          hls::stream<ap_uint<64> >& time_to_book, //pass the time information forward
                          hls::stream<metadata>& metadata_to_book, //pass the metadata forward
                          ap_uint<64>& first_packet, //store the first part of packet
                          fast_protocol::Rx_State& next_state) {//reference to a variable hoding next state
    // Pass-through for time
    time_to_book.write(tagsIn.read());

    // Switch the source and destination port for the metadata
    metadata tempMetadata = lbRxMetadataIn.read();
    sockaddr_in tempSocket = tempMetadata.sourceSocket;
    tempMetadata.sourceSocket = tempMetadata.destinationSocket;
    tempMetadata.destinationSocket = tempSocket;
    
    // Writes the modified metadata to the metadata_to_book stream for further use.
    metadata_to_book.write(tempMetadata);

    // Buffer input packet for later use by the decoder
    axiWord tempWord = lbRxDataIn.read();
    
    // Stores the data part of the axiWord to the first_packet variable for later use by the decoder.
    first_packet = tempWord.data;

    // Determines the next state of the receiver based on whether the current packet is the last packet.
    // If the last flag is set in the tempWord, indicating this is the last packet of the current message,
    // the next state is set to PROCESS_PACKET, signaling that the packet is ready to be processed.
    // Otherwise, the state is set to READ_SECOND, indicating that more data for the current packet is expected.
    next_state = tempWord.last ? PROCESS_PACKET : READ_SECOND;
}

// This function processes a packet by first preparing the encoded message array
// from two parts of the split packet and then decoding it to extract the order information.
void processPacket(ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE], ap_uint<64>& first_packet, ap_uint<64>& second_packet, hls::stream<order>& order_to_book) {
    
    // Loop through the bytes of the first and second parts of the split packet.
    // For each byte, extract it by shifting the packet content and mask out the high bits.
    // The encoded message is reconstructed from both parts of the packet.
     for (unsigned i = 0; i < NUM_BYTES_IN_PACKET; i++) {
        // Extract each byte from the first packet and store it in the encoded message array.
        encoded_message[i] = first_packet >> (BYTE * i);
        // Do the same for the second packet, offsetting the index to store it after the first packet's data.
        encoded_message[i + NUM_BYTES_IN_PACKET] = second_packet >> (BYTE * i);
    }

    // Instantiate an 'order' structure to hold the decoded message.
    order decoded_order;
    // Decode the message from the first and second packets into 'decoded_order'.
    // Fast_Decoder::decode_message is assumed to be capable of handling the decoding logic.
    Fast_Decoder::decode_message(first_packet, second_packet, decoded_order);

    // If the 'order_to_book' stream is not full, write the decoded order to it.
    // This makes the decoded order available for further processing downstream.
    if (!order_to_book.full()) {
        order_to_book.write(decoded_order);
    }
}

void preparePackets(const ap_uint<8> encoded_message[MESSAGE_BUFF_SIZE], axiWord& first_packet, axiWord& second_packet) {
    // Assuming axiWord.data is 64 bits and encoded_message fits within two axiWord packets
    first_packet.data = 0;
    second_packet.data = 0;
    first_packet.keep = 0xFF; // Assuming all bytes are valid for simplicity
    second_packet.keep = 0xFF; 

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

void Fast_processor(hls::stream<axiWord>& lbRxDataIn,
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