#ifndef CYCLICBUF
#define CYCLICBUF

typedef struct CyclicNode{
    char* file;
    struct CyclicNode* next;

}CyclicNode;


class CyclicBuffer {
    private:
        std::string *buffer;
        int start;
        int end;
        int numfiles;
        int cyclicBufferSize;


    public:
        CyclicBuffer(int cyclicBufferSize);
        ~CyclicBuffer();
        bool Insert(char* fileName);
        bool isEmpty(); 
        bool isFull();
        char* get_next_file();
        int get_numFiles() const { return numfiles; }
        int get_cyclicBufferSize() const { return cyclicBufferSize; }
        void print();
};



#endif