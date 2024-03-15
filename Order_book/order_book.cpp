/*How does this work:
  
  1.log_base_2 and pow2 are utility functions that calculate the binary logarithm and powers of 2, respectively, using bitwise 
  operations for efficiency.find_path is a function that determines the path in the heap structure where a new order should be 
  inserted or where an order should be removed from.
  
  2. calculate_index uses bitwise operations to find the index of a child node in the heap based on the current path and level 
  in the heap. left_child and right_child functions return references to the left and right children of a given node in the heap.
  
  3.swapOrders swaps the values of two orders. This is used during the reheapification process when the heap order is restored 
  after an insertion or removal.add_bid and add_ask insert new bid and ask orders into the heap. They use find_path to determine 
  where to insert the new order and then perform swaps as necessary to maintain the heap's order.remove_bid and remove_ask remove 
  orders from the heap. If the order to be removed is at the top, it is removed directly; otherwise, it's marked for removal later.
  The heap is then restructured to fill in the gap left by the removed order.
  
  4. The functions interact with streams (stream<order>, stream<Time>, stream<metadata>) to handle incoming and outgoing data. 
  These streams are abstractions over channels that can be used for communication in hardware designs. order_book is the main function
  that processes incoming orders (order_stream), timestamps (incoming_time), and other metadata (incoming_meta). It handles new limit 
  orders (LIMIT_BID and LIMIT_ASK), and requests to remove orders (REMOVE_BID and REMOVE_ASK).*/
  
#include "order_book.hpp"

// No changes needed for these utility functions as they are already optimized
int log_base_2(unsigned index){
// Directly access a read-only memory (ROM) with precomputed log base 2 values
#pragma HLS INLINE
    return log_rom[index];
}

int pow2(int level){
// Bitwise shift to efficiently calculate powers of 2
#pragma HLS INLINE
    return 1 << level;
}

// Optimize find_path by ensuring efficient conditional checks and minimizing operations
int find_path(unsigned& counter, int& hole_counter, int hole_idx[CAPACITY], int level){
#pragma HLS INLINE
// If there are holes due to removed orders, use the next hole index
    if(hole_counter > 0){
        --hole_counter;
        return hole_idx[hole_counter + 1];
    }
// Otherwise, calculate the new insert position based on the order count
    else {
        return counter - pow2(level);
    }
}

// Simplify calculate_index by using conditional operator for compactness
unsigned calculate_index(int insert_path, int level, int idx){
#pragma HLS INLINE
// Use bitwise operation to determine the child index based on the current path and level
    return (insert_path >> level) & 1 ? (2*idx) + 1 : 2*idx;
}

// No significant changes needed for left_child and right_child functions
order& left_child(unsigned level, unsigned index, order queue[LEVELS][CAPACITY/2]){
    #pragma HLS INLINE
    return queue[level+1][index*2];
}

order& right_child(unsigned level, unsigned index, order queue[LEVELS][CAPACITY/2]){
    #pragma HLS INLINE
    return queue[level+1][(index*2) + 1];
}
// Manual swap function adapted for 'order' struct
void swapOrders(order &a, order &b) {
    // Swap each member individually
    // Swap the price, size, orderID, and direction between two orders
    // The `ap_ufixed` and `ap_uint` are datatypes provided by Xilinx for FPGA development

    //swap for price
    ap_ufixed<16, 8> tempPrice = a.price;
    a.price = b.price;
    b.price = tempPrice;
    
    //swap for size
    ap_uint<8> tempSize = a.size;
    a.size = b.size;
    b.size = tempSize;
    
    //swap for order ID
    ap_uint<32> tempOrderID = a.orderID;
    a.orderID = b.orderID;
    b.orderID = tempOrderID;

    //swap for direction of order
    ap_uint<3> tempDirection = a.direction;
    a.direction = b.direction;
    b.direction = tempDirection;
}

