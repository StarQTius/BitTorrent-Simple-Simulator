#include "bittorrent.hpp"

namespace BitTorrent{
  std::map<Peer::id, Stat> statistic;
  size_t nb_piece = 1000;
  Peer::Schedule::timestamp seeding_period = 900;
  Peer::Schedule::timestamp leecherarrival_period = 300;
  Peer::Schedule::timestamp freeriderarrival_period = 300;
  uint16_t nb_leecher = 0;
  uint16_t nb_totleecher = 0;
  uint16_t nb_seeder = 0;
  uint16_t nb_totseeder = 0;
  uint16_t nb_freerider = 0;
  uint16_t nb_totfreerider = 0;
  Peer::Schedule::timestamp t_seederpeak = 0;
  uint16_t v_seederpeak = 0;

  /*****************************************************************************
    Bitfield
  *****************************************************************************/
  std::vector<uint8_t> getRawBitf(const File& file){
    return file.getPieces();
  }

  std::vector<uint8_t> getEmptyBitf(const File& file){
    return std::vector<uint8_t>(file.getPieces().size(), 0);
  }

  /*****************************************************************************
    Request
  *****************************************************************************/
  bool rarestCmp(const std::pair<size_t, std::list<Peer::id>>& a,
                 const std::pair<size_t, std::list<Peer::id>>& b)
  {
    return a.second.size() < b.second.size();
  }

  /**
    Recherche les pièces les plus rares pouvant intéresser le pair désigné :
      -pid: identifiant du pair
      -m_piece: map associant chaque pièce à la liste de ceux qui la possèdent
      -network: réseau sur lequel on travaille
    +retourne une liste de couples pièce / pair ordonnée selon la rareté (un
     couple associe une pièce avec un pair la possèdant)
  **/
  std::list<std::pair<size_t, Peer::id>> requestRarest
    (Peer::id pid, QueryMap& m_piece, Peer::Network& network)
  {
    Peer& peer = network.getPeer(pid);

    std::vector<std::pair<size_t, std::list<Peer::id>>>
      v_piecepeer(m_piece.begin(), m_piece.end());

    std::sort(v_piecepeer.begin(), v_piecepeer.end(), rarestCmp);

    auto it = v_piecepeer.begin();
    while(it != v_piecepeer.end()){
      if(it->second.size() > 0){ break; }
      it++;
    }

    std::list<std::pair<size_t, Peer::id>> l_piecepeer;
    for(; it != v_piecepeer.end(); it++){
      if(!peer.isPending(it->first)){
        l_piecepeer.emplace_front(it->first, it->second.front());
      }
    }

    return l_piecepeer;
  }

  /**
    Recherche les pièces partiellement acquises par le pair désigné:
      -pid: identifiant du pair
      -m_piece: map associant chaque pièce à la liste de ceux qui la possèdent
      -network: réseau sur lequel on travaille
    +retourne une liste de couples pièce / pair (un couple associe une pièce
     avec un pair la possèdant)
  **/
  std::list<std::pair<size_t, Peer::id>> requestPending
    (Peer::id pid, QueryMap& m_piece, Peer::Network& network)
  {
    std::list<std::pair<size_t, Peer::id>> l_piecepeer;

    Peer& peer = network.getPeer(pid);
    for(auto piece_lpeer: m_piece){
      if(piece_lpeer.second.size() > 0
        && peer.isPending(piece_lpeer.first)
        && !peer.isDownloading(piece_lpeer.first))
      {
        l_piecepeer.emplace_front(piece_lpeer.first,
                                  piece_lpeer.second.front());
      }
    }

    return l_piecepeer;
  }


  /**
    Recherche des pièces aléatoirement:
      -m_piece: map associant chaque pièce à la liste de ceux qui la possèdent
      -network: réseau sur lequel on travaille
    +retourne une liste de couples pièce / pair (un couple associe une pièce
     avec un pair la possèdant)
  **/
  std::list<std::pair<size_t, Peer::id>> requestRandom
    (Peer& peer, QueryMap& m_piece, Peer::Network& network, std::mt19937& rng)
  {
    std::vector<std::pair<size_t, std::list<Peer::id>>>
      v_piecepeer(m_piece.begin(), m_piece.end());

    std::sort(v_piecepeer.begin(), v_piecepeer.end(), rarestCmp);

    auto it = v_piecepeer.begin();
    while(it != v_piecepeer.end()){
      if(it->second.size() > 0){
        break;
      }
      it++;
    }

    std::shuffle(it, v_piecepeer.end(), rng);

    std::list<std::pair<size_t, Peer::id>> l_piecepeer;
    for(; it != v_piecepeer.end(); it++){
      if(!peer.isPending(it->first)){
        l_piecepeer.emplace_front(it->first, it->second.front());
      }
    }

    return l_piecepeer;
  }

