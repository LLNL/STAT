#ifndef MRNETSYMBOLREADER_H
#define MRNETSYMBOLREADER_H

#include "SymReader.h"
#include "mrnet/MRNet.h"
#include "STAT_timer.h"
#include "Comm/MRNetCommFabric.h"
#include "AsyncFastGlobalFileStat.h"
#include "MRNetSymbolReader.h"

using namespace Dyninst;
using namespace MRN;
using namespace FastGlobalFileStat;
using namespace FastGlobalFileStat::MountPointAttribute;
using namespace FastGlobalFileStat::CommLayer;

extern int CUR_OUTPUT_LEVEL; 
#define mrn_dbg(x, y) \
do { \
    if( MRN::CUR_OUTPUT_LEVEL >= x ){           \
        y;                                      \
    }                                           \
} while(0)

class Elf_X;

class MRNetSymbolReader :public Dyninst::SymReader
{
 
 public:

   std::string file_;
   const char *buffer_;
   unsigned long size_;
   SymReader *symReaderHandle_;
   int refCount_;

   MRNetSymbolReader(std::string file, 
                     const char* buffer,
                     unsigned long size, 
                     SymReader* symReaderHandle)
                      : file_(file), buffer_(buffer), size_(size),
                        symReaderHandle_(symReaderHandle) {}   
   virtual ~MRNetSymbolReader() {}
   virtual Symbol_t getSymbolByName(std::string symname);
   virtual Symbol_t getContainingSymbol(Dyninst::Offset offset);
   virtual std::string getInterpreterName();
   virtual unsigned getAddressWidth();
   virtual unsigned numRegions();
   virtual bool getRegion(unsigned num, SymRegion &reg); 
   virtual Dyninst::Offset getSymbolOffset(const Symbol_t &sym);
   virtual std::string getSymbolName(const Symbol_t &sym);
   virtual std::string getDemangledName(const Symbol_t &sym);
   virtual unsigned long getSymbolSize(const Symbol_t &sym);
   virtual bool isValidSymbol(const Symbol_t &sym);
   virtual Section_t getSectionByName(std::string name);
   virtual Section_t getSectionByAddress(Dyninst::Address addr);
   virtual Dyninst::Address getSectionAddress(Section_t sec);
   virtual std::string getSectionName(Section_t sec);
   virtual bool isValidSection(Section_t sec);
   virtual Elf_X *getElfHandle();
   virtual Dyninst::Offset imageOffset();
   virtual Dyninst::Offset dataOffset();
};

class MRNetSymbolReaderFactory : public Dyninst::SymbolReaderFactory
{

   SymbolReaderFactory *symbolReaderFactoryHandle_;
   Network *network_;
   Stream *stream_;
   std::map<std::string, MRNetSymbolReader *> openReaders_;
   std::string perfFile_;
   virtual MRNetSymbolReader *openSymReader(const char *bufferm,
                                            unsigned long size,
                                            std::string pathName="");
 public:
   
   MRNetSymbolReaderFactory(SymbolReaderFactory *symbolReaderFactoryHandle,
                            Network *network,
                            Stream *stream)
                             : symbolReaderFactoryHandle_(symbolReaderFactoryHandle),
                               network_(network), stream_(stream) {} 
   virtual ~MRNetSymbolReaderFactory() {}
   virtual SymReader *openSymbolReader(std::string pathName);
   virtual SymReader *openSymbolReader(const char *buffer, unsigned long size);
   virtual bool closeSymbolReader(SymReader *sr);
};

SymReader *MRNetSymbolReaderFactory::openSymbolReader(std::string pathName)
{
    const char *pathStr = pathName.c_str();
    bool localLib = true;
    int tag = PROT_LIB_REQ, fileContentsLength, ret, size;
    char *fileName;
    unsigned char *fileContents; 
    FILE *fp;
    PacketPtr packet;
    MRNetSymbolReader *msr;
    std::map<std::string, MRNetSymbolReader* >::iterator iter;

    mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader", stderr,
            "Interposed lib functions called openSymbolReader(%s)\n",
            pathName.c_str()));

    iter = openReaders_.find(pathName);
    if (iter == openReaders_.end())
    {  
        mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader", stderr,
                "no existing reader for %s\n", pathStr));

        AsyncGlobalFileStat myStat(pathStr);
        if (IS_YES(myStat.isUnique()))
        {
            //fprintf(stderr, "BE: %s is Unique, requesting contents from FE\n", pathStr);
            localLib = false;                
            
            mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                    stderr, "requesting contents for %s\n", pathStr));
    
            if (stream_->send(tag, "%s", pathStr) == -1)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "BE: stream::send() failure\n"));
                return NULL;
            }
            if (stream_->flush() == -1)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "BE: stream::flush() failure\n"));
                return NULL;
            }
                
            ret = network_->recv(&tag, packet, &stream_);
            if (ret != 1)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "BE: network::recv() failure\n"));
                return NULL;
            }

            if (tag == PROT_LIB_REQ_ERR)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "FE reported error sending contents of %s\n", pathStr));
                localLib = true;
            }
            else if (packet->unpack("%auc %s", &fileContents, &fileContentsLength, &fileName) == -1)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "Failed to unpack contents of %s\n", pathStr));
                localLib = true;
            }
        }
    
        if (localLib == true)
        {
            mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                    stderr, "Trying to read %s locally\n", pathStr));
            fp = fopen(pathStr,"r");
            if (fp == NULL)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "File %s does not exist on BE\n", pathStr));
                return NULL;
            }

            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            fileContents = (unsigned char *)malloc(size * sizeof(unsigned char));
            if (fileContents == NULL)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "Malloc returned NULL for %s\n", pathStr));
                return NULL;
            }
            fread(fileContents, size, 1, fp);
            fclose(fp);
    
            msr = openSymReader((char *)fileContents, size, pathName);
            if (msr == NULL)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "openSymReader returned NULL for %s %d\n", 
                        pathName.c_str(), size));
                return NULL;
            }
        }
        else
        {
            mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                    stderr, "reading contents of %s\n", pathStr));
            msr = openSymReader((char *)fileContents, fileContentsLength,
                                pathName);
            if (msr == NULL)
            {
                mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader",
                        stderr, "openSymReader returned NULL for %s %d\n", 
                        pathName.c_str(), fileContentsLength));
                return NULL;
            }
        }
        openReaders_[pathName] = msr;
        msr->refCount_ = 1;
    }
    else
    {
        mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymbolReader", stderr,
                "Found existing reader for %s\n", pathStr));
        msr = iter->second;
        msr->refCount_++;
    }

    return static_cast<SymReader *>(msr);
}

