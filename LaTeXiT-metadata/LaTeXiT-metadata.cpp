#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <podofo/podofo.h>
#include <zlib.h>

template<typename T>
static inline void bigToHost(T* begin, T* end) {std::reverse(begin, end);}
template<typename T>
static inline T bigToHost(const T& x)
{
  T result = x;
  bigToHost(reinterpret_cast<unsigned char*>(&result), reinterpret_cast<unsigned char*>(&result)+sizeof(result));
  return result;
}
//end bigToHost()
   
template<typename T>
static inline std::ostream& operator<<(std::ostream& stream, const std::vector<T>& v)
{
  for(const auto& it : v)
    printf("%x:", (int)it);
  return stream;
}
//end operator<<(std::ostream&, const std::vector<T>&)

std::vector<unsigned char> extractPDF(const char* filename)
{
  std::vector<unsigned char> result;

  std::vector<unsigned char> data;
  std::FILE* fp = !filename ? 0 : std::fopen(filename, "rb");
  if (fp)
  {
    std::fseek(fp, 0, SEEK_END);
    const size_t size = ftell(fp);
    data.resize(size);
    std::fseek(fp, 0, SEEK_SET);
    if (!data.empty())
      std::fread(data.data(), sizeof(unsigned char), size, fp);
    std::fclose(fp);
  }//end if (fp)
  
  const std::string beginTokenString = "%PDF";
  const std::string endTokenString = "%EOF\n";
  const std::vector<unsigned char> beginToken(beginTokenString.cbegin(), beginTokenString.cend());
  const std::vector<unsigned char> endToken(endTokenString.cbegin(), endTokenString.cend());
  const auto it1 = std::search(data.cbegin(), data.cend(), beginToken.cbegin(), beginToken.cend());
  const auto it2 = std::search(it1, data.cend(), endToken.cbegin(), endToken.cend());
  if (it2 != data.cend())
    result = std::vector<unsigned char>(it1, it2+endToken.size());
  return result;
}
//end extractPDF()

static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static inline bool is_base64(unsigned char c) {return (isalnum(c) || (c == '+') || (c == '/'));}

std::vector<unsigned char> base64_decode(const std::string& encoded_string)
{
  std::vector<unsigned char> result;
  result.reserve(encoded_string.size());

  size_t in_len = encoded_string.size();
  size_t i = 0;
  size_t in_ = 0;
  unsigned char char_array_4[4] = {0};
  unsigned char char_array_3[3] = {0};
  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
  {
    char_array_4[i++] = encoded_string[in_++];
    if (i == 4)
    {
      for(i = 0; i<4; ++i)
        char_array_4[i] = base64_chars.find(char_array_4[i]);
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      for (i = 0; (i < 3); i++)
        result.push_back(char_array_3[i]);
      i = 0;
    }//end if (i == 4)
  }//end while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))

  if (i != 0)
  {
    for (size_t j = i ; j<4 ; ++j)
      char_array_4[j] = 0;
    for (size_t j = 0 ; j<4 ; ++j)
      char_array_4[j] = base64_chars.find(char_array_4[j]);
    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
    for (size_t j = 0; (j+1 < i); ++j)
      result.push_back(char_array_3[j]);
  }//end if (i != 0)

  return result;
}
//end base64_decode()

std::vector<unsigned char> zipuncompress(const std::vector<unsigned char>& data)
{
  std::vector<unsigned char> result;
  if (data.size() >= 4)
  {
    unsigned int bigDestLen = 0;
    memcpy(&bigDestLen, data.data(), 4);
    unsigned int destLen = bigToHost(bigDestLen);
    uLongf destLenf = destLen;
    result.resize(destLen);
    int error = uncompress(result.data(), &destLenf, data.data()+4, data.size()-4);
    switch(error)
    {
      case Z_OK:
        break;
      case Z_DATA_ERROR:
        break;
      default:
        break;
    }//end switch(error)
  }//end if (data.size() >= 4)
  return result;
}
//end zipuncompress:

class LaTeXiTMetaDataExtractor
{
  public:
    LaTeXiTMetaDataExtractor(void) {}
    ~LaTeXiTMetaDataExtractor() {}
  public:
    void process(const PoDoFo::PdfMemDocument& document);
    void process(const PoDoFo::PdfPage& document);
    std::string extractLaTeXiTString(const std::string& s) const;
  public:
    const std::vector<std::string>& getLatexitStrings(void) const {return this->latexitStrings;}
  protected:
    void resetCurvePoints(void) {
      this->reals.resize(0);
      this->points.resize(0);
    }//end resetCurvePoints()
    void checkMetadataFromCurvePointBytes(void);
  protected:
    std::vector<std::string> latexitStrings;
    std::vector<double> reals;
    std::vector<std::pair<double, double> > points;
};
//end LaTeXiTMetaDataExtractor()

std::string LaTeXiTMetaDataExtractor::extractLaTeXiTString(const std::string& s) const
{
  std::string result;
  const std::string beginToken = "<latexit";
  const std::string endToken = "</latexit>";
  const auto it1 = std::search(s.cbegin(), s.cend(), beginToken.cbegin(), beginToken.cend());
  const auto it2 = std::search(it1, s.cend(), endToken.cbegin(), endToken.cend());
  if (it2 != s.cend())
    result = std::string(it1, it2+endToken.size());
  return result;
}
//end LaTeXiTMetaDataExtractor::extractLaTeXiTString()

void LaTeXiTMetaDataExtractor::checkMetadataFromCurvePointBytes(void)
{
  std::stringstream candidateString;
  constexpr const double epsilon = 1e-6;
  for(const auto& it : this->points)
  {
    bool isIntegerX = (std::abs(it.first-std::floor(it.first)) <= epsilon);
    bool isValidIntegerX = isIntegerX && (it.first >= 0) && (it.first <= 255);
    bool isIntegerY = (std::abs(it.second-std::floor(it.second)) <= epsilon);
    bool isValidIntegerY = isIntegerY && (it.second >= 0) && (it.second <= 255);
    if (isValidIntegerX && isValidIntegerY)
    {
      candidateString << static_cast<char>(static_cast<unsigned char>(it.first));
      candidateString << static_cast<char>(static_cast<unsigned char>(it.second));
    }//end if (isValidIntegerX && isValidIntegerY)
  }//end for each point
  std::string s = this->extractLaTeXiTString(candidateString.str());
  if (!s.empty())
    this->latexitStrings.emplace_back(s);
}
//end LaTeXiTMetaDataExtractor::checkMetadataFromCurvePointBytes()

void LaTeXiTMetaDataExtractor::process(const PoDoFo::PdfMemDocument& document)
{
  const auto pagesCount = document.GetPageCount();
  for(auto pageIndex=0; pageIndex<pagesCount; ++pageIndex)
  {
    const PoDoFo::PdfPage* pPage = document.GetPage(pageIndex);
    if (pPage)
      this->process(*pPage);
  }//end for each page
}
//end LaTeXiTMetaDataExtractor::process(const PoDoFo::PdfMemDocument&)

