#ifndef QUEUE_H_
#define QUEUE_H_
#include <iostream>
//模板的实现要写在一个文件中
template <class T, int QUEUE_SIZE = 5>
class CircularQueue
{
private:
    T buffer[QUEUE_SIZE]; // 存储 MppFrame的数组
    int head;        // 读取位置
    int tail;        // 写入位置

public:
    CircularQueue() : head(0), tail(0){} 
    bool queue_push(const T in);
    bool queue_pop(T * out);
};

//入队
template <class T, int QUEUE_SIZE>
bool CircularQueue<T, QUEUE_SIZE>::queue_push(const T in)
{
    int next_tail = (tail + 1) % QUEUE_SIZE;
    if (next_tail == head)
    {
        std::cout << "full" << std::endl;
        return false;
    }
    buffer[tail] = in;
    tail = next_tail;
    return true;
}

//出队
template <class T, int QUEUE_SIZE>
bool CircularQueue<T, QUEUE_SIZE>::queue_pop(T * out)
{
    if (head == tail)
        return false;
    *out = buffer[head];
    head = (head + 1) % QUEUE_SIZE;
    return true;
}

#endif