#ifndef __ERR_H
#define __ERR_H

namespace DysectAPI {
  class Err {
    enum msgType {
      Log,
      Info,
      Verbose,
      Warn,
      Fatal
    };

    static FILE* errStream;
    static FILE* outStream;
    static bool useStatOutFp_;

    static void write(const std::string fmt, va_list ap, enum msgType type);
    //TODO: it may be beneficial to have source + line info for log, warn, and fatal messages
//    static void write(const char *filename, int line, const std::string fmt, va_list ap, enum msgType type);

  public:
    static void init(FILE* estream, FILE* ostream, bool useMRNet = false);
    
    static void verbose(const std::string fmt, ...);
    static DysectErrorCode verbose(DysectErrorCode code, const std::string fmt, ...);
    static bool verbose(bool result, const std::string fmt, ...);

    static void log(const std::string fmt, ...);
    static DysectErrorCode log(DysectErrorCode code, const std::string fmt, ...);
    static bool log(bool result, const std::string fmt, ...);

    static void info(const std::string fmt, ...);
    static DysectErrorCode info(DysectErrorCode code, const std::string fmt, ...);
    static bool info(bool result, const std::string fmt, ...);

    static void warn(const std::string fmt, ...);
    static DysectErrorCode warn(DysectErrorCode code, const std::string fmt, ...);
    static bool warn(bool result, const std::string fmt, ...);

    static void fatal(const std::string fmt, ...);
    static DysectErrorCode fatal(DysectErrorCode code, const std::string fmt, ...);
    static bool fatal(bool result, const std::string fmt, ...);
    };
  }

#endif
