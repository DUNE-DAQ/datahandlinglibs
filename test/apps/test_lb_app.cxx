#include "datahandlinglibs/opmon/datahandling_info.pb.h"

#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"
#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"

#include "fdreadoutlibs/DUNEWIBEthTypeAdapter.hpp"

using namespace dunedaq::datahandlinglibs;
using namespace dunedaq::fdreadoutlibs::types;

int
main(int /*argc*/, char** /*argv[]*/)
{
    std::vector<int> nums{ 3, 1, 2, 4, 6, 5 };
    std::vector<DUNEWIBEthTypeAdapter> frames(6);
    for (int i = 0; i < nums.size(); ++i) {
        frames[i].set_timestamp(nums[i]);
    }

    std::vector<int> sorted_nums{ 1, 2, 3, 4, 5, 6 };
    std::vector<DUNEWIBEthTypeAdapter> sorted_frames(6);
    for (int i = 0; i < sorted_nums.size(); ++i) {
        sorted_frames[i].set_timestamp(sorted_nums[i]);
    }

    SkipListLatencyBufferModel<DUNEWIBEthTypeAdapter> skip_list;
    FixedRateQueueModel<DUNEWIBEthTypeAdapter> fixed_rate_queue(sorted_nums.size() + 1);
    BinarySearchQueueModel<DUNEWIBEthTypeAdapter> binary_search_queue(sorted_nums.size() + 1);

    for (auto frame : frames) {
        skip_list.write(std::move(frame));
    }

    for (auto sorted_frame : sorted_frames) {
        fixed_rate_queue.write(std::move(sorted_frame));
    }

    for (auto sorted_frame : sorted_frames) {
        binary_search_queue.write(std::move(sorted_frame));
    }

    std::cout << "Skip list occupancy: "           << skip_list.occupancy()           << " front: " << skip_list.front()->get_timestamp()           << " back: " << skip_list.back()->get_timestamp() << std::endl;
    std::cout << "Fixed rate queue occupancy: "    << fixed_rate_queue.occupancy()    << " front: " << fixed_rate_queue.front()->get_timestamp()    << " back: " << fixed_rate_queue.back()->get_timestamp() << std::endl;
    std::cout << "Binary search queue occupancy: " << binary_search_queue.occupancy() << " front: " << binary_search_queue.front()->get_timestamp() << " back: " << binary_search_queue.back()->get_timestamp() << std::endl;
}