void LaTeXiTMetaDataExtractor::process(const PoDoFo::PdfPage& page)
{
  PoDoFo::PdfContentsTokenizer tokenizer(const_cast<PoDoFo::PdfPage*>(&page));
  PoDoFo::EPdfContentsType eType = PoDoFo::ePdfContentsType_Keyword;
  const char* pszToken = 0;
  PoDoFo::PdfVariant var;
  while(tokenizer.ReadNext(eType, pszToken, var))
  {
    if (eType == PoDoFo::ePdfContentsType_Variant)
    {
      if (var.IsString())
      {
        std::string s = this->extractLaTeXiTString(var.GetString().GetStringUtf8());
        if (!s.empty())
          this->latexitStrings.emplace_back(s);
      }//end if (var.IsString())
      else if (var.IsNumber())
        this->reals.push_back(1.*var.GetNumber());
      else if (var.IsReal())
        this->reals.push_back(var.GetReal());
    }//end if (eType == PoDoFo::ePdfContentsType_Variant)
    else if (eType == PoDoFo::ePdfContentsType_Keyword)
    {
      if (!strcmp(pszToken, "c"))
      {
        if (this->reals.size() >= 6)
        {
          const double real6 = this->reals.back();
          this->reals.pop_back();
          const double real5 = this->reals.back();
          this->reals.pop_back();
          const double real4 = this->reals.back();
          this->reals.pop_back();
          const double real3 = this->reals.back();
          this->reals.pop_back();
          const double real2 = this->reals.back();
          this->reals.pop_back();
          const double real1 = this->reals.back();
          this->reals.pop_back();
          this->points.push_back(std::make_pair(real1, real2));
          this->points.push_back(std::make_pair(real3, real4));
          this->points.push_back(std::make_pair(real5, real6));
        }//end if (this->reals.size() >= 6)
      }//end if (!strcmp(pszToken, "c"))
      else if (!strcmp(pszToken, "l"))
      {
        if (this->reals.size() >= 2)
        {
          const double real2 = this->reals.back();
          this->reals.pop_back();
          const double real1 = this->reals.back();
          this->reals.pop_back();
          this->points.push_back(std::make_pair(real1, real2));
        }//end if (this->reals.size() >= 2)
      }//end if (!strcmp(pszToken, "l"))
      else if (!strcmp(pszToken, "v") || !strcmp(pszToken, "y"))
      {
        if (this->reals.size() >= 4)
        {
          const double real4 = this->reals.back();
          this->reals.pop_back();
          const double real3 = this->reals.back();
          this->reals.pop_back();
          const double real2 = this->reals.back();
          this->reals.pop_back();
          const double real1 = this->reals.back();
          this->reals.pop_back();
          this->points.push_back(std::make_pair(real1, real2));
          this->points.push_back(std::make_pair(real3, real4));
        }//end if (this->reals.size() >= 4)
      }//end if (!strcmp(pszToken, "v") || !strcmp(pszToken, "y"))
      else if (!strcmp(pszToken, "h"))
      {
        this->checkMetadataFromCurvePointBytes();
        this->resetCurvePoints();
      }//end if (!strcmp(pszToken, "h"))
      else if (!strcmp(pszToken, "m"))
        this->resetCurvePoints();
    }//end if (eType == PoDoFo::ePdfContentsType_Variant)
  }//end while(tokenizer.ReadNext(eType, pszToken, var))
}
//end LaTeXiTMetaDataExtractor::process(const PoDoFo::PdfPage&)

std::string ExtractLaTeXitBase64Data(const std::string& s)
{
  std::string result;
  const std::string beginToken = "\">";
  const std::string endToken = "</latexit>";
  const auto it1 = std::search(s.cbegin(), s.cend(), beginToken.cbegin(), beginToken.cend());
  const auto it2 = std::search(it1, s.cend(), endToken.cbegin(), endToken.cend());
  if (it2 != s.cend())
    result = std::string(it1+beginToken.size(), it2);
  return result;
}
//end ExtractLaTeXitBase64Data()

class BinaryPlistReader
{
  public:
    typedef enum {
      kCFBinaryPlistMarkerNull = 0x00,
      kCFBinaryPlistMarkerFalse = 0x08,
      kCFBinaryPlistMarkerTrue = 0x09,
      kCFBinaryPlistMarkerFill = 0x0F,
      kCFBinaryPlistMarkerInt = 0x10,
      kCFBinaryPlistMarkerReal = 0x20,
      kCFBinaryPlistMarkerDate = 0x33,
      kCFBinaryPlistMarkerData = 0x40,
      kCFBinaryPlistMarkerASCIIString = 0x50,
      kCFBinaryPlistMarkerUnicode16String = 0x60,
      kCFBinaryPlistMarkerUID = 0x80,
      kCFBinaryPlistMarkerArray = 0xA0,
      kCFBinaryPlistMarkerSet = 0xC0,
      kCFBinaryPlistMarkerDict = 0xD0
    } CFBinaryPlistMarker_t;

    typedef struct {
        uint8_t	_magic[6];
        uint8_t	_version[2];
    } CFBinaryPlistHeader;

    typedef struct {
        uint8_t	_unused[5];
        uint8_t     _sortVersion;
        uint8_t	_offsetIntSize;
        uint8_t	_objectRefSize;
        uint64_t	_numObjects;
        uint64_t	_topObject;
        uint64_t	_offsetTableOffset;
    } CFBinaryPlistTrailer;
    typedef uint64_t CFIndex;
    typedef struct {
      int64_t high;
      uint64_t low;
    } CFSInt128Struct;
    typedef float CFSwappedFloat32;
    typedef double CFSwappedFloat64;
    typedef wchar_t UniChar;
  public:
    class IPlist;
    class PlistComparer
    {
      public:
        bool operator()(const std::shared_ptr<IPlist>& p1, const std::shared_ptr<IPlist>& p2) const {return (*this)(p1.get(), p2.get());}
        bool operator()(const std::shared_ptr<IPlist>& p1, const IPlist* p2) const {return (*this)(p1.get(), p2);}
        bool operator()(const IPlist* p1, const std::shared_ptr<IPlist>& p2) const {return (*this)(p1, p2.get());}
        bool operator()(const IPlist* p1, const IPlist* p2) const;
    };
    
    class IPlist
    {
      public:
        IPlist(void) {}
        virtual ~IPlist() {}
      public:
        virtual bool isPrimitive(void) const {return true;}
        virtual std::string to_string(void) const = 0;
        virtual bool equals(const std::shared_ptr<IPlist>& other) const {return this->equals(other.get());}
        virtual bool equals(const IPlist* other) const = 0;
        virtual bool less(const std::shared_ptr<IPlist>& other) const {return this->less(other.get());}
        virtual bool less(const IPlist* other) const = 0;
    };
    
    class PlistNull : public IPlist
    {
      public:
        PlistNull(void) {}
        virtual ~PlistNull() {}
      public:
        virtual std::string to_string(void) const {return "null";}
        virtual bool equals(const IPlist* other) const {
          const PlistNull* _other = dynamic_cast<const PlistNull*>(other);
          return (_other != 0);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistNull* _other = dynamic_cast<const PlistNull*>(other);
          return !_other;
        }//end less()
    };
    class PlistBool : public IPlist
    {
      public:
        PlistBool(const bool& aValue):value(aValue) {}
        virtual ~PlistBool() {}
      public:
        virtual std::string to_string(void) const {return this->value ? "true" : "false";}
        virtual bool equals(const IPlist* other) const {
          const PlistBool* _other = dynamic_cast<const PlistBool*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistBool* _other = dynamic_cast<const PlistBool*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        bool value;
    };