// Refactoring add_bid for clarity and potential optimization
void add_bid(order heap[LEVELS][CAPACITY/2],
             order &new_order,
             unsigned& heap_counter,
             int& hole_counter,
             int hole_idx[CAPACITY],
             int hole_lvl[CAPACITY],
             stream<order> &top_bid,
             stream<order> &top_ask,
             stream<Time> &outgoing_time,
             stream<metadata> &outgoing_meta,
             ap_uint<32> &top_bid_id,
             ap_uint<32> &top_ask_id,
             Time t, metadata m, order ask, bool w)
{
#pragma HLS INLINE
    heap_counter++;
    int insert_level = hole_counter > 0 ? hole_lvl[hole_counter] : log_base_2(heap_counter);
    int insert_path = find_path(heap_counter, hole_counter, hole_idx, insert_level);
    unsigned idx = 1, level = 0, new_idx = 0;

    // Write to streams only if 'w' is true, reducing unnecessary operations
    if(w){
        top_ask.write(ask);
        top_ask_id = ask.orderID;
        outgoing_time.write(t);
        outgoing_meta.write(m);
        top_bid.write(new_order.price > heap[0][0].price ? new_order : heap[0][0]);
        top_bid_id = new_order.price > heap[0][0].price ? new_order.orderID : heap[0][0].orderID;
    }

    // Optimizing the loop for bid insertion
    BID_PUSH_LOOP:
    for(int i = insert_level; i > 0; i--){
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
        order &current_order = heap[level][new_idx];
        bool should_swap = new_order.price > current_order.price || 
                           (new_order.price == current_order.price && new_order.orderID < current_order.orderID);
        if(should_swap){
            swapOrders(new_order, current_order);
        }
        new_idx = calculate_index(insert_path, i-1, new_idx);
        level++;
    }
    heap[level][new_idx] = new_order;
}

void remove_bid(order heap[LEVELS][CAPACITY/2],
                ap_uint<8>& req_size,
                unsigned& heap_counter,
                int& hole_counter,
                int hole_idx[CAPACITY],
                int hole_lvl[CAPACITY],
                order dummy_order)
{
#pragma HLS INLINE
    //If the incoming remove order is of size less than the top, modify the size and don't pop
    if (req_size < heap[0][0].size){
        heap[0][0].size -= req_size;
        req_size = 0;
    }
    //Else, the top is removed and the book is re-heapified
    else{
        req_size -= heap[0][0].size;
        heap_counter--;
        hole_counter++;
        unsigned level = 0;
        unsigned new_idx = 0;
        unsigned hole_level = 0;
        unsigned hole_index = 0;
        unsigned offset = 0;
        order left = left_child(level, new_idx, heap);
        order right = right_child(level, new_idx, heap);

        BID_POP_LOOP:
        while(level < LEVELS-1){
#pragma HLS DEPENDENCE variable=heap inter false
#pragma HLS LOOP_TRIPCOUNT max=11
#pragma HLS PIPELINE II=1
            if((left.price > right.price) || (left.price == right.price && left.orderID < right.orderID)){
                offset = 0;
                heap[level][new_idx] = left;
            }
            else{
                offset = 1;
                heap[level][new_idx] = right;
            }
            left = left_child(level+1, (new_idx<<1)+offset, heap);
            right = right_child(level+1, (new_idx<<1)+offset, heap);
            level++;
            new_idx = (new_idx<<1)+offset;
        }
        hole_lvl[hole_counter] = level;
        hole_idx[hole_counter] = new_idx;
        heap[level][new_idx] = dummy_order;
    }
}

