#ifndef H_UNCHOKE
  #define H_UNCHOKE

  #include <algorithm>
  #include <cmath>
  #include <iostream>
  #include <iterator>
  #include <random>
  #include <list>
  #include <map>
  #include <queue>

  #include "file.hpp"
  #include "peer.hpp"

  namespace BitTorrent{
    /*Definitions*/
    typedef std::map<size_t, std::list<Peer::id>> QueryMap;

    struct Stat{
      enum Type { LEECHER, SEEDER, FREERIDER };

      Type type = LEECHER;
      Peer::Schedule::timestamp t_entry = 0;
      Peer::Schedule::timestamp t_completed = 0;
      Peer::Schedule::timestamp t_leave = 0;
      uint64_t nb_subpiecebyunch = 0;
      uint64_t nb_subpiecebyoptunch = 0;
      uint64_t nb_subpiecefromseed = 0;
      std::vector<Peer::Schedule::timestamp> t_obtpiece;
      std::vector<uint64_t> nb_subpiecebyopt;
      std::list<std::pair<Peer::Schedule::timestamp, size_t>> l_tnbleecher;
      std::list<std::pair<Peer::Schedule::timestamp, size_t>> l_tnbdownload;
      std::vector<uint64_t> nb_subpiecerandom;
    };
    extern std::map<Peer::id, Stat> statistic;

    const size_t nb_unchpeer = 3;
    const size_t nb_optunchpeer = 1;
    const int annoucement_period = 1800;
    const File::kbyte bw_up = 128;
    const File::kbyte bw_dw = 256;
    const size_t nb_peerbyannoucement = 50;
    const Peer::Schedule::timestamp unch_period = 10;
    const Peer::Schedule::timestamp optunch_period = 30;
    extern size_t nb_piece;
    extern Peer::Schedule::timestamp seeding_period;
    extern Peer::Schedule::timestamp leecherarrival_period;
    extern Peer::Schedule::timestamp freeriderarrival_period;
    extern uint16_t nb_leecher;
    extern uint16_t nb_totleecher;
    extern uint16_t nb_seeder;
    extern uint16_t nb_totseeder;
    extern uint16_t nb_freerider;
    extern uint16_t nb_totfreerider;
    extern Peer::Schedule::timestamp t_seederpeak;
    extern uint16_t v_seederpeak;

    /*Bitfield*/
    std::vector<uint8_t> getRawBitf(const File&);
    std::vector<uint8_t> getEmptyBitf(const File&);

    /*Misc behavior*/
    inline void onevent_nop(Peer::id,
                     const std::list<Peer::id>&,
                     Peer::Network&,
                     Peer::Schedule&,
                     std::mt19937&) {}
    inline void onpiece_nop(Peer::id,
                     const std::list<Peer::id>&,
                     size_t,
                     Peer::Network&,
                     Peer::Schedule&,
                     std::mt19937&) {}
    inline std::list<Peer::id> onunch_nop(Peer::id,
                                   const std::list<Peer::id>&,
                                   Peer::Network&,
                                   Peer::Schedule&,
                                   std::mt19937&)
    {
      return std::list<Peer::id>();
    }

    /*Request*/
    std::list<size_t, Peer::id>
      requestRarest(Peer::id, const QueryMap&, Peer::Network&);
    std::list<size_t, Peer::id>
      requestPending(Peer::id, const QueryMap&, Peer::Network&);
    std::list<size_t, Peer::id>
      requestRandom(const QueryMap&, Peer::Network&, std::mt19937&);

    /*Unchoking*/
    std::list<Peer::id> unchokeBest(Peer::id,
                                    const std::list<Peer::id>&,
                                    Peer::Network&,
                                    std::mt19937&);
    std::list<Peer::id> unchokeFirst(Peer::id,
                                     const std::list<Peer::id>&,
                                     Peer::Network&,
                                     std::mt19937&);
    std::list<Peer::id> unchokeRandom(Peer::id,
                                      const std::list<Peer::id>&,
                                      Peer::Network&,
                                      std::mt19937&);

    /*Event*/
    void downloadPiece(Peer::Network&, std::mt19937&, Peer::Schedule&,
                       Peer::EventData&&);
    void makeFetchPieces(Peer::Network&, std::mt19937&, Peer::Schedule&,
                         Peer::EventData&&);
    void makeFetchPeers(Peer::Network&, std::mt19937&, Peer::Schedule&,
                        Peer::EventData&&);
    void makeFetchPeersOnce(Peer::Network&, std::mt19937&, Peer::Schedule&,
                            Peer::EventData&&);
    void addPeer(Peer::Network&, std::mt19937&, Peer::Schedule&,
                 Peer::EventData&&);
    void removePeer(Peer::Network&, std::mt19937&, Peer::Schedule&,
                    Peer::EventData&&);
    void makeUnchoke(Peer::Network&, std::mt19937&, Peer::Schedule&,
                     Peer::EventData&&);
    void makeOptUnchoke(Peer::Network&, std::mt19937&, Peer::Schedule&,
                        Peer::EventData&&);
    void decrementLeecher(Peer::Network&, std::mt19937&, Peer::Schedule&,
                          Peer::EventData&&);
    void decrementSeeder(Peer::Network&, std::mt19937&, Peer::Schedule&,
                         Peer::EventData&&);
    void decrementFreerider(Peer::Network&, std::mt19937&, Peer::Schedule&,
                            Peer::EventData&&);


    /*Management*/
    void createDownload(Peer::id, Peer::id, size_t, Peer::Network&,
                        Peer::Schedule&);
    std::list<size_t> createMostDownload
      (Peer::id, const std::list<std::pair<size_t, Peer::id>>&, Peer::Network&,
       Peer::Schedule&);
    QueryMap getMapPiece(Peer::id, const std::list<Peer::id>&, Peer::Network&);
    void turnIntoSeeder(Peer&);
    std::string reportState(const Peer::Network&,
                            const Peer::Schedule& schedule);

    namespace Leecher{
      extern const Peer::bitfreq& onBitfRequest;
      Peer::onevent onEntrance;
      Peer::onpiece onPieceComplete;
      Peer::onpiece onPieceCancel;
      Peer::onevent onPieceFetching;
      Peer::onevent onPeerFetching;
      Peer::onunch onUnchoking;
      Peer::onunch onOptUnchoking;
      Peer::onevent onFileComplete;
    };

    namespace Seeder{
      extern const Peer::bitfreq& onBitfRequest;
      Peer::onevent onEntrance;
      extern const Peer::onpiece& onPieceComplete;
      extern const Peer::onpiece& onPieceCancel;
      extern const Peer::onevent& onPieceFetching;
      extern const Peer::onevent& onPeerFetching;
      Peer::onunch onUnchoking;
      extern const Peer::onunch& onOptUnchoking;
      extern const Peer::onevent& onFileComplete;
    };

    namespace FreeRider{
      extern Peer::Schedule::timestamp annoucement_period;
      extern float annoucement_period_increase;

      extern const Peer::bitfreq& onBitfRequest;
      Peer::onevent onEntrance;
      extern const Peer::onpiece& onPieceComplete;
      extern const Peer::onpiece& onPieceCancel;
      Peer::onevent onPieceFetching;
      Peer::onevent onPeerFetching;
      extern const Peer::onunch& onUnchoking;
      extern const Peer::onunch& onOptUnchoking;
      Peer::onevent onFileComplete;
    };
  };

#endif
