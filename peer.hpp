#ifndef H_PEER
  #define H_PEER

  #include <algorithm>
  #include <list>
  #include <map>
  #include <random>
  #include <set>
  #include <utility>
  #include <vector>

  #include "file.hpp"
  #include "timeline.hpp"

  class Peer{
    /**Definitions**/
    public:
      typedef uint64_t id;

      class Network;
      class EventData;
      typedef Timeline<EventData, Network&, std::mt19937&> Schedule;
      typedef std::vector<uint8_t> (bitfreq) (const File&);
      typedef void (onevent)
        (id, const std::list<id>&, Network&, Schedule&, std::mt19937&);
      typedef void (onpiece)
        (id, const std::list<id>&, size_t, Network&, Schedule&, std::mt19937&);
      typedef std::list<id> (onunch)
        (id, const std::list<id>&, Network&, Schedule&, std::mt19937&);

      struct EventData{
        struct PeerJob{
          id peer;
          Timeline<int>::timestamp t;
        };

        struct FileSharing{
          id dwpeer;
          id uppeer;
          size_t piece;
          File::kbyte bw;
        };

        struct Spawner{
          bool is_filecomplete;
          File::kbyte upbw;
          File::kbyte dwbw;
          size_t nb_unchpeer;
          size_t nb_optunchpeer;
          bitfreq* on_bitfreq;
          onevent* on_entrance;
          onpiece* on_piececomplete;
          onpiece* on_piececancel;
          onevent* on_piecefetching;
          onevent* on_peerfetching;
          onunch* on_unchoking;
          onunch* on_optunchoking;
          onevent* on_filecomplete;
          Timeline<int>::timestamp rt_nextspawn;
        };

        EventData(const PeerJob& pj): pj(pj) {};
        EventData(const FileSharing& fs): fs(fs) {};
        EventData(const Spawner& sp): sp(sp) {};

        union{
          PeerJob pj;
          FileSharing fs;
          Spawner sp;
        };
      };

      class Network{
        /**Methods**/
        public:
          Network(size_t, size_t);

          /*Peer*/
          Peer::id addPeer(bool, File::kbyte, File::kbyte, size_t, size_t,
                           Peer::bitfreq&, Peer::onevent&, Peer::onpiece&,
                           Peer::onpiece&, Peer::onevent&, Peer::onevent&,
                           Peer::onunch&, Peer::onunch&, Peer::onevent&,
                           Schedule&, std::mt19937&);
          void removePeer(Peer::id);
          Peer& getPeer(Peer::id);
          size_t getNbPeer() const { return peers.size(); }
          bool isUp(Peer::id) const;
          std::vector<Peer::id> drawPeers(std::mt19937&) const;

        /**Attributes**/
        private:
          std::map<Peer::id, Peer> peers;
          Peer::id avai_id;
          size_t sz_file;
      };

    /**Statics**/
    public:
      static bool cmpId(Peer::id, Peer::id);

    /**Methods**/
    public:
      Peer();
      Peer(const File&, File::kbyte, File::kbyte, size_t, size_t, bitfreq&,
           onevent&, onpiece&, onpiece&, onevent&, onevent&, onunch&, onunch&,
           onevent&);

      File::kbyte getUpBandW() const;
      File::kbyte getDwBandW() const;
      File::kbyte getFreeUpBandW() const;
      File::kbyte getFreeDwBandW() const;
      void takeUpBandW(File::kbyte);
      void takeDwBandW(File::kbyte);
      size_t getFileSize() const { return file.getSize(); };
      bool isPieceComplete(size_t) const;
      bool isFileEmpty() const;
      bool isFileComplete() const;
      bool hasChoked(id) const;
      bool hasRegChoked(id) const;
      bool hasOptChoked(id) const;
      void mergePeers(std::list<Peer::id>&&);
      void purgePeers(const Network&);
      void addSubPiece(size_t);
      void fileDownload(size_t);
      void unfileDownload(size_t);
      bool isDownloading(size_t) const;
      size_t getNbDownload() const;
      void filePending(size_t);
      void unfilePending(size_t);
      bool isPending(size_t) const;
      void fileLeecher(Peer::id);
      void unfileLeecher(Peer::id);
      bool isLeechingTo(Peer::id) const;
      size_t getNbLeecher() const;
      std::list<size_t> getIncomplete(std::vector<uint8_t>) const;
      void setBehaviour(bitfreq&, onevent&, onpiece&, onpiece&, onevent&,
                        onevent&, onunch&, onunch&, onevent&);

      std::vector<uint8_t> requestBitf() const{
        return on_bitfrequest(file);
      }

      void onEntrance(id pid, Network& network, Schedule& schedule,
                      std::mt19937& rng)
      {
        return on_entrance(pid, l_peer, network, schedule, rng);
      }

      void onPieceComplete(id pid, size_t piece, Network& network,
                           Schedule& schedule, std::mt19937& rng)
      {
        return on_piececomplete(pid, l_peer, piece, network, schedule, rng);
      }

      void onPieceCancel(id pid, size_t piece, Network& network,
                         Schedule& schedule, std::mt19937& rng)
      {
        return on_piececancel(pid, l_peer, piece, network, schedule, rng);
      }

      void onPieceFetching(id pid, Network& network, Schedule& schedule,
                           std::mt19937& rng)
      {
        return on_piecefetching(pid, l_peer, network, schedule, rng);
      }

      void onPeerFetching(id pid, Network& network, Schedule& schedule,
                          std::mt19937& rng)
      {
        return on_peerfetching(pid, l_peer, network, schedule, rng);
      }

      void onUnchoking(id pid, Network& network, Schedule& schedule,
                       std::mt19937& rng)
      {
        unch_peers = std::list<Peer::id>(on_unchoking(pid, l_peer, network,
                                                      schedule, rng));
        for(auto p: optunch_peers){
          unch_peers.remove(p);
        }
        if(unch_peers.size() > nb_unchpeer){
          unch_peers.resize(nb_unchpeer);
        }
      }

      void onOptUnchoking(id pid, Network& network, Schedule& schedule,
                          std::mt19937& rng)
      {
        optunch_peers = std::list<Peer::id>(on_optunchoking(pid, l_peer,
                                                            network, schedule,
                                                            rng));
        for(auto p: unch_peers){
          optunch_peers.remove(p);
        }
        if(optunch_peers.size() > nb_optunchpeer){
          optunch_peers.resize(nb_optunchpeer);
        }
      }

      void onFileComplete(id pid, Network& network, Schedule& schedule,
                          std::mt19937& rng)
      {
        return on_filecomplete(pid, l_peer, network, schedule, rng);
      }

    /**Attributes**/
    private:
      File file;
      std::set<Peer::id> s_leecher;
      std::set<size_t> dw_pieces;
      std::set<size_t> pend_pieces;

      File::kbyte up_bandw;
      File::kbyte used_upbandw;
      File::kbyte dw_bandw;
      File::kbyte used_dwbandw;

      std::list<id> l_peer;
      std::list<id> unch_peers;
      std::list<id> optunch_peers;
      size_t nb_unchpeer;
      size_t nb_optunchpeer;

      bitfreq* on_bitfrequest;
      onevent* on_entrance;
      onpiece* on_piececomplete;
      onpiece* on_piececancel;
      onevent* on_piecefetching;
      onevent* on_peerfetching;
      onunch* on_unchoking;
      onunch* on_optunchoking;
      onevent* on_filecomplete;
  };
#endif