    class PlistInteger : public IPlist
    {
      public:
        PlistInteger(const int64_t& aValue):value(aValue) {}
        virtual ~PlistInteger() {}
      public:
        virtual std::string to_string(void) const {return std::to_string(this->value);}
        virtual bool equals(const IPlist* other) const {
          const PlistInteger* _other = dynamic_cast<const PlistInteger*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistInteger* _other = dynamic_cast<const PlistInteger*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        int64_t value;
    };

    class PlistBigInteger : public IPlist
    {
      public:
        PlistBigInteger(const CFSInt128Struct& aValue):value(aValue) {}
        virtual ~PlistBigInteger() {}
      public:
        virtual std::string to_string(void) const {
          std::stringstream ss;
          ss << '{' << std::to_string(this->value.high) << ';' << std::to_string(this->value.low) << '}';
          return ss.str();
        }
        virtual bool equals(const IPlist* other) const {
          const PlistBigInteger* _other = dynamic_cast<const PlistBigInteger*>(other);
          return _other && !memcmp(&_other->value, &this->value, sizeof(this->value));
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistBigInteger* _other = dynamic_cast<const PlistBigInteger*>(other);
          return _other && ((this->value.high == _other->value.high) ? (this->value.low < _other->value.low) : (this->value.high < _other->value.high));
        }//end less()
      public:
        CFSInt128Struct value;
    };
    
    class PlistUID : public IPlist
    {
      public:
        PlistUID(const uint32_t& aValue):value(aValue) {}
        virtual ~PlistUID() {}
      public:
        virtual std::string to_string(void) const {return std::to_string(this->value);}
        virtual bool equals(const IPlist* other) const {
          const PlistUID* _other = dynamic_cast<const PlistUID*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistUID* _other = dynamic_cast<const PlistUID*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        uint32_t value;
    };

    class PlistReal : public IPlist
    {
      public:
        PlistReal(const double& aValue):value(aValue) {}
        virtual ~PlistReal() {}
      public:
        virtual std::string to_string(void) const {return std::to_string(this->value);}
        virtual bool equals(const IPlist* other) const {
          const PlistReal* _other = dynamic_cast<const PlistReal*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistReal* _other = dynamic_cast<const PlistReal*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        double value;
    };

    class PlistDate : public IPlist
    {
      public:
        PlistDate(const double& aValue):value(aValue) {}
        virtual ~PlistDate() {}
      public:
        virtual std::string to_string(void) const {return std::to_string(this->value);}
        virtual bool equals(const IPlist* other) {
          const PlistDate* _other = dynamic_cast<const PlistDate*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistDate* _other = dynamic_cast<const PlistDate*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        double value;
    };

    class PlistString : public IPlist
    {
      public:
        PlistString(const std::string& aValue):value(aValue) {}
        virtual ~PlistString() {}
      public:
        virtual std::string to_string(void) const {return this->value;}
        virtual bool equals(const IPlist* other) const {
          const PlistString* _other = dynamic_cast<const PlistString*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistString* _other = dynamic_cast<const PlistString*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        std::string value;
    };

    class PlistWString : public IPlist
    {
      public:
        PlistWString(const std::basic_string<wchar_t>& aValue):value(aValue) {}
        virtual ~PlistWString() {}
      public:
        virtual std::string to_string(void) const {
          std::stringstream ss;
          ss << this->value.c_str();
          return ss.str();
        }
        virtual bool equals(const IPlist* other) const {
          const PlistWString* _other = dynamic_cast<const PlistWString*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistWString* _other = dynamic_cast<const PlistWString*>(other);
          return _other && (this->value < _other->value);
        }//end less()
      public:
        std::basic_string<wchar_t> value;
    };

    class PlistData : public IPlist
    {
      public:
        PlistData(const std::vector<unsigned char>& aValue):value(aValue) {}
        virtual ~PlistData() {}
      public:
        virtual std::string to_string(void) const {
          std::stringstream ss;
          ss << this->value;
          return ss.str();
        }
        virtual bool equals(const IPlist* other) const {
          const PlistData* _other = dynamic_cast<const PlistData*>(other);
          return _other && (_other->value == this->value);
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistData* _other = dynamic_cast<const PlistData*>(other);
          return _other && std::lexicographical_compare(this->value.cbegin(), this->value.cend(), _other->value.cbegin(), _other->value.cend());
        }//end less()
      public:
        std::vector<unsigned char> value;
    };

    class PlistArray : public IPlist
    {
      public:
        PlistArray(void) {}
        virtual ~PlistArray() {}
      public:
        virtual bool isPrimitive(void) const {return false;}
        virtual std::string to_string(void) const {
          std::stringstream ss;
          ss << '[';
          for(size_t i = 0 ; i<value.size() ; ++i)
          {
            const IPlist* subValue = value[i].get();
             ss << (!i ? "" : ",") << (!subValue ? "<null>" : subValue->to_string());
          }
          ss << ']';
          return ss.str();
        }
        virtual bool equals(const IPlist* other) const {
          bool result = false;
          const PlistArray* _other = dynamic_cast<const PlistArray*>(other);
          result = _other && (_other->value.size() == this->value.size());
          for(size_t i = 0 ; result && i<_other->value.size() ; ++i)
            result &= _other->value[i].get() && _other->value[i]->equals(this->value[i]);
          return result;
        }//end equals()
        virtual bool less(const IPlist* other) const {
          const PlistArray* _other = dynamic_cast<const PlistArray*>(other);
          return _other && std::lexicographical_compare(this->value.cbegin(), this->value.cend(), _other->value.cbegin(), _other->value.cend(), PlistComparer());
        }//end less()
      public:
        std::vector<std::shared_ptr<IPlist> > value;
    };

    class PlistDict : public IPlist
    {
      public:
        PlistDict(void) {}
        virtual ~PlistDict() {}
      public:
        virtual bool isPrimitive(void) const {return false;}
        virtual std::string to_string(void) const {
          std::stringstream ss;
          ss << '{';
          size_t index = 0;
          for(auto& it : value)
          {
            const auto& key = it.first.get();
            const auto& subValue = it.second.get();
            ss << (!index ? "" : ",") << (!key ? "<null>" : key->to_string()) << ':' << (!subValue ? "<null>" : subValue->to_string());
            ++index;
          }
          ss << '}';
          return ss.str();
        }
        virtual bool equals(const IPlist* other) const {
          bool result = false;
          const PlistDict* _other = dynamic_cast<const PlistDict*>(other);
          result = _other && (_other->value.size() == this->value.size());
          auto itOther = _other->value.cbegin();
          auto itSelf  = this->value.cbegin();
          for( ; result && (itOther != _other->value.cend()) && (itSelf != this->value.cend()) ; ++itOther, ++itSelf)
            result &= itOther->first.get() && itOther->first->equals(itSelf->first) &&
                      itOther->second.get() && itOther->second->equals(itSelf->second);
          return result;
        }//end equals()
        virtual bool less(const IPlist* other) const {
          bool result = false;
          const PlistDict* _other = dynamic_cast<const PlistDict*>(other);
          if (_other != 0)
          {
            if (_other->value.size() == this->value.size())
              result = std::lexicographical_compare(this->value.cbegin(), this->value.cend(), _other->value.cbegin(), _other->value.cend());
            else
              result = (_other->value.size() < this->value.size());
          }//end if (_other != 0)
          return result;
        }//end less()
        virtual std::shared_ptr<IPlist> getObjectForKey(const std::shared_ptr<IPlist>& key) const {return this->getObjectForKey(key.get());}
        virtual std::shared_ptr<IPlist> getObjectForKey(const IPlist& key) const {return this->getObjectForKey(&key);}
        virtual std::shared_ptr<IPlist> getObjectForKey(const IPlist* key) const;
      public:
        std::map<std::shared_ptr<IPlist>, std::shared_ptr<IPlist>, PlistComparer> value;
    };
    
