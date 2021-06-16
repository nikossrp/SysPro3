#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "CyclicBuffer.h"

using namespace std;

CyclicBuffer::CyclicBuffer(int cyclicBufferSize)
{
    this->buffer = new string[cyclicBufferSize];
    this->start = 0;
    this->end = -1;
    this->cyclicBufferSize = cyclicBufferSize;
    this->numfiles = 0;
}

CyclicBuffer::~CyclicBuffer()
{
    delete [] buffer;
}


bool CyclicBuffer::Insert(char* fileName)
{
    string fileName_str(fileName);
    this->end = (this->end + 1) % cyclicBufferSize;
    this->buffer[this->end] = fileName_str;
    this->numfiles++;
    return true;
}

char* CyclicBuffer::get_next_file()
{
    string file;
    char* file_c;
    file = this->buffer[this->start];
    file_c = strdup(file.c_str());
    this->start = (this->start + 1) % cyclicBufferSize;
    this->numfiles--;
    return file_c;
}


bool CyclicBuffer::isEmpty()
{
    return (this->numfiles <= 0);
}


bool CyclicBuffer::isFull()
{
    return (this->numfiles >= cyclicBufferSize);
}