  /*****************************************************************************
    Unchoking
  *****************************************************************************/
  std::list<Peer::id> unchokeFirst(Peer::id pid,
                                   const std::list<Peer::id>& l_peer,
                                   Peer::Network& network, std::mt19937& rng)
  {
    return l_peer;
  }

  class CmpUpBw{
    public:
      CmpUpBw(Peer::Network& network) : network(&network) {}
      bool operator()(Peer::id lhs, Peer::id rhs){
        Peer& lhs_peer = network->getPeer(lhs);
        Peer& rhs_peer = network->getPeer(rhs);
        return lhs_peer.getUpBandW() / (lhs_peer.getNbLeecher() + 1)
               > rhs_peer.getUpBandW() / (rhs_peer.getNbLeecher() + 1);
      }
    private:
      Peer::Network* network;
  };
  /**
    Classe le voisinage d'un pair selon les débits ascendants qu'ils proposent :
      -pid: identifiant du pair
      -l_peer: voisinage du pair
      -network: réseau sur lequel sur lequel on travaille
      -rng: générateur de nombre (pseudo)-aléatoires
    +retourne une liste ordonnée d'identifiants de pair
  **/
  std::list<Peer::id> unchokeBestUp(Peer::id pid,
                                    const std::list<Peer::id>& l_peer,
                                    Peer::Network& network,
                                    std::mt19937& rng)
  {
    Peer& peer = network.getPeer(pid);
    std::vector<Peer::id> v_intpeer;
    v_intpeer.reserve(l_peer.size());

    for(auto p: l_peer){
      try{
        Peer& rpeer = network.getPeer(p);
        if(!peer.getIncomplete(rpeer.requestBitf()).empty()
          && !rpeer.hasChoked(pid)){
          v_intpeer.push_back(p);
        }
      }
      catch(...) {}
    }

    std::sort(v_intpeer.begin(), v_intpeer.end(), CmpUpBw(network));

    return std::list<Peer::id>(v_intpeer.begin(), v_intpeer.end());
  }

  class CmpDwBw{
    public:
      CmpDwBw(Peer::Network& network) : network(&network) {}
      bool operator()(Peer::id lhs, Peer::id rhs){
        return network->getPeer(lhs).getFreeDwBandW()
               < network->getPeer(rhs).getFreeDwBandW();
      }
    private:
      Peer::Network* network;
  };
  /**
    Classe le voisinage d'un pair selon les débits descendants qu'ils proposent:
      -pid: identifiant du pair
      -l_peer: voisinage du pair
      -network: réseau sur lequel sur lequel on travaille
      -rng: générateur de nombre (pseudo)-aléatoires
    +retourne une liste ordonnée d'identifiants de pair
  **/
  std::list<Peer::id> unchokeBestDw(Peer::id pid,
                                  const std::list<Peer::id>& l_peer,
                                  Peer::Network& network,
                                  std::mt19937& rng)
  {
    Peer& peer = network.getPeer(pid);
    std::vector<Peer::id> v_intpeer;
    v_intpeer.reserve(l_peer.size());

    for(auto p: l_peer){
      try{
        if(!network.getPeer(p).getIncomplete(peer.requestBitf()).empty()){
          v_intpeer.push_back(p);
        }
      }
      catch(...) {}
    }

    std::sort(v_intpeer.begin(), v_intpeer.end(), CmpDwBw(network));
    return std::list<Peer::id>(v_intpeer.begin(), v_intpeer.end());
  }

  std::list<Peer::id> unchokeRandom(Peer::id pid,
                                    const std::list<Peer::id>& l_peer,
                                    Peer::Network& network,
                                    std::mt19937& rng)
  {
    std::vector<Peer::id> tmp(l_peer.begin(), l_peer.end());
    std::shuffle(tmp.begin(), tmp.end(), rng);
    return std::list<Peer::id>(tmp.begin(), tmp.end());
  }

