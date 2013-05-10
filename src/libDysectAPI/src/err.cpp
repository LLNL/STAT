#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;

FILE* Err::errStream;
FILE* Err::outStream;
bool Err::useMRNet_ = false;

#define DYSECT_LOG
#define DYSECT_INFO
#define DYSECT_WARN
#define DYSECT_VERBOSE

void Err::init(FILE* estream, FILE* ostream, bool useMRNet) {
    errStream = estream;
    outStream = ostream;
    useMRNet_ = useMRNet;
}

DysectErrorCode Err::log(DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_LOG
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif
    
    return code;
}

bool Err::log(bool result, const std::string fmt, ...) {
#ifdef DYSECT_LOG
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif
    
    return result;
}

void Err::log(const std::string fmt, ...) {
#ifdef DYSECT_LOG
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Log);
    va_end (args);
#endif
}

DysectErrorCode Err::verbose(DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif
    
    return code;
}

bool Err::verbose(bool result, const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif
    
    return result;
}

void Err::verbose(const std::string fmt, ...) {
#ifdef DYSECT_VERBOSE
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Verbose);
    va_end (args);
#endif
}

DysectErrorCode Err::info(DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_INFO
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif
    
    return code;
}

bool Err::info(bool result, const std::string fmt, ...) {
#ifdef DYSECT_INFO
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif
    
    return result;
}


void Err::info(const std::string fmt, ...) {
#ifdef DYSECT_INFO
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Info);
    va_end (args);
#endif
}

DysectErrorCode Err::warn(DysectErrorCode code, const std::string fmt, ...) {
#ifdef DYSECT_WARN
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif
    
    return code;
}

bool Err::warn(bool result, const std::string fmt, ...) {
#ifdef DYSECT_WARN
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif
    
    return result;
}

void Err::warn(const std::string fmt, ...) {
#ifdef DYSECT_WARN
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Warn);
    va_end (args);
#endif
}

DysectErrorCode Err::fatal(DysectErrorCode code, const std::string fmt, ...) {
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);
    
    return code;
}

bool Err::fatal(bool result, const std::string fmt, ...) {
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);
    
    return result;
}

void Err::fatal(const std::string fmt, ...) {
    va_list args;
    va_start (args, fmt);
    write(fmt, args, Fatal);
    va_end (args);
}

void Err::write(const std::string fmt, va_list args, enum msgType type) {
    if(errStream == 0) {
        errStream = stderr;
    }
    
    if(outStream == 0) {
        outStream = stdout;
    }
    
    time_t currentTime = time(NULL);
    struct tm *localTime = localtime(&currentTime);
    const char *timeFormat = "%b %d %T";
    
    const int bufSize = 512;
    
    char timeString[bufSize];
    char strBuf[bufSize];
    string typeStr;
    
    switch(type) {
        case Log:
            typeStr = "Log";
        break;
        case Info:
            typeStr = "Info";
        break;
        case Verbose:
            typeStr = "Verbose";
        break;
        case Warn:
            typeStr = "**WARNING**";
        break;
        case Fatal:
            typeStr = "!!FATAL!!";
        break;
    }
    
    if (localTime == NULL) {
        snprintf(timeString, bufSize, "NULL");
    } else {
        strftime(timeString, bufSize, timeFormat, localTime);
    }
    
    char environmentStr[bufSize];
    if(environment == BackendEnvironment) {
      if(DysectAPI::DaemonHostname != 0) {
        snprintf(environmentStr, bufSize, "Backend(%s)", DysectAPI::DaemonHostname);
      } else {
        snprintf(environmentStr, bufSize, "Backend(?)");
      }
    } else if(environment == FrontendEnvironment) {
      snprintf(environmentStr, bufSize, "Frontend");
    } else {
      snprintf(environmentStr, bufSize, "Unknown environment!");
    }

    sprintf(strBuf, "<%s> DysectAPI %s: %s > %s\n", 
            timeString,
            environmentStr,
            typeStr.c_str(),
            fmt.c_str());
    
    if((type == Warn) || (type == Fatal)) {
        vfprintf(errStream, strBuf, args);
        fflush(errStream);
    }
    
    if(useMRNet_) {
        char msg[bufSize];
        
        vsnprintf(msg, bufSize, strBuf, args);
        //mrn_printf(sourceFile, sourceLine, "", statOutFp, "%s", msg);
    } else {
        if((type == Info) || (type == Log) || (type == Verbose)) {
            vfprintf(outStream, strBuf, args);
            fflush(outStream);
        }
    }
}