  public:
    static inline int64_t CFSwapInt16BigToHost(int16_t x) {return bigToHost(x);}
    static inline int64_t CFSwapInt32BigToHost(int32_t x) {return bigToHost(x);}
    static inline int64_t CFSwapInt64BigToHost(int64_t x) {return bigToHost(x);}
    static inline float CFConvertFloat32SwappedToHost(CFSwappedFloat32 x) {return bigToHost(x);}
    static inline double CFConvertFloat64SwappedToHost(CFSwappedFloat64 x) {return bigToHost(x);}
    typedef enum {
      CF_NO_ERROR = 0,
      CF_OVERFLOW_ERROR = (1 << 0),
    } CFError_t;

    static inline uint32_t __check_uint32_add_unsigned_unsigned(uint32_t x, uint32_t y, int32_t* err) {
        if((UINT_MAX - y) < x)
        *err = *err | CF_OVERFLOW_ERROR;
        return x + y;
    }
    static inline uint32_t __check_uint32_mul_unsigned_unsigned(uint32_t x, uint32_t y, int32_t* err) {
      uint64_t tmp = (uint64_t) x * (uint64_t) y;
      /* If any of the upper 32 bits touched, overflow */
      if(tmp & 0xffffffff00000000ULL)
      *err = *err | CF_OVERFLOW_ERROR;
      return (uint32_t) tmp;
    }
    static inline uint64_t __check_uint64_add_unsigned_unsigned(uint64_t x, uint64_t y, int32_t* err) {
       if((ULLONG_MAX - y) < x)
            *err = *err | CF_OVERFLOW_ERROR;
       return x + y;
    }
    static inline uint64_t __check_uint64_mul_unsigned_unsigned(uint64_t x, uint64_t y, int32_t* err) {
      if(x == 0) return 0;
      if(ULLONG_MAX/x < y)
         *err = *err | CF_OVERFLOW_ERROR;
      return x * y;
    }
    static inline const uint8_t* check_ptr_add(const void* p, const size_t a, int32_t* err)
    {
      const uint8_t* result = 0;
      if (sizeof(void*) == 8)
        result = (const uint8_t*)__check_uint64_add_unsigned_unsigned((uintptr_t)p, (uintptr_t)a, err);
      else
        result = (const uint8_t*)__check_uint32_add_unsigned_unsigned((uintptr_t)p, (uintptr_t)a, err);
      return result;
    }//end check_ptr_add()
    static inline const size_t check_size_t_mul(const size_t b, const size_t a, int32_t* err)
    {
      size_t result = 0;
      if (sizeof(void*) == 8)
        result = (size_t)__check_uint64_mul_unsigned_unsigned(b, a, err);
      else
        result = (size_t)__check_uint32_mul_unsigned_unsigned(b, a, err);
      return result;
    }//end check_size_t_mul()

    static inline uint64_t _getSizedInt(const uint8_t *data, uint8_t valSize)
    {
      if (valSize == 1) {
          return (uint64_t)*data;
      } else if (valSize == 2) {
          uint16_t val = *(uint16_t *)data;
          return (uint64_t)CFSwapInt16BigToHost(val);
      } else if (valSize == 4) {
          uint32_t val = *(uint32_t *)data;
          return (uint64_t)CFSwapInt32BigToHost(val);
      } else if (valSize == 8) {
          uint64_t val = *(uint64_t *)data;
          return CFSwapInt64BigToHost(val);
      }
      return 0;
    }//end _getSizedInt()
    
    static inline bool _readInt(const uint8_t *ptr, const uint8_t *end_byte_ptr, uint64_t *bigint, const uint8_t **newptr)
    {
      if (end_byte_ptr < ptr)
        return false;
      uint8_t marker = *ptr++;
      if ((marker & 0xf0) != kCFBinaryPlistMarkerInt)
        return false;
      uint64_t cnt = 1 << (marker & 0x0f);
      int32_t err = CF_NO_ERROR;
      const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
      if (CF_NO_ERROR != err)
        return false;
      if (end_byte_ptr < extent)
        return false;
      // integers are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
      *bigint = _getSizedInt(ptr, cnt);
      ptr += cnt;
      if (newptr)
        *newptr = ptr;
      return true;
    }//end _readInt()
    