void add_ask(order heap[LEVELS][CAPACITY/2],
             order &new_order,
             unsigned& heap_counter,
             int& hole_counter,
             int hole_idx[CAPACITY],
             int hole_lvl[CAPACITY],
             stream<order> &top_bid,
             stream<order> &top_ask,
             stream<Time> &outgoing_time,
             stream<metadata> &outgoing_meta,
             ap_uint<32> &top_bid_id,
             ap_uint<32> &top_ask_id,
             Time t, metadata m, order bid, bool w)
{
#pragma HLS INLINE
    // Simplify and directly compute the insert level and path
    heap_counter++;
    int insert_level = hole_counter > 0 ? hole_lvl[hole_counter] : log_base_2(heap_counter);
    int insert_path = find_path(heap_counter, hole_counter, hole_idx, insert_level);

    // Stream updates based on condition
    if(w){
        top_bid.write(bid);
        top_bid_id = bid.orderID;
        outgoing_time.write(t);
        outgoing_meta.write(m);

        // Decide which order to write based on price comparison
        if(new_order.price < heap[0][0].price || heap[0][0].orderID == 0){
            top_ask.write(new_order);
            top_ask_id = new_order.orderID;
        } else {
            top_ask.write(heap[0][0]);
            top_ask_id = heap[0][0].orderID;
        }
    }

    // Optimized loop for inserting a new ask order
    unsigned idx = 1, level = 0, new_idx = 0;
    ASK_PUSH_LOOP:
    for(int i = insert_level; i > 0; i--){
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1
        order &current_order = heap[level][new_idx];
        bool should_swap = new_order.price < current_order.price || 
                           current_order.orderID == 0 || 
                           (new_order.price == current_order.price && new_order.orderID < current_order.orderID);
        if(should_swap){
            swapOrders(new_order, current_order);
        }
        new_idx = calculate_index(insert_path, i-1, new_idx);
        level++;
    }
    heap[level][new_idx] = new_order;
}

void remove_ask(order heap[LEVELS][CAPACITY/2],
                ap_uint<8>& req_size,
                unsigned& heap_counter,
                int& hole_counter,
                int hole_idx[CAPACITY],
                int hole_lvl[CAPACITY],
                order dummy_order)
{
#pragma HLS INLINE
    // Directly adjust the size if the remove request is less than the top size
    if (req_size < heap[0][0].size){
        heap[0][0].size -= req_size;
        req_size = 0;
    } else {
        // Process removal logic
        req_size -= heap[0][0].size;
        heap_counter--;
        hole_counter++;

        // Optimized loop for re-heapifying the ask order heap
        unsigned level = 0, new_idx = 0, offset = 0;
        ASK_POP_LOOP:
        while(level < LEVELS-1){
#pragma HLS PIPELINE II=1
            order &left = left_child(level, new_idx, heap);
            order &right = right_child(level, new_idx, heap);
            
            bool is_left_preferred = left.price < right.price || 
                                     right.orderID == 0 || 
                                     (left.price == right.price && left.orderID < right.orderID);
            offset = is_left_preferred ? 0 : 1;
            heap[level][new_idx] = is_left_preferred ? left : right;

            // Prepare for next iteration
            level++;
            new_idx = (new_idx << 1) + offset;
        }
        // Set the last node to dummy and update hole information
        heap[level][new_idx] = dummy_order;
        hole_lvl[hole_counter] = level;
        hole_idx[hole_counter] = new_idx;
    }
}

