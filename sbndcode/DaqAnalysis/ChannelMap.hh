#ifndef ChannelMap_h
#define ChannelMap_h

#include "lardataobj/RawData/RawDigit.h"

namespace daqAnalysis {
  class ChannelMap;
}

// maps wire id's to and from card no, fem no, channel index
class daqAnalysis::ChannelMap {
public:
  typedef raw::ChannelID_t wire_id_t;
  struct board_channel {
    size_t slot_no;
    size_t fem_no;
    size_t channel_ind;
  };

  // TODO: Implement
  static board_channel Wire2Channel(wire_id_t wire) {
    return board_channel {0, 0, (size_t) wire };
  }

  //TODO: Imeplement
  static wire_id_t Channel2Wire(board_channel channel) {
    return (wire_id_t) channel.channel_ind;
  }

  static wire_id_t Channel2Wire(size_t card_no, size_t fem_no, size_t channel_id) {
    board_channel channel {card_no, fem_no, channel_id};
    return Channel2Wire(channel);
  }

  // TODO: Implement
  static unsigned Board(unsigned fem) {
    return 0;
  }

  // TODO: Implement
  // 1 == induction plane
  // 2 == collection plane
  static unsigned PlaneType(unsigned wire_no) {
    bool is_induction = wire_no < 240;
    if (is_induction) {
      return 1;
    }
    else {
      return 2;
    }
  }

  static const size_t n_boards = 1;
  static const size_t n_fem_per_board = 1;
  static const size_t n_channel_per_fem = 16;
};
#endif