    // bytesptr points at a ref
    static inline uint64_t _getOffsetOfRefAt(const uint8_t *databytes, const uint8_t *bytesptr, const CFBinaryPlistTrailer *trailer) {
      // *trailer contents are trusted, even for overflows -- was checked when the trailer was parsed;
      // this pointer arithmetic and the multiplication was also already done once and checked,
      // and the offsetTable was already validated.
      const uint8_t *objectsFirstByte = databytes + 8;
      const uint8_t *offsetsFirstByte = databytes + trailer->_offsetTableOffset;
      if (bytesptr < objectsFirstByte || offsetsFirstByte - trailer->_objectRefSize < bytesptr)
        return std::numeric_limits<uint64_t>::max();

      uint64_t ref = _getSizedInt(bytesptr, trailer->_objectRefSize);
      if (trailer->_numObjects <= ref)
        return std::numeric_limits<uint64_t>::max();

      bytesptr = databytes + trailer->_offsetTableOffset + ref * trailer->_offsetIntSize;
      uint64_t off = _getSizedInt(bytesptr, trailer->_offsetIntSize);
      return off;
    }
  public:
    BinaryPlistReader(void) {}
    ~BinaryPlistReader(void) {}
  public:
    std::shared_ptr<IPlist> read(const unsigned char* data, const size_t size);
    void emitNull(void) const {}//std::cout << "emit null" << std::endl;}
    void emit(bool value) const {}//std::cout << "emit bool : " << value << std::endl;}
    void emit(int64_t value) const {}//std::cout << "emit int64_t: " << value << std::endl;}
    void emitUID(uint32_t value) const {}//std::cout << "emit uint32_t: " << value << std::endl;}
    void emit(const CFSInt128Struct& value) const {}//std::cout << "emit 128-wide" << std::endl;}
    void emit(float value) const {}//std::cout << "emit float : " << value << std::endl;}
    void emit(double value) const {}//std::cout << "emit double : " << value << std::endl;}
    void emitDate(double value) const {}//std::cout << "emit date : " << value << std::endl;}
    void emit(const std::string& value) const {}//std::cout << "emit ASCII string : " << value << std::endl;}
    void emit(const std::basic_string<wchar_t>& value) const {}//std::cout << "emit UTF16 string : " << std::endl;}
    void emit(const std::vector<unsigned char>& value) const {}//std::cout << "emit base64 data : " << value << std::endl;}
    void emitBeginArray(size_t size) const {}//std::cout << "emit begin array of size " << size << std::endl;}
    void emitEndArray(void) const {}//std::cout << "emit end array" << std::endl;}
    void emitBeginDict(size_t size) const {}//std::cout << "emit begin dict of size " << size << std::endl;}
    void emitEndDict(void) const {}//std::cout << "emit end dict" << std::endl;}
  protected:
    bool __CFTryParseBinaryPlist(const unsigned char* data, const size_t size, std::shared_ptr<IPlist>& outPlist);
    bool __CFBinaryPlistGetTopLevelInfo(const uint8_t* databytes, uint64_t datalen, uint8_t* marker, uint64_t* offset, CFBinaryPlistTrailer* trailer);
    bool __CFBinaryPlistCreateObjectFiltered(const uint8_t* databytes, uint64_t datalen, uint64_t startOffset, const CFBinaryPlistTrailer* trailer, std::map<uint64_t, std::shared_ptr<IPlist> >& objects, std::set<uint64_t>* set, CFIndex curDepth, std::shared_ptr<IPlist>& outPlist);
};
//end class BinaryPlistReader

std::shared_ptr<BinaryPlistReader::IPlist> BinaryPlistReader::PlistDict::getObjectForKey(const BinaryPlistReader::IPlist* other) const
{
  std::shared_ptr<IPlist> result;
  if (other)
  {
    std::shared_ptr<IPlist> pOther(const_cast<BinaryPlistReader::IPlist*>(other), [](IPlist*){});
    const auto found = this->value.find(pOther);
    if (found != this->value.cend())
      result = found->second;
  }//end if (other)
  return result;
}
//end BinaryPlistReader::PlistDict::getObjectForKey()

bool BinaryPlistReader::PlistComparer::operator()(const IPlist* p1, const IPlist* p2) const
{
  bool result =
    (!p1 && !p2) ? false :
    !p1 ? true :
    p1->less(p2);
  return result;
}
//end BinaryPlistReader::PlistComparer::operator()(const IPlist*, const IPlist*)

std::shared_ptr<BinaryPlistReader::IPlist> BinaryPlistReader::read(const unsigned char* data, const size_t size)
{
  std::shared_ptr<IPlist> result;
  this->__CFTryParseBinaryPlist(data, size, result);
  return result;
}
//end BinaryPlistReader::read()

bool BinaryPlistReader::__CFTryParseBinaryPlist(const unsigned char* data, const size_t size, std::shared_ptr<BinaryPlistReader::IPlist>& outPlist)
{
  bool result = false;
  uint8_t marker = 0;
  CFBinaryPlistTrailer trailer = {0};
  uint64_t offset = 0;
  const uint8_t* databytes = data;
  uint64_t datalen = size;
  if (8 <= datalen && this->__CFBinaryPlistGetTopLevelInfo(databytes, datalen, &marker, &offset, &trailer))
  {
    std::map<uint64_t, std::shared_ptr<IPlist> > objects;
    std::set<uint64_t>* set = 0;
    result = this->__CFBinaryPlistCreateObjectFiltered(databytes, datalen, offset, &trailer, objects, set, 0, outPlist);
  }
  return result;
}
//end BinaryPlistReader::__CFTryParseBinaryPlist()

bool BinaryPlistReader::__CFBinaryPlistGetTopLevelInfo(const uint8_t* databytes, uint64_t datalen, uint8_t* marker, uint64_t* offset, BinaryPlistReader::CFBinaryPlistTrailer* trailer)
{
  bool result = false;

  CFBinaryPlistTrailer trail = {0};

  if (!databytes || datalen < sizeof(trail) + 8 + 1)
    return false;
  // Tiger and earlier will parse "bplist00"
  // Leopard will parse "bplist00" or "bplist01"
  // SnowLeopard will parse "bplist0?" where ? is any one character
  if (0 != memcmp("bplist0", databytes, 7))
    return false;

  std::memmove(&trail, databytes + datalen - sizeof(trail), sizeof(trail));
  // In Leopard, the unused bytes in the trailer must be 0 or the parse will fail
  // This check is not present in Tiger and earlier or after Leopard
  trail._numObjects = CFSwapInt64BigToHost(trail._numObjects);
  trail._topObject = CFSwapInt64BigToHost(trail._topObject);
  trail._offsetTableOffset = CFSwapInt64BigToHost(trail._offsetTableOffset);
  
  // Don't overflow on the number of objects or offset of the table
  if (LONG_MAX < trail._numObjects)
    return false;
  if (LONG_MAX < trail._offsetTableOffset)
    return false;
  
  //  Must be a minimum of 1 object
  if (trail._numObjects < 1)
    return false;
  
  // The ref to the top object must be a value in the range of 1 to the total number of objects
  if (trail._numObjects <= trail._topObject)
    return false;
  
  // The offset table must be after at least 9 bytes of other data ('bplist??' + 1 byte of object table data).
  if (trail._offsetTableOffset < 9)
    return false;
  
  // The trailer must point to a value before itself in the data.
  if (datalen - sizeof(trail) <= trail._offsetTableOffset)
    return false;
  
  // Minimum of 1 byte for the size of integers and references in the data
  if (trail._offsetIntSize < 1)
    return false;
  if (trail._objectRefSize < 1)
    return false;
  
  int32_t err = 0;
  
  // The total size of the offset table (number of objects * size of each int in the table) must not overflow
  uint64_t offsetIntSize = trail._offsetIntSize;
  uint64_t offsetTableSize = __check_uint64_mul_unsigned_unsigned(trail._numObjects, offsetIntSize, &err);
  if (CF_NO_ERROR!= err)
    return false;
  
  // The offset table must have at least 1 entry
  if (offsetTableSize < 1)
    return false;
  
  // Make sure the size of the offset table and data sections do not overflow
  uint64_t objectDataSize = trail._offsetTableOffset - 8;
  uint64_t tmpSum = __check_uint64_add_unsigned_unsigned(8, objectDataSize, &err);
  tmpSum = __check_uint64_add_unsigned_unsigned(tmpSum, offsetTableSize, &err);
  tmpSum = __check_uint64_add_unsigned_unsigned(tmpSum, sizeof(trail), &err);
  if (CF_NO_ERROR != err)
    return false;
  
  // The total size of the data should be equal to the sum of offsetTableOffset + sizeof(trailer)
  if (datalen != tmpSum)
    return false;
  
  // The object refs must be the right size to point into the offset table. That is, if the count of objects is 260, but only 1 byte is used to store references (max value 255), something is wrong.
  if (trail._objectRefSize < 8 && (1ULL << (8 * trail._objectRefSize)) <= trail._numObjects)
    return false;
  
  // The integers used for pointers in the offset table must be able to reach as far as the start of the offset table.
  if (trail._offsetIntSize < 8 && (1ULL << (8 * trail._offsetIntSize)) <= trail._offsetTableOffset)
    return false;
  
  
  (void)check_ptr_add(databytes, 8, &err);
  if (CF_NO_ERROR != err)
    return false;
  const uint8_t *offsetsFirstByte = check_ptr_add(databytes, trail._offsetTableOffset, &err);
  if (CF_NO_ERROR != err)
    return false;
  (void)check_ptr_add(offsetsFirstByte, offsetTableSize - 1, &err);
  if (CF_NO_ERROR != err)
    return false;

  const uint8_t *bytesptr = databytes + trail._offsetTableOffset;
  uint64_t maxOffset = trail._offsetTableOffset - 1;
  for (CFIndex idx = 0; idx < trail._numObjects; idx++)
  {
    uint64_t off = _getSizedInt(bytesptr, trail._offsetIntSize);
    if (maxOffset < off)
      return false;
    bytesptr += trail._offsetIntSize;
  }

  bytesptr = databytes + trail._offsetTableOffset + trail._topObject * trail._offsetIntSize;
  uint64_t off = _getSizedInt(bytesptr, trail._offsetIntSize);
  if (off < 8 || trail._offsetTableOffset <= off)
    return false;
  if (trailer)
    *trailer = trail;
  if (offset)
    *offset = off;
  if (marker)
    *marker = *(databytes + off);

  result = true;
  return result;
}
//end BinaryPlistReader::__CFBinaryPlistGetTopLevelInfo()

bool BinaryPlistReader::__CFBinaryPlistCreateObjectFiltered(const uint8_t* databytes, uint64_t datalen, uint64_t startOffset, const BinaryPlistReader::CFBinaryPlistTrailer* trailer, std::map<uint64_t, std::shared_ptr<BinaryPlistReader::IPlist> >& objects, std::set<uint64_t>* set, BinaryPlistReader::CFIndex curDepth, std::shared_ptr<BinaryPlistReader::IPlist>& outPlist)
{
  auto it = objects.find(startOffset);
  if (it != objects.end())
  {
    outPlist = it->second;
    return true;
  }//end if (it != objects.end())

  // at any one invocation of this function, set should contain the offsets in the "path" down to this object
  if (set && (set->find(startOffset) != set->end()))
    return false;

  // databytes is trusted to be at least datalen bytes long
  // *trailer contents are trusted, even for overflows -- was checked when the trailer was parsed
  uint64_t objectsRangeStart = 8, objectsRangeEnd = trailer->_offsetTableOffset - 1;
  if (startOffset < objectsRangeStart || objectsRangeEnd < startOffset)
    return false;

  uint64_t off = 0;
  uint8_t marker = *(databytes + startOffset);
  switch (marker & 0xf0)
  {
    case kCFBinaryPlistMarkerNull:
	    switch (marker)
      {
        case kCFBinaryPlistMarkerNull:
            this->emitNull();
            outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistNull);
            return true;
        case kCFBinaryPlistMarkerFalse:
            this->emit(false);
            outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistBool(false));
            return true;
        case kCFBinaryPlistMarkerTrue:
            this->emit(true);
            outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistBool(true));
            return true;
      }
	    return false;
    case kCFBinaryPlistMarkerInt:
    {
	    const uint8_t *ptr = (databytes + startOffset);
	    int32_t err = CF_NO_ERROR;
	    ptr = check_ptr_add(ptr, 1, &err);
	    if (CF_NO_ERROR != err)
        return false;
	    uint64_t cnt = 1 << (marker & 0x0f);
	    const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	    if (CF_NO_ERROR != err)
        return false;
	    if (databytes + objectsRangeEnd < extent)
        return false;
	    if (16 < cnt)
        return false;
      // in format version '00', 1, 2, and 4-byte integers have to be interpreted as unsigned,
      // whereas 8-byte integers are signed (and 16-byte when available)
      // negative 1, 2, 4-byte integers are always emitted as 8 bytes in format '00'
      // integers are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
      uint64_t bigint = _getSizedInt(ptr, cnt);
      if (8 < cnt) {
          CFSInt128Struct val;
          val.high = 0;
          val.low = bigint;
          this->emit(val);
          outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistBigInteger(val));
      } else {
          this->emit((int64_t)bigint);
          outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistInteger(bigint));
      }
      // these are always immutable
      if (outPlist.get() != 0)
        objects[startOffset] = outPlist;
      return (outPlist.get() != 0);
    }
    case kCFBinaryPlistMarkerReal:
  	  switch (marker & 0x0f) {
	      case 2: {
          const uint8_t *ptr = (databytes + startOffset);
          int32_t err = CF_NO_ERROR;
          ptr = check_ptr_add(ptr, 1, &err);
          if (CF_NO_ERROR != err)
            return false;
          const uint8_t *extent = check_ptr_add(ptr, 4, &err) - 1;
          if (CF_NO_ERROR != err)
            return false;
          if (databytes + objectsRangeEnd < extent)
            return false;
          CFSwappedFloat32 swapped32;
          std::memmove(&swapped32, ptr, 4);
          float f = CFConvertFloat32SwappedToHost(swapped32);
          this->emit(f);
          outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistReal(f));
          // these are always immutable
          if (outPlist.get() != 0)
            objects[startOffset] = outPlist;
          return (outPlist.get() != 0);
        }
        case 3: {
            const uint8_t *ptr = (databytes + startOffset);
            int32_t err = CF_NO_ERROR;
            ptr = check_ptr_add(ptr, 1, &err);
            if (CF_NO_ERROR != err)
              return false;
            const uint8_t *extent = check_ptr_add(ptr, 8, &err) - 1;
            if (CF_NO_ERROR != err)
              return false;
            if (databytes + objectsRangeEnd < extent)
              return false;
            CFSwappedFloat64 swapped64;
            std::memmove(&swapped64, ptr, 8);
            double d = CFConvertFloat64SwappedToHost(swapped64);
            this->emit(d);
            outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistReal(d));
            // these are always immutable
            if (outPlist.get() != 0)
              objects[startOffset] = outPlist;
            return (outPlist.get() != 0);
        }
	   }
     return false;
   case kCFBinaryPlistMarkerDate & 0xf0:
     switch (marker) {
       case kCFBinaryPlistMarkerDate: {
         const uint8_t *ptr = (databytes + startOffset);
         int32_t err = CF_NO_ERROR;
         ptr = check_ptr_add(ptr, 1, &err);
         if (CF_NO_ERROR != err)
           return false;
         const uint8_t *extent = check_ptr_add(ptr, 8, &err) - 1;
         if (CF_NO_ERROR != err)
           return false;
         if (databytes + objectsRangeEnd < extent)
           return false;
         CFSwappedFloat64 swapped64;
         std::memmove(&swapped64, ptr, 8);
         double d = CFConvertFloat64SwappedToHost(swapped64);
         this->emitDate(d);
         outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistReal(d));
         // these are always immutable
         if (outPlist.get() != 0)
           objects[startOffset] = outPlist;
         return (outPlist.get() != 0);
       }
	   }
     return false;
   case kCFBinaryPlistMarkerData: {
     const uint8_t *ptr = databytes + startOffset;
     int32_t err = CF_NO_ERROR;
     ptr = check_ptr_add(ptr, 1, &err);
     if (CF_NO_ERROR != err)
       return false;
     CFIndex cnt = marker & 0x0f;
     if (0xf == cnt) {
        uint64_t bigint = 0;
        if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr))
          return false;
        if (LONG_MAX < bigint)
          return false;
        cnt = (CFIndex)bigint;
     }
     const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
     if (CF_NO_ERROR != err)
       return false;
     if (databytes + objectsRangeEnd < extent)
       return false;
     std::vector<unsigned char> data(ptr, ptr+cnt);
     this->emit(data);
     outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistData(data));
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
   }
   case kCFBinaryPlistMarkerASCIIString: {
	   const uint8_t *ptr = databytes + startOffset;
	   int32_t err = CF_NO_ERROR;
	   ptr = check_ptr_add(ptr, 1, &err);
	   if (CF_NO_ERROR != err)
       return false;
	   CFIndex cnt = marker & 0x0f;
	   if (0xf == cnt) {
       uint64_t bigint = 0;
	     if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr))
         return false;
	     if (LONG_MAX < bigint)
         return false;
	     cnt = (CFIndex)bigint;
	   }
	   const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	   if (CF_NO_ERROR != err)
       return false;
	   if (databytes + objectsRangeEnd < extent)
       return false;
     std::string s(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr)+cnt);
     this->emit(s);
     outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistString(s));
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
 	 }
   case kCFBinaryPlistMarkerUnicode16String: {
	   const uint8_t *ptr = databytes + startOffset;
	   int32_t err = CF_NO_ERROR;
	   ptr = check_ptr_add(ptr, 1, &err);
	   if (CF_NO_ERROR != err)
       return false ;
	   CFIndex cnt = marker & 0x0f;
	   if (0xf == cnt) {
       uint64_t bigint = 0;
	     if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr))
         return false;
	     if (LONG_MAX < bigint)
         return false;
	     cnt = (CFIndex)bigint;
	   }
	   const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
	   extent = check_ptr_add(extent, cnt, &err);	// 2 bytes per character
	   if (CF_NO_ERROR != err)
       return false;
	   if (databytes + objectsRangeEnd < extent)
       return false;
	   size_t byte_cnt = check_size_t_mul(cnt, sizeof(UniChar), &err);
	   if (CF_NO_ERROR != err)
       return false;
     std::vector<wchar_t> chars(byte_cnt);
     std::memmove(chars.data(), ptr, byte_cnt);
	   for (CFIndex idx = 0; idx < cnt; idx++) {
	     chars[idx] = CFSwapInt16BigToHost(chars[idx]);
	   }
     std::basic_string<wchar_t> s(chars.begin(), chars.end());
     this->emit(s);
     outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistWString(s));
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
   }
   case kCFBinaryPlistMarkerUID: {
     const uint8_t *ptr = databytes + startOffset;
     int32_t err = CF_NO_ERROR;
     ptr = check_ptr_add(ptr, 1, &err);
     if (CF_NO_ERROR != err)
       return false;
     CFIndex cnt = (marker & 0x0f) + 1;
     const uint8_t *extent = check_ptr_add(ptr, cnt, &err) - 1;
     if (CF_NO_ERROR != err)
       return false;
     if (databytes + objectsRangeEnd < extent)
       return false;
     // uids are not required to be in the most compact possible representation, but only the last 64 bits are significant currently
     uint64_t bigint = _getSizedInt(ptr, cnt);
     if (UINT32_MAX < bigint)
       return false;
     this->emitUID((uint32_t)bigint);
     outPlist = std::shared_ptr<IPlist>(new(std::nothrow) PlistUID((uint32_t)bigint));
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
   }
   case kCFBinaryPlistMarkerArray:
   case kCFBinaryPlistMarkerSet: {
     const uint8_t *ptr = databytes + startOffset;
     int32_t err = CF_NO_ERROR;
     ptr = check_ptr_add(ptr, 1, &err);
     if (CF_NO_ERROR != err)
       return false;
     CFIndex arrayCount = marker & 0x0f;
     if (0xf == arrayCount) {
       uint64_t bigint = 0;
       if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr))
         return false;
       if (LONG_MAX < bigint)
         return false;
       arrayCount = (CFIndex)bigint;
     }
     size_t byte_cnt = check_size_t_mul(arrayCount, trailer->_objectRefSize, &err);
     if (CF_NO_ERROR != err)
       return false;
     const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
     if (CF_NO_ERROR != err)
       return false;
     if (databytes + objectsRangeEnd < extent)
       return false;
     byte_cnt = check_size_t_mul(arrayCount, sizeof(void*), &err);
     if (CF_NO_ERROR != err)
       return false;
    std::shared_ptr<PlistArray> list(new(std::nothrow) PlistArray);
    if (!list.get())
      return false;
     std::set<uint64_t> localSet;
     if (!set && 15 < curDepth)
       set = &localSet;
     this->emitBeginArray(arrayCount);
     if (set)
       set->insert(startOffset);
     for (CFIndex idx = 0; idx < arrayCount; idx++) {
       std::shared_ptr<IPlist> pl;
       off = _getOffsetOfRefAt(databytes, ptr, trailer);
       if (!__CFBinaryPlistCreateObjectFiltered(databytes, datalen, off, trailer, objects, set, curDepth + 1, pl))
         return false;
       list->value.push_back(pl);
       ptr += trailer->_objectRefSize;
     }
     outPlist = list;
     this->emitEndArray();
     if (set)
       set->erase(startOffset);
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
   }
   case kCFBinaryPlistMarkerDict: {
     const uint8_t *ptr = databytes + startOffset;
     int32_t err = CF_NO_ERROR;
     ptr = check_ptr_add(ptr, 1, &err);
     if (CF_NO_ERROR != err)
       return false;
     CFIndex dictionaryCount = marker & 0x0f;
     if (0xf == dictionaryCount) {
       uint64_t bigint = 0;
       if (!_readInt(ptr, databytes + objectsRangeEnd, &bigint, &ptr))
         return false;
       if (LONG_MAX < bigint)
         return false;
       dictionaryCount = (CFIndex)bigint;
     }
     dictionaryCount = check_size_t_mul(dictionaryCount, 2, &err);
     if (CF_NO_ERROR != err)
       return false;
     size_t byte_cnt = check_size_t_mul(dictionaryCount, trailer->_objectRefSize, &err);
     if (CF_NO_ERROR != err)
       return false;
     const uint8_t *extent = check_ptr_add(ptr, byte_cnt, &err) - 1;
     if (CF_NO_ERROR != err)
       return false;
     if (databytes + objectsRangeEnd < extent)
       return false;
     byte_cnt = check_size_t_mul(dictionaryCount, sizeof(void*), &err);
     if (CF_NO_ERROR != err)
       return false;
     std::shared_ptr<PlistArray> list(new(std::nothrow) PlistArray);
     if (!list.get())
       return false;
     std::set<uint64_t> localSet;
     if (!set && 15 < curDepth)
       set = &localSet;
     this->emitBeginDict(dictionaryCount/2);
     if (set)
       set->insert(startOffset);
     for (CFIndex idx = 0; idx < dictionaryCount; idx++) {
       std::shared_ptr<IPlist> pl;
       off = _getOffsetOfRefAt(databytes, ptr, trailer);
       if (!__CFBinaryPlistCreateObjectFiltered(databytes, datalen, off, trailer, objects, set, curDepth + 1, pl) || (idx < dictionaryCount / 2 && !pl->isPrimitive()))
           return false;
       list->value.push_back(pl);
       ptr += trailer->_objectRefSize;
     }
     std::shared_ptr<PlistDict> dict(new(std::nothrow) PlistDict);
     for (CFIndex idx = 0; idx < dictionaryCount / 2; idx++) {
       std::shared_ptr<IPlist> value = list->value[idx+dictionaryCount / 2];
       std::shared_ptr<IPlist> key = list->value[idx];
       dict->value[key] = value;
     }
     outPlist = dict;
     this->emitEndDict();
     if (set)
       set->erase(startOffset);
     if (outPlist.get() != 0)
       objects[startOffset] = outPlist;
     return (outPlist.get() != 0);
	 }
 }
 return false;
}
//end __CFBinaryPlistCreateObjectFiltered::__CFBinaryPlistCreateObjectFiltered()

