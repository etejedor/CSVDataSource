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
   auto i = 0U;

   auto parsedCols = ParseColumns(line);

   for (auto &col : parsedCols) {
      auto &colType = fColTypes[fHeaders[i]];
      
      if (colType == "int") {
         record.emplace_back(new int(std::stoi(col))); // TODO: LONG?
      }
      else if (colType == "double") {
         record.emplace_back(new double(std::stod(col)));
      }
      else if (colType == "bool") {
         bool *b = new bool();
         record.emplace_back(b);
         // Ignore case, just like boolRegex
         std::transform(col.begin(), col.end(), col.begin(), ::tolower);
         std::istringstream is(col);
         is >> std::boolalpha >> *b;
      }
      else {
         record.emplace_back(new std::string(col));
      }
      ++i;
   }
}

void TCsvDS::GenerateHeaders(size_t size)
{
   for (size_t i = 0; i < size; ++i) {
      fHeaders.push_back("Col" + std::to_string(i));
   }
}

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
   // TODO: Date

   fColTypes[fHeaders[idxCol]] = type;
}

std::vector<std::string> TCsvDS::ParseColumns(std::string &line) {
   std::vector<std::string> columns;

   for (size_t i = 0; i < line.size(); ++i) {
     i = ParseValue(line, columns, i);
   }

   return columns;
}

size_t TCsvDS::ParseValue(std::string &line, std::vector<std::string> &columns, size_t i)
{
   std::stringstream val;
   bool quoted = false;
   
   for (; i < line.size(); ++i) {
      if (line[i] == fDelimiter && !quoted) {
         break;
      }
      else if (line[i] == '"') {
         // Keep just one quote for escaped quotes, none for the normal quotes
         if (line[i + 1] != '"') {
            quoted = !quoted;
         }
         else {
            val << line[++i];
         }
      }
      else {
         val << line[i];
      }
   }

   columns.emplace_back(val.str());

   return i;
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

   // Read all records and store them in memory
   while (std::getline(stream, line)) {
      fRecords.emplace_back();
      FillRecord(line, fRecords.back());
   }

   // Generate headers if not provided in the CSV file
   if (!fRecords.empty() && !readHeaders) {
      GenerateHeaders(fRecords[0].size());
   }
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

const std::vector<std::string> &TCsvDS::GetColumnNames() const
{
   return fHeaders;
}

const std::vector<std::pair<ULong64_t, ULong64_t>> &TCsvDS::GetEntryRanges() const
{
   return fEntryRanges;
}

std::string TCsvDS::GetTypeName(std::string_view colName) const
{
   if (!HasColumn(colName)) {
      auto e = "The dataset does not have column " + colName;
      throw std::runtime_error(e);
   }

   return fColTypes.at(colName.data());
}

bool TCsvDS::HasColumn(std::string_view colName) const
{
   return fHeaders.end() != std::find(fHeaders.begin(), fHeaders.end(), colName);
}

void TCsvDS::SetEntry(unsigned int slot, ULong64_t entry)
{
   auto nColumns = fHeaders.size();
   
   for (auto i : ROOT::TSeqU(nColumns)) {
      // Update the address of every column of the slot to point to the record
      fColAddresses[i][slot] = fRecords[entry][i];
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