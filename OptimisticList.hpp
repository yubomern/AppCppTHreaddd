#ifndef OPTIMISTIC_LIST_HPP
#define OPTIMISTIC_LIST_HPP


//
// Used for hashing
#include <functional>
#include <limits>
//
// Used for printing
#include <iostream>
//
// Used for locks
#include <mutex>
#include <atomic>
//
// Smart pointers!
#include <memory>

using namespace std;

/**
 * Generic template for a Linked List.
 */
template<typename T> class OptimisticList {
    private: 
        /**
         * Inner nested node class.
         */
        class Node{
            public:
                //
                // Item being stored
                T item; 

                //
                // Hash of the item
                size_t key;

                //
                // Next node in the chain
                // Node *next;
                std::shared_ptr<Node> next;

                //
                // Lock for a node.
                std::mutex mutex;
                std::atomic<bool> isLocked{false};

                /**
                 * Regular Node constructor 
                 */
                Node(T item, size_t key) {
                    this->item = item;
                    this->key = key;
                    this->next = nullptr;
                }

                /**
                 * Constructor for sentinal nodes
                 */
                Node(size_t key){
                    this->key = key;
                    this->next = nullptr;
                }

                /**
                 * Lock the node
                 */
                void lock(){
                    mutex.lock();
                    isLocked.store(true);
                }

                /**
                 * Unlock the node if locked.
                 */
                void unlock(){
                    if (isLocked.exchange(false)) {
                        mutex.unlock();
                    }
                }
        };

        //
        // The head and tail of the singly linked list implementation of OptimisticList.
        // Node head;
        std::shared_ptr<Node> head = std::make_shared<Node>(0);

        //
        // The std hashing object, so we don't need to initate it multiple times.
        hash<T> hasher;

    public: 
        /**
         * The constructor for the OptimisticList. It initiates the head and tail.
         */
        OptimisticList() {
            std::shared_ptr<Node> tail = std::make_shared<Node>(std::numeric_limits<std::size_t>::max());
            head->next = tail;
        }

        /**
         * The destructor for the OptimisticList. It clears all dynamically allocated memory.
         * What happens if this is called while other threads are doing work?
         */
        ~OptimisticList(){
            //
            // This should be enough in theory, since head is the only saved reference.
            head.reset();
        }

        /**
         * Check that prev and curr are still in list and adjacent
         * @param pred predecessor node
         * @param curr current node
         * @return whther predecessor and current have changed
         */
        bool validate(std::shared_ptr<Node> prev, std::shared_ptr<Node> curr){
            std::shared_ptr<Node> node = head;
            while (node != nullptr && node->key <= prev->key){
                if (node->key == prev->key){
                    return prev->next == curr;
                }
                node = node->next;
            }
            return false;
        }

        /**
         * Add an element.
         * @param item element to add
         * @return true iff element was not there already
         */
        bool add(T item) {
            //
            // Get the hash of the item we are trying to insert.
            size_t key = hasher(item);

            //
            // Lock the head, and set it as prev.
            std::shared_ptr<Node> prev;
            std::shared_ptr<Node> curr;

            prev = head;

            //
            // Until validation is true (optimistically, this loop will only execute once)
            while (true){
                //
                // Find the insertion spot without locking.
                curr = prev->next;
                while (curr->key < key){
                    prev = curr;
                    curr = curr->next;
                }

                prev->lock();
                curr->lock();

                try{
                    if (validate(prev, curr)){
                        //
                        // If the item already exists in the list, return false.
                        if (key == curr->key){
                            prev->unlock();
                            curr->unlock();
                            return false;
                        }

                        //
                        // Insert the node.
                        std::shared_ptr<Node> newNode = std::make_shared<Node>(item, key);

                        newNode->next = curr;
                        prev->next = newNode;

                        prev->unlock();
                        curr->unlock();

                        return true;
                    }
                    else {
                        //
                        // If validation did not work, we start over again.
                        continue;
                    }
                }
                catch (...) {
                    cout << "Something went wrong during add(). \n";
                    prev->unlock();
                    curr->unlock();
                    prev = head;
                }
            }
        }

        /**
         * Remove an element.
         * 
         * #TODO Nodes are removed instantly, and since we don't get a lock
         * to traverse the list, it is possible to have a thread try to reference a node
         * that has been deleted. I think smart pointers are the solution... 
         * 
         * 
         * Also, C++20 has atomic smart pointers, but that is kind of cheating?
         * 
         * I'll try using std::shared_ptr instead of regular pointers for nodes. Ask Dr. Mendes about this issue.
         * 
         * @param item element to remove
         * @return true if element was present
         */
        bool remove(T item) {
            //
            // Get the hash of the item we are trying to remove.
            size_t key = hasher(item);

            //
            // Lock the head, and set it as prev.
            std::shared_ptr<Node> prev;
            std::shared_ptr<Node> curr;

            prev = head;

            //
            // Until validation is true (optimistically, this loop will only execute once)
            while (true){
                //
                // Find the remove spot without locking.
                curr = prev->next;
                while (curr->key < key){
                    prev = curr;
                    curr = curr->next;
                }

                prev->lock();
                curr->lock();

                try{
                    if (validate(prev, curr)){
                        //
                        // If the item does not exist in the list, return false.
                        if (key != curr->key){
                            prev->unlock();
                            curr->unlock();
                            return false;
                        }

                        //
                        // Remove the node, the smart pointer should dereference it in theory.
                        prev->next = curr->next;

                        curr->unlock();
                        prev->unlock();
                        return true;
                    }
                    else {
                        //
                        // If validation did not work, we start over again.
                        continue;
                    }
                }
                catch (...) {
                    prev->unlock();
                    curr->unlock();

                    cout << "Something went wrong during remove(). \n";
                    
                    prev = head;
                    return false;
                }
            }
        }

        /**
         * Test whether element is present
         * @param item element to test
         * @return true iff element is present
         */
        bool contains(T item) {
            //
            // Get the hash of the item we are trying to find.
            size_t key = hasher(item);

            //
            // Lock the head, and set it as prev.
            std::shared_ptr<Node> prev;
            std::shared_ptr<Node> curr;

            prev = head;

            //
            // Until validation is true (optimistically, this loop will only execute once)
            while (true){
                //
                // Find the insertion spot without locking.
                curr = prev->next;
                while (curr->key < key){
                    prev = curr;
                    curr = curr->next;
                }

                prev->lock();
                curr->lock();

                try{
                    if (validate(prev, curr)){
                        //
                        // If the item exists in the list, return true.
                        if (key == curr->key){
                            prev->unlock();
                            curr->unlock();
                            return true;
                        }
                        else {
                            prev->unlock();
                            curr->unlock();
                            return false;
                        }
                    }
                    else {
                        //
                        // If validation did not work, we start over again.
                        continue;
                    }
                }
                catch (...) {
                    cout << "Something went wrong during contains(). \n";
                    prev->unlock();
                    curr->unlock();
                    prev = head;
                }
            }
        }
};







#endif 