void usage(int argc, char* argv[])
{
  std::cout << "Usage : " << argv[0] << " " << "<filename.pdf|filename.emf>" << std::endl;
}
//end usage()

int main(int argc, char* argv[])
{
  int result = 0;
  if (argc < 2)
  {
    usage(argc, argv);
    result = -1;
  }//end if (argc < 2)
  else//if (argc >= 2)
  {
    const char* filename = argv[1];
    std::cout << "extracting PDF data from <" << filename << ">" << std::endl;
    std::vector<unsigned char> pdfData = extractPDF(filename);
    std::cout << "pdfData size : " << pdfData.size() << std::endl;
    
    std::vector<std::string> latexitStrings;
    PoDoFo::PdfMemDocument* pDocument = 0;
    try{
      std::cout << "creating PDF document to parse" << std::endl;
      pDocument = pdfData.empty() ? 0 : new(std::nothrow) PoDoFo::PdfMemDocument();
      if (pDocument != 0)
      {
        std::cout << "Loading data..." << std::endl;
        pDocument->LoadFromBuffer(reinterpret_cast<const char*>(pdfData.data()), static_cast<long>(pdfData.size()));
        std::cout << "parsing..." << std::endl;
        LaTeXiTMetaDataExtractor extractor;
        extractor.process(*pDocument);
        latexitStrings = extractor.getLatexitStrings();
        std::cout << "...parsing done" << std::endl;
      }//end if (pDocument != 0)
    }
    catch(PoDoFo::PdfError& e){
      e.PrintErrorMsg();
    }
    catch(std::exception& e){
      std::cout << e.what() << std::endl;
    }
    delete pDocument;

    std::cout << "latexit strings :" << std::endl;
    size_t metadataIndex = 0;
    for(const auto& s : latexitStrings)
    {
      std::cout << "full string : " << s << std::endl;
      std::string base64dataString = ExtractLaTeXitBase64Data(s);
      std::cout << "base64data : " << base64dataString << std::endl;
      std::vector<unsigned char> compressedData = base64_decode(base64dataString);
      std::cout << "raw compressedData size : " << compressedData.size() << std::endl;
      std::vector<unsigned char> uncompressedData = zipuncompress(compressedData);
      std::cout << "raw uncompressedData size : " << uncompressedData.size() << std::endl;
      std::cout << "uncompressedData : " << uncompressedData << std::endl;
      BinaryPlistReader binaryPlistReader;
      std::shared_ptr<BinaryPlistReader::IPlist> plist = binaryPlistReader.read(uncompressedData.data(), uncompressedData.size());
      std::cout << "decoded plist : " << std::endl;
      std::cout << plist->to_string() << std::endl;
      BinaryPlistReader::PlistDict* dict = dynamic_cast<BinaryPlistReader::PlistDict*>(plist.get());
      std::stringstream texStream;
      if (dict != 0)
      {
        BinaryPlistReader::PlistString preambleKey("preamble");
        BinaryPlistReader::PlistString sourceKey("source");
        BinaryPlistReader::PlistString modeKey("type");
        BinaryPlistReader::PlistString altmodeKey("mode");
        std::shared_ptr<BinaryPlistReader::IPlist> preamble = dict->getObjectForKey(preambleKey);
        std::shared_ptr<BinaryPlistReader::IPlist> source = dict->getObjectForKey(sourceKey);
        std::shared_ptr<BinaryPlistReader::IPlist> mode = dict->getObjectForKey(modeKey);
        std::shared_ptr<BinaryPlistReader::IPlist> altmode = dict->getObjectForKey(altmodeKey);
        if (preamble.get() != 0)
        {
            std::string preambleString;
            preambleString = preamble->to_string();
            texStream << preambleString << std::endl;
            std::string extraPreamble;
            extraPreamble = "\\pagestyle{empty}";
            if (preambleString.find(extraPreamble) == std::string::npos) 
            {
                texStream << extraPreamble << std::endl;
            }//end if (preambleString.find(extraPreamble) == std::string::npos)
        }//end if (preamble.get() != 0)
        if (source.get() != 0)
        {
          std::string prefix;
          std::string suffix;
          int modeInt = -1;
          if (mode.get() != 0)
          {
              modeInt = std::atoi(mode->to_string().c_str());
          }//end if (mode.get() != 0)
          else if (altmode.get() != 0)
          {
              modeInt = std::atoi(altmode->to_string().c_str());
          }//end if (altmode.get() != 0)
          if (modeInt == 0)//DISPLAY
          {
              prefix = "\\[";
              suffix = "\\]";
          }//end if (modeInt == 0)
          else if (modeInt == 1)//INLINE
          {
              prefix = "$";
              suffix = "$";
          }//end if (modeInt == 1)
          else if (modeInt == 2) {//TEXT
          }//end if (modeInt == 2)
          else if (modeInt == 3)//EQNARRAY
          {
              prefix = "\\begin{eqnarray*}";
              suffix = "\\end{eqnarray*}";
          }//end if (modeInt == 3)
          else if (modeInt == 4)//ALIGN
          {
              prefix = "\\begin{align*}";
              suffix = "\\end{align*}";
          }//end if (modeInt == 4)
          
          texStream << "\\begin{document}" << std::endl;
          texStream << prefix << source->to_string() << suffix << std::endl;
          texStream << "\\end{document}" << std::endl;
        }//end if (source.get() != 0)
      }//end if (dict != 0)
      std::string texString = texStream.str();
      std::cout << "full TeX : " << texString << std::endl;
      if (!texString.empty())
      {
        std::string texFileName = std::string(filename)+(!metadataIndex ? "" : ("-"+std::to_string(metadataIndex)))+".tex";
        FILE* fTex = fopen(texFileName.c_str(), "wb");
        if (fTex)
        {
          fwrite(texString.c_str(), sizeof(unsigned char), texString.size(), fTex);
          std::cout << "written to " << texFileName << std::endl;
          fclose(fTex);
        }//end if (fTex)
      }//end if (!texString.empty())
      ++metadataIndex;
    }//end for each s

  }//end if (argc >= 2)
  return result;
}
//end main()
