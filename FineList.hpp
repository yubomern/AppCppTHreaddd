#ifndef FINE_LIST_HPP
#define FINE_LIST_HPP


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

using namespace std;

/**
 * Generic template for a Linked List.
 */
template<typename T> class FineList {
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
                Node *next;

                //
                // Lock for a node.
                std::mutex mutex;
                // #TODO use recirsive mutex instead
                // https://en.cppreference.com/w/cpp/thread/recursive_mutex
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
                 * #TODO use recirsive mutex instead
                 * https://en.cppreference.com/w/cpp/thread/recursive_mutex
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
        // The head and tail of the singly linked list implementation of FineList.
        Node head;
        Node tail;

        //
        // The std hashing object, so we don't need to initate it multiple times.
        hash<T> hasher;

    public: 
        /**
         * The constructor for the FineList. It initiates the head and tail.
         */
        FineList() : head(std::numeric_limits<std::size_t>::min()), tail(std::numeric_limits<std::size_t>::max()){
            head.next = &tail;
        }

        /**
         * The destructor for the FineList. It clears all dynamically allocated memory.
         * What happens if this is called while other threads are doing work?
         */
        ~FineList(){
            Node* curr;
            Node* temp;
            
            try{
                curr = head.next;
                while (curr != &tail){
                    curr->lock();
                    temp = curr;
                    curr = curr->next;
                    delete temp;
                }
            }
            catch (...) {
                cout << "Something went wrong during destruction of CoarseList object...\n";
            }
        }

        /**
         * Add an element.
         * @param item element to add
         * @return true iff element was not there already
         */
        bool add(T item) {
            //
            // Get the hash of the item we are trying to insert.
            size_t key = hasher(item) + 1;
            //
            // Lock the head, and set it as prev.
            Node* prev;
            Node* curr;

            head.lock();
            prev = &head;

            try {
                //
                // Find the spot we need to add this item to.
                curr = prev->next;
                curr->lock();

                while (curr->key < key){
                    prev->unlock();
                    prev = curr;
                    curr = curr->next;
                    curr->lock();
                }

                //
                // If the item already exists in the list, return false.
                if (key == curr->key){
                    prev->unlock();
                    curr->unlock();
                    return false;
                }
                
                //
                // Insert the node.
                Node* newNode = new Node(item, key);

                newNode->next = curr;
                prev->next = newNode;

                prev->unlock();
                curr->unlock();
                return true;
                
            } catch (...) {
                prev->unlock();
                curr->unlock();
                
                cout << "Something went wrong during add(). \n";
                
                return false;
            }
        }

        /**
         * Remove an element.
         * @param item element to remove
         * @return true if element was present
         */
        bool remove(T item) {
            //
            // Get the hash of the item we are trying to insert.
            size_t key = hasher(item) + 1;

            //
            // Lock the head, and set it as prev.
            Node* prev;
            Node* curr;

            head.lock();
            prev = &head;

            try {
                //
                // Find the spot we need to remove the item from
                curr = prev->next;
                curr->lock();

                while (curr->key < key){
                    prev->unlock();
                    prev = curr;
                    curr = curr->next;
                    curr->lock();
                }

                //
                // If the item does not exist in the list, return false.
                if (key != curr->key){
                    prev->unlock();
                    curr->unlock();
                    return false;
                }
                
                //
                // Remove the node
                prev->next = curr->next;

                delete curr;
                //
                // What happens if the a thread crashes right here???
                curr = nullptr;

                prev->unlock();
                return true;
                
            } catch (...) {
                prev->unlock();
                if (curr != nullptr) curr->unlock();

                cout << "Something went wrong during remove(). \n";
                return false;
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
            size_t key = hasher(item) + 1;

            //
            // Lock the head, and set it as prev.
            Node* prev;
            Node* curr;

            head.lock();
            prev = &head;

            try {
                //
                // Find the spot we need to add this item to.
                curr = prev->next;
                curr->lock();

                while (curr->key < key){
                    prev->unlock();
                    prev = curr;
                    curr = curr->next;
                    curr->lock();
                }

                //
                // If the item already exists in the list, return false.
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
                
            } catch (...) {
                prev->unlock();
                curr->unlock();
                
                cout << "Something went wrong during contain(). \n";
                
                return false;
            }
        }
};




#endif 