  /*****************************************************************************
    Event
  *****************************************************************************/
  /**
    Vérifie si le téléchargement s'est bien déroulé, ajoute une sous-pièce à la
    pièce désignée et planifie les événements suivant selon le contexte :
      -network: réseau sur lequel on travaille
      -rng: générateur de nombres (pseudo)-aléatoires
      -schedule: ligne de temps des événements
      -edata: données de l'événement
  **/
  void downloadPiece(Peer::Network& network,
                     std::mt19937& rng,
                     Peer::Schedule& schedule,
                     Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;

    if(network.isUp(fs.dwpeer)){ //Pair téléchargeant connecté
      Peer& dwpeer = network.getPeer(fs.dwpeer);

      if(network.isUp(fs.uppeer)){ //Pair téléversant connecté
        Peer& uppeer = network.getPeer(fs.uppeer);

        if(!uppeer.hasChoked(fs.dwpeer)){ //Pair téléversant disponible
          /*Téléchargement*/
          dwpeer.addSubPiece(fs.piece);

          if(!uppeer.hasRegChoked(fs.dwpeer)){ //"Regular unchoking"
            statistic[fs.dwpeer].nb_subpiecebyunch++;
          }
          if(!uppeer.hasOptChoked(fs.dwpeer)){ //"Optimistic unchoking"
            statistic[fs.dwpeer].nb_subpiecebyoptunch++;
            statistic[fs.dwpeer].nb_subpiecebyopt[fs.piece]++;
          }

          if(statistic[fs.uppeer].type == BitTorrent::Stat::Type::SEEDER){
            statistic[fs.dwpeer].nb_subpiecefromseed++;
          }

          if(!dwpeer.isPieceComplete(fs.piece)){ //Piece incomplète
            /*Pipelining*/
            createDownload(fs.dwpeer, fs.uppeer, fs.piece, network, schedule);
          }
          else if(dwpeer.isFileComplete()){ //Fichier complet
            /*Téléchargement terminé*/
            uppeer.unfileLeecher(fs.dwpeer);
            dwpeer.onPieceComplete(fs.dwpeer, fs.piece, network, schedule, rng);
            dwpeer.onFileComplete(fs.dwpeer, network, schedule, rng);
          }
          else{ //Pièce complète
            uppeer.unfileLeecher(fs.dwpeer);
            dwpeer.onPieceComplete(fs.dwpeer, fs.piece, network, schedule, rng);
          }
        }
        else{ //Pair téléversant indisponible
          uppeer.unfileLeecher(fs.dwpeer);
          dwpeer.onPieceCancel(fs.dwpeer, fs.piece, network, schedule, rng);
        }
        statistic[fs.uppeer].l_tnbleecher.emplace_back
          (schedule.getDate(), uppeer.getNbLeecher());
      }
      else{ //Pair téléversant déconnecté
        dwpeer.onPieceCancel(fs.dwpeer, fs.piece, network, schedule, rng);
      }
    }
  }

  void makeFetchPieces(Peer::Network& network, std::mt19937& rng,
                       Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;
    if(network.isUp(fs.dwpeer)){
      network.getPeer(fs.dwpeer).onPieceFetching(fs.dwpeer, network, schedule,
                                                 rng);
    }
  }

  void makeFetchPeers(Peer::Network& network, std::mt19937& rng,
                      Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;
    if(network.isUp(fs.dwpeer)){
      network.getPeer(fs.dwpeer).onPeerFetching(fs.dwpeer, network, schedule,
                                                rng);
    }
  }

  /**
    Lance une recherche de pairs sans relance de l'événement :
      -network: réseau sur lequel on travaille
      -rng: générateur de nombres (pseudo)-aléatoires
      -schedule: ligne de temps des événements
      -edata: données de l'événement
  **/
  void makeFetchPeersOnce(Peer::Network& network, std::mt19937& rng,
                          Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::PeerJob pj = edata.pj;
    if(!network.isUp(pj.peer)){
      return;
    }
    std::cout << reportState(network, schedule)
              << "Peer " << pj.peer << " is fetching peers" << std::endl;
    Peer& peer = network.getPeer(pj.peer);

    /*Purge des pairs déconnectés*/
    peer.purgePeers(network);

    /*Tirage d'une liste de pairs à mettre en relation avec le pair*/
    std::vector<Peer::id> v_freshpeer = network.drawPeers(rng);
    v_freshpeer.erase(std::remove
      (v_freshpeer.begin(), v_freshpeer.end(), pj.peer));
    for(auto p: v_freshpeer){
      network.getPeer(p).mergePeers({pj.peer});
    }
    peer.mergePeers(std::list<Peer::id>(v_freshpeer.begin(),
                                        v_freshpeer.end()));
  }

  void addPeer(Peer::Network& network, std::mt19937& rng,
               Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::Spawner sp = edata.sp;

    network.addPeer(sp.is_filecomplete, sp.upbw, sp.dwbw, sp.nb_unchpeer,
                    sp.nb_optunchpeer, *sp.on_bitfreq, *sp.on_entrance,
                    *sp.on_piececomplete, *sp.on_piececancel,
                    *sp.on_piecefetching, *sp.on_peerfetching, *sp.on_unchoking,
                    *sp.on_optunchoking, *sp.on_filecomplete, schedule, rng);
    std::uniform_real_distribution<double> distr;
    schedule.pushEvent(schedule.getDate()
                      - sp.rt_nextspawn * log(1 - distr(rng)),
                      sp, addPeer);
  }

  void removePeer(Peer::Network& network, std::mt19937& rng,
                  Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;
    if(network.isUp(fs.dwpeer)){
      network.removePeer(fs.dwpeer);
      statistic[fs.dwpeer].t_leave = schedule.getDate();
    }
  }

