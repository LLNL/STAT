#if !defined(STAT_CircularLogs_)
#define STAT_CircularLogs_

#include <boost/circular_buffer.hpp>

class CircularBuffer
{
    public:
        CircularBuffer(size_t size);
        ~CircularBuffer();

        /* Return a write FILE handle for adding to the buffer */
        FILE *handle();

        /* Return the buffer in two parts. */
        bool getBuffer(char* &buffer1, size_t &buffer1Size, char* &buffer2, size_t &buffer2Size);

        /* Write the buffer to the given file descriptor */
        int flushBufferTo(int fd);

        /* Convert the buffer to a string and return it.  Adds a null character to end of buffer */
        const char *str();

        /* Clear the buffer */
        void reset();

    private:
        typedef boost::circular_buffer<char> Buffer_t;

        static ssize_t writeWrapper(void *cookie, const char *buf, size_t size);
        static int closeWrapper(void *cookie);
        ssize_t cWrite(const char *buf, size_t size);
        int cClose();
        void init();

        Buffer_t *buffer_;
        FILE *fHandle_;
        size_t bufferSize_;
};

#endif
