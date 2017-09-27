#ifndef ROOT_TCSVTDS
#define ROOT_TCSVTDS

#include "ROOT/TDataSource.hxx"

#include <map>
#include <fstream>
#include <vector>
#include <regex>

namespace ROOT {
namespace Experimental {
namespace TDF {

class TCsvDS final : public ROOT::Experimental::TDF::TDataSource {
private:
   typedef std::vector<void *> Record;

   unsigned int fNSlots = 0U;
   std::string fFileName;
   char fDelimiter;
   std::vector<std::string> fHeaders;
   std::map<std::string, std::string> fColTypes;

   std::vector<std::vector<void *>> fColAddresses; // first container-> slot, second -> column;
   std::vector<std::pair<ULong64_t, ULong64_t>> fEntryRanges;

   std::vector<Record> fRecords; // first container-> record, second -> column;

   static std::regex intRegex, doubleRegex, boolRegex, quotedRegex;
   
   std::vector<void *> GetColumnReadersImpl(std::string_view, const std::type_info &);
   void FillHeaders(std::string&);
   void FillRecord(std::string&, Record&);
   void GenerateHeaders(size_t);
   void InferColTypes(std::string&);
   void InferType(std::string&, unsigned int);


public:
   TCsvDS(std::string_view fileName, bool readHeaders = false, char delimiter = ',');
   ~TCsvDS();
   std::string GetTypeName(std::string_view colName) const;
   const std::vector<std::string> &GetColumnNames() const;
   bool HasColumn(std::string_view colName) const;
   const std::vector<std::pair<ULong64_t, ULong64_t>> &GetEntryRanges() const;
   void SetEntry(unsigned int slot, ULong64_t entry);
   void SetNSlots(unsigned int nSlots);
};

} // ns TDF
} // ns Experimental
} // ns ROOT

#endif