  void makeUnchoke(Peer::Network& network, std::mt19937& rng,
                   Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;
    if(network.isUp(fs.dwpeer)){
      network.getPeer(fs.dwpeer).onUnchoking(fs.dwpeer, network, schedule, rng);
    }
  }

  void makeOptUnchoke(Peer::Network& network, std::mt19937& rng,
                      Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    Peer::EventData::FileSharing fs = edata.fs;
    if(network.isUp(fs.dwpeer)){
      network.getPeer(fs.dwpeer).onOptUnchoking(fs.dwpeer, network, schedule,
                                                rng);
    }
  }

  void decrementLeecher(Peer::Network& network, std::mt19937& rng,
                        Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    nb_leecher--;
  }

  void decrementSeeder(Peer::Network& network, std::mt19937& rng,
                       Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    nb_seeder--;
  }

  void decrementFreerider(Peer::Network& network, std::mt19937& rng,
                          Peer::Schedule& schedule, Peer::EventData&& edata)
  {
    nb_freerider--;
  }

  /*****************************************************************************
    Management
  *****************************************************************************/
  /**
    Planifie le téléchargement d'une pièce:
      -dwpid: identifiant du pair téléchargeant
      -uppid: identifiant du pair téléversant
      -piece: pièce qui sera téléchargée
      -network: réseau sur lequel on travaille
      -schedule: ligne de temps des événements sur laquelle on travaille
  **/
  void createDownload(Peer::id dwpid, Peer::id uppid, size_t piece,
                      Peer::Network& network, Peer::Schedule& schedule)
  {
    Peer& uppeer = network.getPeer(uppid);

    std::cout << reportState(network, schedule)
              << "Peer " << dwpid
              << " is downloading piece " << piece
              << " from peer " << uppid
              << std::endl;

    uppeer.fileLeecher(dwpid);
    File::kbyte bw = uppeer.getUpBandW() / uppeer.getNbLeecher();

    Peer::EventData::FileSharing fs;
    fs.dwpeer = dwpid;
    fs.uppeer = uppid;
    fs.piece = piece;
    fs.bw = bw;
    schedule.pushEvent(schedule.getDate() + File::sz_subpiece / bw,
                      fs, downloadPiece);
    statistic[uppid].l_tnbleecher.emplace_back
      (schedule.getDate(), uppeer.getNbLeecher());
  }

  /**
    Lance le téléchargement de plusieurs pièces jusqu'à saturation de la
    bande passante ou de l'épuisement des pièces disponibles :
      -dwpid: identifiant du pair téléchargeant
      -l_piecepeer: liste de couple (pièce / pair possédant la pièce)
      -piece: pièce qui sera téléchargée
      -network: réseau sur lequel on travaille
      -schedule: ligne de temps des événements sur laquelle on travaille
    +retourne la liste de pièces dont le téléchargement a été planifié
  **/
  std::list<size_t> createMostDownload
    (Peer::id dwpid, const std::list<std::pair<size_t, Peer::id>>& l_piecepeer,
     Peer::Network& network, Peer::Schedule& schedule)
  {
    std::list<size_t> l_dwpiece;

    auto it = l_piecepeer.begin();
    for(; it != l_piecepeer.end(); it++)
    {
      Peer& uppeer = network.getPeer(it->second);
      if(!uppeer.isLeechingTo(dwpid)
        && uppeer.getNbLeecher() < nb_unchpeer + nb_optunchpeer){
        createDownload(dwpid, it->second, it->first, network, schedule);
        l_dwpiece.push_back(it->first);
      }
    }

    return l_dwpiece;
  }

  /**
    Recherche les voisins disponibles pour un pair possédant des pièces
    intéréssantes:
      -pid: identifiant du pair
      -l_peer: voisinage du pair
      -network: réseau sur lequel on travaille
    +retourne une map associant chaque pièce à la liste des voisins disponibles
     la possédant
  **/
  QueryMap getMapPiece(Peer::id pid, const std::list<Peer::id>& l_peer,
                       Peer::Network& network)
  {
    Peer& peer = network.getPeer(pid);
    QueryMap m_pieces;

    for(auto p: l_peer){
      try{
        Peer& rpeer = network.getPeer(p);

        if(!rpeer.hasChoked(pid)){
          std::list<size_t> l_piece = peer.getIncomplete(rpeer.requestBitf());
          for(auto piece: l_piece){
            m_pieces[piece].push_front(p);
          }
        }
      }
      catch(...) {}
    }

    return m_pieces;
  }

  void turnIntoSeeder(Peer& peer){
    peer.setBehaviour
     (BitTorrent::Seeder::onBitfRequest,
      BitTorrent::Seeder::onEntrance,
      BitTorrent::Seeder::onPieceComplete,
      BitTorrent::Seeder::onPieceCancel,
      BitTorrent::Seeder::onPieceFetching,
      BitTorrent::Seeder::onPeerFetching,
      BitTorrent::Seeder::onUnchoking,
      BitTorrent::Seeder::onOptUnchoking,
      BitTorrent::Seeder::onFileComplete);
  }