void order_book(stream<order> &order_stream,
                stream<Time> &incoming_time,
                stream<metadata> &incoming_meta,
                stream<order> &top_bid,
                stream<order> &top_ask,
                stream<Time> &outgoing_time,
                stream<metadata> &outgoing_meta,
                ap_uint<32> &top_bid_id,
                ap_uint<32> &top_ask_id)
{
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite port=top_ask_id bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite port=top_bid_id bundle=CTRL_BUS
#pragma HLS INTERFACE axis register port=order_stream
#pragma HLS INTERFACE axis register port=incoming_time
#pragma HLS INTERFACE axis register port=incoming_meta
#pragma HLS INTERFACE axis register port=top_bid
#pragma HLS INTERFACE axis register port=top_ask
#pragma HLS INTERFACE axis register port=outgoing_time
#pragma HLS INTERFACE axis register port=outgoing_meta

    static order dummy_bid; dummy_bid.price = 0; dummy_bid.orderID = 0; dummy_bid.direction = 0; dummy_bid.size = 0;
    static order dummy_ask; dummy_ask.price = 255; dummy_ask.orderID = 0; dummy_ask.direction = 0; dummy_ask.size = 0;

    static order bid [LEVELS][CAPACITY/2];
#pragma HLS ARRAY_PARTITION variable=bid cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=bid complete dim=1
    static unsigned counter_bid = 0;
    static int hole_counter_bid = 0;
    static int hole_idx_bid [CAPACITY];
    static int hole_lvl_bid [CAPACITY];

    static order ask [LEVELS][CAPACITY/2];
#pragma HLS ARRAY_PARTITION variable=ask cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=ask complete dim=1
    static unsigned counter_ask = 0;
    static int hole_counter_ask = 0;
    static int hole_idx_ask [CAPACITY];
    static int hole_lvl_ask [CAPACITY];

    static order bid_remove [LEVELS][CAPACITY/2];
#pragma HLS ARRAY_PARTITION variable=bid_remove cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=bid_remove complete dim=1
    static unsigned counter_bid_remove = 0;
    static int hole_counter_bid_remove = 0;
    static int hole_idx_bid_remove [CAPACITY];
    static int hole_lvl_bid_remove [CAPACITY];

    static order ask_remove [LEVELS][CAPACITY/2];
#pragma HLS ARRAY_PARTITION variable=ask_remove cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=ask_remove complete dim=1
    static unsigned counter_ask_remove = 0;
    static int hole_counter_ask_remove = 0;
    static int hole_idx_ask_remove [CAPACITY];
    static int hole_lvl_ask_remove [CAPACITY];

    if(!order_stream.empty() && !incoming_time.empty() && !incoming_meta.empty() &&
        !top_bid.full() && !top_ask.full() && !outgoing_time.full() && !outgoing_meta.full()){
        order input = order_stream.read();
        Time time_buffer = incoming_time.read();
        metadata meta_buffer = incoming_meta.read();

        //INCOMING LIMITED BID
        if(input.direction == 3){
            //Add the incoming input to the bid order book
            add_bid(bid, input, counter_bid, hole_counter_bid, hole_idx_bid, hole_lvl_bid, top_bid,
                     top_ask, outgoing_time, outgoing_meta, top_bid_id, top_ask_id,
                     time_buffer, meta_buffer, ask[0][0], true);
            //If the book becomes full, discard the last entry
            if (counter_bid == CAPACITY-1)
                counter_bid--;
        }

        //INCOMING REMOVE BID
        else if (input.direction == 5){
            ap_uint<8> req_size = input.size;
            //If the incoming order ID is zero, keep removing entries until order size is fulfilled.
            //After that, make sure the top entry is not arbitrary deleted.
            if(input.orderID == 0){
                OPEN_BID_REMOVE:
                while (req_size > 0){
    #pragma HLS LOOP_TRIPCOUNT min=1 max=1
                    remove_bid(bid, req_size, counter_bid, hole_counter_bid, hole_idx_bid, hole_lvl_bid, dummy_bid);
                    ARBITRARY_BID_REMOVE:
                    while(bid[0][0].orderID == bid_remove[0][0].orderID && bid[0][0].orderID != 0){
    #pragma HLS LOOP_TRIPCOUNT min=0 max=1
                        ap_uint<8> temp = bid[0][0].size;
                        ap_uint<8> temp_remove = bid_remove[0][0].size;
                        remove_bid(bid, temp, counter_bid, hole_counter_bid, hole_idx_bid, hole_lvl_bid, dummy_bid);
                        remove_bid(bid_remove, temp_remove, counter_bid_remove, hole_counter_bid_remove, hole_idx_bid_remove, hole_lvl_bid_remove, dummy_bid);
                    }
                }
            }
            //If the incoming order ID is the top entry, remove it.
            else if(input.orderID == bid[0][0].orderID){
                req_size = bid[0][0].size;
                remove_bid(bid, req_size, counter_bid, hole_counter_bid, hole_idx_bid, hole_lvl_bid, dummy_bid);
                while(bid[0][0].orderID == bid_remove[0][0].orderID && bid[0][0].orderID != 0){
    #pragma HLS LOOP_TRIPCOUNT min=0 max=1
                    ap_uint<8> temp = bid[0][0].size;
                    ap_uint<8> temp_remove = bid_remove[0][0].size;
                    remove_bid(bid, temp, counter_bid, hole_counter_bid, hole_idx_bid, hole_lvl_bid, dummy_bid);
                    remove_bid(bid_remove, temp_remove, counter_bid_remove, hole_counter_bid_remove, hole_idx_bid_remove, hole_lvl_bid_remove, dummy_bid);
                }
            }
            //Otherwise, add the incoming order to the remove heap
            else{
                add_bid(bid_remove, input, counter_bid_remove, hole_counter_bid_remove, hole_idx_bid_remove, hole_lvl_bid_remove, top_bid,
                         top_ask, outgoing_time, outgoing_meta, top_bid_id, top_ask_id,
                         time_buffer, meta_buffer, ask[0][0], false);
            }
        }

        //INCOMING LIMITED ASK
        else if(input.direction == 2){
            //Add the incoming input to the ask order book
            add_ask(ask, input, counter_ask, hole_counter_ask, hole_idx_ask, hole_lvl_ask, top_bid,
                    top_ask, outgoing_time, outgoing_meta, top_bid_id, top_ask_id,
                    time_buffer, meta_buffer, bid[0][0], true);
            //If the book becomes full, discard the last entry
            if (counter_ask == CAPACITY-1)
                counter_ask--;
        }

        //INCOMING REMOVE ASK
        else if (input.direction == 4){
            ap_uint<8> req_size = input.size;
            //If the incoming order ID is zero, keep removing entries until order size is fulfilled.
            //After that, make sure the top entry is not arbitrary deleted.
            if(input.orderID == 0){
                OPEN_ASK_REMOVE:
                while (req_size > 0){
    #pragma HLS LOOP_TRIPCOUNT min=1 max=1
                    remove_ask(ask, req_size, counter_ask, hole_counter_ask, hole_idx_ask, hole_lvl_ask, dummy_ask);
                    ARBITRARY_ASK_REMOVE:
                    while(ask[0][0].orderID == ask_remove[0][0].orderID && ask[0][0].orderID != 0){
    #pragma HLS LOOP_TRIPCOUNT min=0 max=1
                        ap_uint<8> temp = ask[0][0].size;
                        ap_uint<8> temp_remove = ask_remove[0][0].size;
                        remove_ask(ask, temp, counter_ask, hole_counter_ask, hole_idx_ask, hole_lvl_ask, dummy_ask);
                        remove_ask(ask_remove, temp_remove, counter_ask_remove, hole_counter_ask_remove, hole_idx_ask_remove, hole_lvl_ask_remove, dummy_ask);
                    }
                }
            }
            //If the incoming order ID is the top entry, remove it.
            else if(input.orderID == ask[0][0].orderID){
                req_size = ask[0][0].size;
                remove_ask(ask, req_size, counter_ask, hole_counter_ask, hole_idx_ask, hole_lvl_ask, dummy_ask);
                while(ask[0][0].orderID == ask_remove[0][0].orderID && ask[0][0].orderID != 0){
    #pragma HLS LOOP_TRIPCOUNT min=0 max=1
                    ap_uint<8> temp = ask[0][0].size;
                    ap_uint<8> temp_remove = ask_remove[0][0].size;
                    remove_ask(ask, temp, counter_ask, hole_counter_ask, hole_idx_ask, hole_lvl_ask, dummy_ask);
                    remove_ask(ask_remove, temp_remove, counter_ask_remove, hole_counter_ask_remove, hole_idx_ask_remove, hole_lvl_ask_remove, dummy_ask);
                }
            }
            //Otherwise, add the incoming order to the remove heap
            else{
                add_ask(ask_remove, input, counter_ask_remove, hole_counter_ask_remove, hole_idx_ask_remove,
                         hole_lvl_ask_remove, top_bid,
                         top_ask, outgoing_time, outgoing_meta, top_bid_id, top_ask_id,
                         time_buffer, meta_buffer, ask[0][0], false);
            }
        }

        if(input.direction != 2 && input.direction != 3){
            top_bid.write(bid[0][0]);
            top_bid_id = bid[0][0].orderID;
            top_ask.write(ask[0][0]);
            top_ask_id = ask[0][0].orderID;
            outgoing_meta.write(meta_buffer);
            outgoing_time.write(time_buffer);
        }
    }
}