MRNetSymbolReader *
MRNetSymbolReaderFactory::openSymReader(const char *buffer,
                                        unsigned long size,
                                        std::string file)
{
    SymReader *handle = symbolReaderFactoryHandle_->openSymbolReader(buffer,
                                                                     size);    
    if (handle == NULL)
    {
        mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymReader", stderr,
                "openSymbolReader returned NULL for %s %d\n", file.c_str(),
                size));
        return NULL;
    }
    MRNetSymbolReader *msr = new MRNetSymbolReader(file, buffer, size, handle);
    return msr;
}

SymReader *
MRNetSymbolReaderFactory::openSymbolReader(const char *buffer, unsigned long size)
{
    MRNetSymbolReader *msr = openSymReader(buffer,size);
    if (msr == NULL)
    {
        mrn_dbg(5, mrn_printf(__FILE__, __LINE__, "openSymReader", stderr,
                "openSymbolReader returned NULL for %d\n", size));
        return NULL;
    }
    msr->refCount_ = 1;
    return static_cast<SymReader *>(msr);
}

bool MRNetSymbolReaderFactory::closeSymbolReader(SymReader *sr)
{
    MRNetSymbolReader *msr = static_cast<MRNetSymbolReader *>(sr);
    msr->refCount_--;
    std::map<std::string, MRNetSymbolReader *>::iterator iter = openReaders_.find(msr->file_);
    if ((iter != openReaders_.end()) && (msr->refCount_ == 0))
        openReaders_.erase(iter);    
    
    delete msr;
    return true;
}

inline Symbol_t MRNetSymbolReader::getSymbolByName(std::string symname)
{
    return symReaderHandle_->getSymbolByName(symname);
}

inline Symbol_t MRNetSymbolReader::getContainingSymbol(Dyninst::Offset offset)
{
    return symReaderHandle_->getContainingSymbol(offset);
}

inline std::string  MRNetSymbolReader::getInterpreterName()
{
    return symReaderHandle_->getInterpreterName();
}

inline unsigned MRNetSymbolReader::getAddressWidth()
{
    return symReaderHandle_->getAddressWidth();
}

inline unsigned MRNetSymbolReader::numRegions()
{
    return symReaderHandle_->numRegions();
}

inline bool MRNetSymbolReader::getRegion(unsigned num, SymRegion &reg)
{
    return symReaderHandle_->getRegion( num,reg);
}

inline Dyninst::Offset MRNetSymbolReader::getSymbolOffset(const Symbol_t &sym)
{
    return symReaderHandle_->getSymbolOffset(sym);
}

inline std::string MRNetSymbolReader::getSymbolName(const Symbol_t &sym)
{
    return symReaderHandle_->getSymbolName(sym);
}

inline std::string MRNetSymbolReader::getDemangledName(const Symbol_t &sym)
{
    return symReaderHandle_->getDemangledName(sym);
}

inline  unsigned long MRNetSymbolReader::getSymbolSize(const Symbol_t &sym)
{
    return symReaderHandle_->getSymbolSize(sym);
}

inline bool MRNetSymbolReader::isValidSymbol(const Symbol_t &sym)
{
    return symReaderHandle_->isValidSymbol(sym);
}

inline Section_t MRNetSymbolReader::getSectionByName(std::string name)
{
    return symReaderHandle_->getSectionByName(name);
}

inline Section_t MRNetSymbolReader::getSectionByAddress(Dyninst::Address addr)
{
    return symReaderHandle_->getSectionByAddress(addr);
}

inline Dyninst::Address MRNetSymbolReader::getSectionAddress(Section_t sec)
{
    return symReaderHandle_->getSectionAddress(sec);
}

inline std::string MRNetSymbolReader::getSectionName(Section_t sec)
{
    return symReaderHandle_->getSectionName(sec);
}

inline bool MRNetSymbolReader::isValidSection(Section_t sec)
{
    return symReaderHandle_->isValidSection(sec);
}

inline Elf_X *MRNetSymbolReader::getElfHandle() 
{
    return symReaderHandle_->getElfHandle();
}

inline Dyninst::Offset MRNetSymbolReader::imageOffset()
{
    return symReaderHandle_->imageOffset();
}

inline Dyninst::Offset MRNetSymbolReader::dataOffset()
{
    return symReaderHandle_->dataOffset();
}

#endif
