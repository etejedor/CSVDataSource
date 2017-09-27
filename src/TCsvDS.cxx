#include <ROOT/TCsvDS.hxx>
#include <ROOT/TDFUtils.hxx>
#include <ROOT/TSeq.hxx>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

namespace ROOT {
namespace Experimental {
namespace TDF {

// Regular expressions for type inference
std::regex TCsvDS::intRegex("^-?\\d+$");
std::regex TCsvDS::doubleRegex("^\\d+\\.?\\d*$");
std::regex TCsvDS::boolRegex("^(true|false)$", std::regex_constants::ECMAScript | std::regex_constants::icase);
std::regex TCsvDS::quotedRegex("^\"[^\"].*[^\"]\"$");


std::vector<void *> TCsvDS::GetColumnReadersImpl(std::string_view colName, const std::type_info &)
{
   const auto &colNames = GetColumnNames();

   if (fColAddresses.empty()) {
      auto nColumns = colNames.size();
      // Initialise the entire set of addresses
      fColAddresses.resize(nColumns, std::vector<void *>(fNSlots, nullptr));
   }

   const auto index = std::distance(colNames.begin(), std::find(colNames.begin(), colNames.end(), colName));
   std::vector<void *> ret(fNSlots);
   for (auto slot : ROOT::TSeqU(fNSlots)) {
      ret[slot] = (void *)&fColAddresses[index][slot];
   }
   return ret;
}


void TCsvDS::FillHeaders(std::string& line)
{
   std::istringstream lineStream(line);
   std::string col;
   while (std::getline(lineStream, col, fDelimiter)) {
      fHeaders.emplace_back(col);
   }
}

void TCsvDS::FillRecord(std::string& line, Record& record)
{
   std::istringstream lineStream(line);
   std::string col;
   auto i = 0U;
   while (std::getline(lineStream, col, fDelimiter)) {
      auto &colType = fColTypes[fHeaders[i]];
      
      if (std::regex_match(col, quotedRegex)) { // TODO: Do this while parsing
         col = col.substr(1, col.size() - 2);
      }

      if (colType == "int") {
         record.emplace_back(new int(std::stoi(col))); // OVERFLOW?
      }
      else if (colType == "double") {
         record.emplace_back(new double(std::stod(col)));
      }
      else if (colType == "bool") {
         bool *b = new bool();
         record.emplace_back(b);
         std::istringstream is(col);
         is >> std::boolalpha >> *b; // TODO: what if it is not "true" or "false"?
      }
      else {
         record.emplace_back(new std::string(col)); // TODO: remove quotes
      }
      ++i;
   }
}

void TCsvDS::InferType(std::string &col, unsigned int idxCol) {
   // Remove quotes, if present, before inferring type
   if (std::regex_match(col, quotedRegex)) { // TODO: regex only from gcc49!!
      col = col.substr(1, col.size() - 2);
   }

   std::string type;
   if (std::regex_match(col, intRegex)) {
      type = "int";
   }
   else if (std::regex_match(col, doubleRegex)) {
      type = "double";
   }
   else if (std::regex_match(col, boolRegex)) {
      type = "bool";
   }
   else { // everything else is a string
      type = "std::string";
   }

   fColTypes[fHeaders[idxCol]] = type;
}

void TCsvDS::InferColTypes(std::string &line)
{
   std::istringstream lineStream(line);
   std::string col;
   auto i = 0U;
   while (std::getline(lineStream, col, fDelimiter)) {
      InferType(col, i);
      ++i;
   }
}

void TCsvDS::GenerateHeaders(size_t size)
{
   for (size_t i = 0; i < size; ++i) {
      fHeaders.push_back("Col" + std::to_string(i));
   }
}

TCsvDS::TCsvDS(std::string_view fileName, bool readHeaders, char delimiter) // TODO: Let users specify types?
   : fFileName(fileName), fDelimiter(delimiter)
{
   std::ifstream stream(fFileName);
   std::string line;

   // Read the headers if present
   if (readHeaders) {
      if (std::getline(stream, line)) {
         FillHeaders(line);
      }
      else {
         auto msg = "Error reading headers of CSV file " + fileName;
         throw std::runtime_error(msg);
      }
   }

   // Infer the types of the columns with first record
   if (std::getline(stream, line)) {
      InferColTypes(line);
      fRecords.emplace_back();
      FillRecord(line, fRecords.back());
   }

   // Read the rest of the records
   while (std::getline(stream, line)) {
      fRecords.emplace_back();
      FillRecord(line, fRecords.back());
   }

   // Generate headers if not provided in the CSV
   if (!fRecords.empty() && !readHeaders) {
      GenerateHeaders(fRecords[0].size());
   }

   for (auto &header : fHeaders)
      std::cout << fColTypes[header] << ",";
   std::cout << std::endl;
   
   for (auto &header : fHeaders)
      std::cout << header << ",";
   std::cout << std::endl;

   /*for (auto &r : fRecords) {
      for (auto &s : r)
         std::cout << s << ",";
      std::cout << std::endl;
   }*/
}

TCsvDS::~TCsvDS()
{
   for (auto &record : fRecords) {
      for (size_t i = 0; i < record.size(); ++i) {
         void *p = record[i];
         auto &colType = fColTypes[fHeaders[i]];

         if (colType == "int") {
            delete static_cast<int*>(p);
         }
         else if (colType == "double") {
            delete static_cast<double*>(p);
         }
         else if (colType == "bool") {
            delete static_cast<bool*>(p);
         }
         else {
            delete static_cast<std::string*>(p);
         }
      }
   }
}

std::string TCsvDS::GetTypeName(std::string_view colName) const
{
   if (!HasColumn(colName)) {
      auto e = "The dataset does not have column " + colName;
      throw std::runtime_error(e);
   }

   return fColTypes.at(colName.data());
}

const std::vector<std::string> &TCsvDS::GetColumnNames() const
{
   return fHeaders;
}

bool TCsvDS::HasColumn(std::string_view colName) const
{
   return fHeaders.end() != std::find(fHeaders.begin(), fHeaders.end(), colName);
}

const std::vector<std::pair<ULong64_t, ULong64_t>> &TCsvDS::GetEntryRanges() const
{
   return fEntryRanges;
}

void TCsvDS::SetEntry(unsigned int slot, ULong64_t entry)
{
   auto nColumns = fHeaders.size();
   
   for (auto i : ROOT::TSeqU(nColumns)) {
      // Update the address of every column of the slot to point to the record
      fColAddresses[i][slot] = &fRecords[entry][i];
   }
}

void TCsvDS::SetNSlots(unsigned int nSlots)
{
   assert(0U == fNSlots && "Setting the number of slots even if the number of slots is different from zero.");

   fNSlots = nSlots;

   auto nRecords = fRecords.size();
   auto chunkSize = nRecords / fNSlots;
   auto remainder = 1U == fNSlots ? 0 : nRecords % fNSlots;
   auto start = 0UL;
   auto end = 0UL;

   for (auto i : ROOT::TSeqU(fNSlots)) {
      start = end;
      end += chunkSize;
      fEntryRanges.emplace_back(start, end);
      (void)i;
   }
   fEntryRanges.back().second += remainder;
}

} // ns TDF
} // ns Experimental
} // ns ROOT