  std::string reportState
    (const Peer::Network& network, const Peer::Schedule& schedule)
  {
    return std::string() +
      "[ t = " + std::to_string(schedule.getDate())
      + " np = "  + std::to_string(network.getNbPeer())
      + " nl = "  + std::to_string(nb_leecher)
      + " ns = "  + std::to_string(nb_seeder)
      + " nfr = " + std::to_string(nb_freerider)
      + "] - ";
  }

  namespace Leecher{
    /**
      Retourne exactement le bit field donné en argument:
      NB : il s'agit d'un alias
    **/
    const Peer::bitfreq& onBitfRequest = getRawBitf;

    /**
      Met en place le pair et ses routines dans le réseau:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onEntrance(Peer::id pid, const std::list<Peer::id>& l_peer,
                    Peer::Network& network, Peer::Schedule& schedule,
                    std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Peer " << pid <<
                " entered the network" << std::endl;
      statistic[pid].type = Stat::Type::LEECHER;
      statistic[pid].t_entry = schedule.getDate();
      statistic[pid].t_obtpiece.resize(nb_piece);
      statistic[pid].nb_subpiecebyopt.resize(nb_piece);
      statistic[pid].l_tnbleecher.emplace_back(schedule.getDate(), 0);
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(), 0);
      statistic[pid].nb_subpiecerandom.resize(nb_piece);

      /*Mise en place les routines du pair*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate(), fs,
                        makeFetchPeers);
      schedule.pushEvent(schedule.getDate() + 1, fs,
                        makeUnchoke);
      schedule.pushEvent(schedule.getDate() + 1, fs,
                        makeOptUnchoke);
      schedule.pushEvent(schedule.getDate() + 2, fs,
                        makeFetchPieces);
      nb_leecher++;
      nb_totleecher++;
    }

    /**
      Réalise les actions du pair lors de la complétion d'une pièce
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -piece : index de la pièce téléchargée
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPieceComplete
      (Peer::id pid, const std::list<Peer::id>& l_peer, size_t piece,
       Peer::Network& network, Peer::Schedule& schedule, std::mt19937& rng)
    {
      Peer& peer = network.getPeer(pid);

      /*Désenregistrement de la pièce*/
      peer.unfileDownload(piece);
      peer.unfilePending(piece);
      std::cout << reportState(network, schedule)
                << "Peer " << pid << " downloaded piece " << piece << std::endl;
      statistic[pid].t_obtpiece[piece] = schedule.getDate();
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                peer.getNbDownload());
    }

    /**
      Réalise les actions du pair lors de l'annulation du téléchargement d'une
      pièce:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -piece : index de la pièce téléchargée
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPieceCancel(Peer::id pid, const std::list<Peer::id>& l_peer,
                       size_t piece, Peer::Network& network,
                       Peer::Schedule& schedule, std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Download of piece " << piece
                << " to peer " << pid
                << " was canceled" << std::endl;
      Peer& peer = network.getPeer(pid);

      QueryMap m_piece = getMapPiece(pid, l_peer, network);

      if(m_piece.find(piece) != m_piece.end()){ //Pièce trouvée ailleurs
        /*Poursuite du téléchargement*/
        createDownload(pid, m_piece[piece].front(), piece, network, schedule);
      }
      else{
        peer.unfileDownload(piece);
        statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                  peer.getNbDownload());
      }
    }

    /**
      Recherche des pièces à télécharger et planifie leur téléchargement:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPieceFetching(Peer::id pid, const std::list<Peer::id>& l_peer,
                         Peer::Network& network, Peer::Schedule& schedule,
                         std::mt19937& rng)
    {
      Peer& peer = network.getPeer(pid);
      QueryMap m_piece = getMapPiece(pid, l_peer, network);
      if(m_piece.empty()){
        std::cout << reportState(network, schedule)
                  << "Peer " << pid << " did not find any piece" << std::endl;
      }
      else{
        std::cout << reportState(network, schedule)
                  << "Peer " << pid << " found piece(s)" << std::endl;
      }

      /*Téléchargement des pièces dont le pair possède déjà des sous-pièces
        (strict policy) et enregistrement*/
      auto l_piecependpeer = requestPending(pid, m_piece, network);
      std::list<size_t> l_piecepend = createMostDownload(pid, l_piecependpeer,
                                                         network, schedule);
      for(auto piece: l_piecepend){
        peer.fileDownload(piece);
      }
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                peer.getNbDownload());

      std::list<std::pair<size_t, Peer::id>> l_piecepeer;

      bool is_random = false;
      if(peer.isFileEmpty()){ //Aucune pièce complète
        /*Sélection aléatoire de pièce (random first)*/
        l_piecepeer = requestRandom(peer, m_piece, network, rng);
        is_random = true;
      }
      else{ //Au moins une pièce complète
        /*Sélection des pièces les plus rares (rarest first)*/
        l_piecepeer = requestRarest(pid, m_piece, network);
      }

      /*Téléchargement des pièces sélectionnées et enregistrement*/
      std::list<size_t> l_piece = createMostDownload(pid, l_piecepeer,
                                                     network, schedule);
      for(auto piece: l_piece){
        peer.fileDownload(piece);
        peer.filePending(piece);
        if(is_random){
          statistic[pid].nb_subpiecerandom[piece]++;
        }
      }
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                peer.getNbDownload());

      /*Planification de la prochaine recherche*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + unch_period, fs,
                         makeFetchPieces);
    }

    /**
      Recherche des pairs dans le réseau:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPeerFetching(Peer::id pid, const std::list<Peer::id>& l_peer,
                        Peer::Network& network, Peer::Schedule& schedule,
                        std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Peer " << pid << " is fetching peers" << std::endl;
      Peer& peer = network.getPeer(pid);

      /*Purge des pairs déconnectés*/
      peer.purgePeers(network);

      /*Tirage d'une liste de pairs à mettre en relation avec le pair*/
      std::vector<Peer::id> v_freshpeer = network.drawPeers(rng);
      v_freshpeer.erase(std::remove
        (v_freshpeer.begin(), v_freshpeer.end(), pid));
      for(auto p: v_freshpeer){
        network.getPeer(p).mergePeers({pid});
      }
      peer.mergePeers(std::list<Peer::id>(v_freshpeer.begin(),
                                          v_freshpeer.end()));

      /*Planification de la prochaine recherche*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + annoucement_period, fs,
                        makeFetchPieces);
    }

    /**
      Réalise la procédure d'"unchoking" du pair:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
      +retourne une liste de pair trié dans un ordre de préférence
    **/
    std::list<Peer::id> onUnchoking(Peer::id pid,
                                    const std::list<Peer::id>& l_peer,
                                    Peer::Network& network,
                                    Peer::Schedule& schedule, std::mt19937& rng)
    {
      /*Planification du prochain unchoking*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + unch_period, fs,
                        makeUnchoke);

      /*Retour de la liste ordonnée des meilleurs pairs selon leur débit
        montant*/
      std::list<Peer::id> l_pid = unchokeBestUp(pid, l_peer, network, rng);

      return l_pid;
    }

    /**
      Réalise la procédure d'"optimistic unchoking" du pair:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
      +retourne une liste de pair trié dans un ordre de préférence
    **/
    std::list<Peer::id> onOptUnchoking(Peer::id pid,
                                       const std::list<Peer::id>& l_peer,
                                       Peer::Network& network,
                                       Peer::Schedule& schedule,
                                       std::mt19937& rng)
    {
      /*Planification du prochain optimistic unchoking*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + optunch_period, fs,
                        makeOptUnchoke);

      /*Retour d'une liste aléatoire de pair*/
      return unchokeRandom(pid, l_peer, network, rng);
    }

    /**
      Réalise les actions du pair lors de la complétion du fichier
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onFileComplete(Peer::id pid, const std::list<Peer::id>& l_peer,
                        Peer::Network& network, Peer::Schedule& schedule,
                        std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Peer " << pid << " is now seeding" << std::endl;
      statistic[pid].t_completed = schedule.getDate();

      /*Transformation du pair en seeder*/
      turnIntoSeeder(network.getPeer(pid));
      nb_leecher--; nb_seeder++;
      if(v_seederpeak < nb_seeder){
        v_seederpeak = nb_seeder;
        t_seederpeak = schedule.getDate();
      }

      /*Planification du départ du pair*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      std::uniform_real_distribution<double> distr;
      Peer::Schedule::timestamp period = -seeding_period * log(1 - distr(rng));
      schedule.pushEvent(schedule.getDate() + period, fs, removePeer);
      schedule.pushEvent(schedule.getDate() + period, fs, decrementSeeder);
    }
  };

  namespace Seeder{
    /**
      Retourne exactement le bit field donné en argument:
      NB : il s'agit d'un alias
    **/
    const Peer::bitfreq& onBitfRequest = getRawBitf;

    /**
      Met en place le pair et ses routines dans le réseau:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onEntrance(Peer::id pid, const std::list<Peer::id>& l_peer,
                    Peer::Network& network, Peer::Schedule& schedule,
                    std::mt19937& rng)
    {
        std::cout << reportState(network, schedule)
                  << "Peer " << pid
                  << " entered the network" << std::endl;
        statistic[pid].type = Stat::Type::SEEDER;
        statistic[pid].t_entry = schedule.getDate();
        statistic[pid].t_obtpiece.resize(nb_piece);
        statistic[pid].nb_subpiecebyopt.resize(nb_piece);
        statistic[pid].l_tnbleecher.emplace_back(schedule.getDate(), 0);
        statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(), 0);
        statistic[pid].nb_subpiecerandom.resize(nb_piece);

        Peer& peer = network.getPeer(pid);

        /*Tirage d'une liste de pairs à mettre en relation avec le pair*/
        std::vector<Peer::id> v_freshpeer = network.drawPeers(rng);
        v_freshpeer.erase(std::remove
          (v_freshpeer.begin(), v_freshpeer.end(), pid));
        for(auto p: v_freshpeer){
          network.getPeer(p).mergePeers({pid});
        }
        peer.mergePeers(std::list<Peer::id>(v_freshpeer.begin(),
                                            v_freshpeer.end()));

        /*Mise en place les routines du pair*/
        Peer::EventData::FileSharing fs;
        fs.dwpeer = pid;
        schedule.pushEvent(schedule.getDate(), fs,
                           makeUnchoke);
        schedule.pushEvent(schedule.getDate(), fs,
                           makeOptUnchoke);
        nb_seeder++;
        nb_totseeder++;
        if(v_seederpeak < nb_seeder){
          v_seederpeak = nb_seeder;
          t_seederpeak = schedule.getDate();
        }
      }

    /**
      Réalise les actions du pair lors de la complétion d'une pièce:
      NB : il s'agit d'un alias
    **/
    const Peer::onpiece& onPieceComplete = onpiece_nop;

    /**
      Réalise les actions du pair lors de l'annulation du téléchargement d'une
      pièce:
      NB : il s'agit d'un alias
    **/
    const Peer::onpiece& onPieceCancel = onpiece_nop;

    /**
      Recherche des pièces à télécharger et planifie leur téléchargement:
      NB : il s'agit d'un alias
    **/
    const Peer::onevent& onPieceFetching = onevent_nop;

    /**
      Recherche des pairs dans le réseau:
      NB : il s'agit d'un alias
    **/
    const Peer::onevent& onPeerFetching = onevent_nop;

    /**
      Réalise la procédure d'"unchoking" du pair:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
      +retourne une liste de pair trié dans un ordre de préférence
    **/
    std::list<Peer::id> onUnchoking(Peer::id pid,
                                    const std::list<Peer::id>& l_peer,
                                    Peer::Network& network,
                                    Peer::Schedule& schedule,
                                    std::mt19937& rng)
    {
      Peer& peer = network.getPeer(pid);

      /*Planification du prochain unchoking*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + unch_period, fs, makeUnchoke);

      /*Purge de la liste de pair*/
      std::cout << reportState(network, schedule)
                << "Seed " << pid << " is purging" << std::endl;
      peer.purgePeers(network);

      /*Retour de la liste ordonnée des meilleurs pairs selon leur débit
        descendant*/
      std::list<Peer::id> l_pid = unchokeRandom(pid, l_peer, network, rng);

      return l_pid;
    }

    /**
      Réalise la procédure d'"optimistic unchoking" du pair:
      NB : il s'agit d'un alias
    **/
    const Peer::onunch& onOptUnchoking = Leecher::onOptUnchoking;

    /**
      Réalise les actions du pair lors de la complétion du fichier
      NB : il s'agit d'un alias
    **/
    const Peer::onevent& onFileComplete = onevent_nop;
  };

  namespace FreeRider{
    Peer::Schedule::timestamp annoucement_period = 30;
    float annoucement_period_increase = 1.5;

    /**
      Retourne exactement le bit field donné en argument:
      NB : il s'agit d'un alias
    **/
    const Peer::bitfreq& onBitfRequest = getEmptyBitf;

    /**
      Met en place le pair et ses routines dans le réseau:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onEntrance(Peer::id pid, const std::list<Peer::id>& l_peer,
                    Peer::Network& network, Peer::Schedule& schedule,
                    std::mt19937& rng)
    {
        std::cout << reportState(network, schedule)
                  << "Peer " << pid
                  << " entered the network" << std::endl;
        statistic[pid].type = Stat::Type::FREERIDER;
        statistic[pid].t_entry = schedule.getDate();
        statistic[pid].t_obtpiece.resize(nb_piece);
        statistic[pid].nb_subpiecebyopt.resize(nb_piece);
        statistic[pid].l_tnbleecher.emplace_back(schedule.getDate(), 0);
        statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(), 0);
        statistic[pid].nb_subpiecerandom.resize(nb_piece);

        /*Mise en place des routines du pair*/
        Peer::EventData::FileSharing fs;
        fs.dwpeer = pid;
        schedule.pushEvent(schedule.getDate(), fs, makeFetchPeers);
        schedule.pushEvent(schedule.getDate() + 1, fs, makeFetchPieces);
        nb_freerider++;
        nb_totfreerider++;

        /*Prévision des premières recherches*/
        Peer::Schedule::timestamp u = annoucement_period;
        Peer::Schedule::timestamp s = 0;
        Peer::EventData::PeerJob pj;
        pj.peer = pid;
        while(u < BitTorrent::annoucement_period){
          s += u;
          schedule.pushEvent(schedule.getDate() + s, pj, makeFetchPeersOnce);
          u *= 1.5;
        }
      }

    /**
      Réalise les actions du pair lors de la complétion d'une pièce
      NB : il s'agit d'un alias
    **/
    const Peer::onpiece& onPieceComplete = BitTorrent::Leecher::onPieceComplete;

    /**
      Réalise les actions du pair lors de l'annulation du téléchargement d'une
      pièce
      NB : il s'agit d'un alias
    **/
    const Peer::onpiece& onPieceCancel = BitTorrent::Leecher::onPieceCancel;

    /**
      Recherche des pièces à télécharger et planifie leur téléchargement:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPieceFetching(Peer::id pid, const std::list<Peer::id>& l_peer,
                         Peer::Network& network, Peer::Schedule& schedule,
                         std::mt19937& rng)
    {
      Peer& peer = network.getPeer(pid);
      QueryMap m_piece = getMapPiece(pid, l_peer, network);
      if(m_piece.empty()){
        std::cout << reportState(network, schedule)
                  << "Peer " << pid << " did not find any piece" << std::endl;
      }
      else{
        std::cout << reportState(network, schedule)
                  << "Peer " << pid << " found piece(s)" << std::endl;
      }

      /*Téléchargement des pièces dont le pair possède déjà des sous-pièces
        (strict policy) et enregistrement*/
      auto l_piecependpeer = requestPending(pid, m_piece, network);
      std::list<size_t> l_piecepend = createMostDownload(pid, l_piecependpeer,
                                                         network, schedule);
      for(auto piece: l_piecepend){
        peer.fileDownload(piece);
      }
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                peer.getNbDownload());

      /*Sélection aléatoire de pièce (comportement de BitThief)*/
      std::list<std::pair<size_t, Peer::id>> l_piecepeer =
        requestRandom(peer, m_piece, network, rng);

      /*Téléchargement des pièces sélectionnées et enregistrement*/
      std::list<size_t> l_piece = createMostDownload(pid, l_piecepeer, network,
                                                     schedule);
      for(auto piece: l_piece){
        peer.fileDownload(piece);
        peer.filePending(piece);
        statistic[pid].nb_subpiecerandom[piece]++;
      }
      statistic[pid].l_tnbdownload.emplace_back(schedule.getDate(),
                                                peer.getNbDownload());

      /*Planification de la prochaine recherche*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + unch_period, fs, makeFetchPieces);
    }

    /**
      Recherche des pairs dans le réseau:
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onPeerFetching(Peer::id pid, const std::list<Peer::id>& l_peer,
                        Peer::Network& network, Peer::Schedule& schedule,
                        std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Peer " << pid << " is fetching peers" << std::endl;
      Peer& peer = network.getPeer(pid);

      /*Purge des pairs déconnectés*/
      peer.purgePeers(network);

      /*Tirage d'une liste de pairs à mettre en relation avec le pair*/
      std::vector<Peer::id> v_freshpeer = network.drawPeers(rng);
      v_freshpeer.erase(std::remove
        (v_freshpeer.begin(), v_freshpeer.end(), pid));
      for(auto p: v_freshpeer){
        network.getPeer(p).mergePeers({pid});
      }
      peer.mergePeers(std::list<Peer::id>(v_freshpeer.begin(),
                                          v_freshpeer.end()));

      /*Planification de la prochaine recherche*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate() + annoucement_period, fs,
                         makeFetchPieces);
    }

    /**
      Réalise la procédure d'"unchoking" du pair
      NB : il s'agit d'un alias
    **/
    const Peer::onunch& onUnchoking = onunch_nop;

    /**
      Réalise la procédure d'"optimistic unchoking" du pair
      NB : il s'agit d'un alias
    **/
    const Peer::onunch& onOptUnchoking = onunch_nop;

    /**
      Réalise les actions du pair lors de la complétion du fichier
        -pid : identifiant du pair
        -l_peer : liste des voisins du pair
        -network : réseau auquel appartient le pair
        -rng : générateur de nombre aléatoire
    **/
    void onFileComplete(Peer::id pid, const std::list<Peer::id>& l_peer,
                        Peer::Network& network, Peer::Schedule& schedule,
                        std::mt19937& rng)
    {
      std::cout << reportState(network, schedule)
                << "Peer " << pid
                << " left the network" << std::endl;
      statistic[pid].t_completed = schedule.getDate();

      /*Planification du départ du pair*/
      Peer::EventData::FileSharing fs;
      fs.dwpeer = pid;
      schedule.pushEvent(schedule.getDate(), fs, removePeer);
      nb_freerider--;
    }
  };
};
