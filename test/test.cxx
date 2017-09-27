#include "ROOT/TCsvDS.hxx"
#include "ROOT/TSeq.hxx"
#include "ROOT/TDataFrame.hxx"

#include <iostream>
#include <thread>
#include <vector>

using namespace ROOT::Experimental;
using namespace ROOT::Experimental::TDF;

void testSource() {

   ROOT::EnableThreadSafety();

   TCsvDS tds("ages.csv", true);

   /*const auto nSlots = 2U;
   tds.SetNSlots(nSlots);

  for (auto &&name : tds.GetColumnNames()) {
    std::cout << name << std::endl;
  }

  for (auto &&colName : {"test", "A"}) {
    std::cout << "Has column \"" << colName << "\" ? " << tds.HasColumn(colName)
              << std::endl;
  }

  std::cout << "Typename of A is " << tds.GetTypeName("A") << std::endl;

  auto ranges = tds.GetEntryRanges();

  auto slot = 0U;
  for (auto &&range : ranges) {
    printf("Chunk %u , Entry Range %llu - %llu\n", slot, range.first,
           range.second);
    slot++;
  }

  auto vals = tds.GetColumnReaders<ULong64_t>("B");
  std::vector<std::thread> pool;
  slot = 0U;
  for (auto &&range : ranges) {
    auto work = [slot, range, &tds, &vals]() {
      for (auto i : ROOT::TSeq<ULong64_t>(range.first, range.second)) {
        tds.SetEntry(slot, i);
        printf("Value of B for entry %llu is %f\n", i, *((double*)*vals[slot]));
      }
    };
    pool.emplace_back(work);
    slot++;
  }

  for (auto &&t : pool)
    t.join();*/

}

void testMore() {
  
  ROOT::EnableImplicitMT(2);

  std::unique_ptr<TDataSource> tds(new TCsvDS("doubles.csv", true));
  TDataFrame tdf(std::move(tds));
  auto m = tdf.Max<double>("A");
  auto c = tdf.Count();
  std::cout << "The TDF with TDS has " << *c
            << " records and the max of A is " << *m << std::endl;

}


int main() {

  testSource();

  //testMore();

}
