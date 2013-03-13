#if !defined(STAT_CircularLogs_)
#define STAT_CircularLogs_

#include <boost/circular_buffer.hpp>

class CircularBuffer
{
public:
    CircularBuffer(size_t size);
    ~CircularBuffer();

    //Return a write FILE handle for adding to the buffer
    FILE *handle();

    //Return the buffer in two parts.
    bool getBuffer(char* &buffer1, size_t &buffer1_size, char* &buffer2, size_t &buffer2_size);

    //Write the buffer to the given file descriptor
    int flushBufferTo(int fd);

    //Convert the buffer to a string and return it.  Adds a null character to end of buffer
    const char *str();

    //Clear the buffer
    void reset();

  private:
    typedef boost::circular_buffer<char> buffer_t;

    static ssize_t WriteWrapper(void *cookie, const char *buf, size_t size);
    static int CloseWrapper(void *cookie);
    ssize_t CWrite(const char *buf, size_t size);
    int CClose();

    void init();
   
    buffer_t *buffer;
    FILE *fhandle;
    size_t bufferSize;
};

#endif
