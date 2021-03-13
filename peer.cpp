#include "peer.hpp"

/*******************************************************************************
  Peer - Static
*******************************************************************************/

bool Peer::cmpId(Peer::id a, Peer::id b){
  return a < b;
}

/*******************************************************************************
  Peer - Methods
*******************************************************************************/

Peer::Peer(){
  up_bandw = 0;
  used_upbandw = 0;
  dw_bandw = 0;
  used_dwbandw = 0;
}

Peer::Peer(const File& file, File::kbyte up_bandw,
           File::kbyte dw_bandw, size_t nb_unchpeer, size_t nb_optunchpeer,
           Peer::bitfreq& on_bitfrequest, Peer::onevent& on_entrance,
           Peer::onpiece& on_piececomplete, Peer::onpiece& on_piececancel,
           Peer::onevent& on_piecefetching, Peer::onevent& on_peerfetching,
           Peer::onunch& on_unchoking, Peer::onunch& on_optunchoking,
           Peer::onevent& on_filecomplete):
  file(file), up_bandw(up_bandw), used_upbandw(0), dw_bandw(dw_bandw),
  used_dwbandw(0), nb_unchpeer(nb_unchpeer), nb_optunchpeer(nb_optunchpeer),
  on_bitfrequest(&on_bitfrequest), on_entrance(&on_entrance),
  on_piececomplete(&on_piececomplete), on_piececancel(&on_piececancel),
  on_piecefetching(&on_piecefetching), on_peerfetching(&on_peerfetching),
  on_unchoking(&on_unchoking), on_optunchoking(&on_optunchoking),
  on_filecomplete(&on_filecomplete) {}

  File::kbyte Peer::getUpBandW() const{
    return up_bandw;
  }

  File::kbyte Peer::getDwBandW() const{
    return dw_bandw;
  }

File::kbyte Peer::getFreeUpBandW() const{
  return up_bandw - used_upbandw;
}

File::kbyte Peer::getFreeDwBandW() const{
  return dw_bandw - used_dwbandw;
}

void Peer::takeUpBandW(File::kbyte bw){
  if(used_upbandw + bw <= up_bandw && 0 <= used_upbandw + bw){
    used_upbandw += bw;
  }
}

void Peer::takeDwBandW(File::kbyte bw){
  if(used_dwbandw + bw <= dw_bandw && 0 <= used_dwbandw + bw){
    used_dwbandw += bw;
  }
}

bool Peer::isPieceComplete(size_t piece) const{
  return file.isPieceComplete(piece);
}

bool Peer::isFileEmpty() const{
  return file.isEmpty();
}

bool Peer::isFileComplete() const {
  return file.isComplete();
}

bool Peer::hasChoked(Peer::id pid) const {
  return hasRegChoked(pid) && hasOptChoked(pid);
}

bool Peer::hasRegChoked(Peer::id pid) const {
  for(auto peer: unch_peers){
    if(peer == pid) return false;
  }

  return true;
}

bool Peer::hasOptChoked(Peer::id pid) const {
  for(auto peer: optunch_peers){
    if(peer == pid) return false;
  }

  return true;
}

void Peer::mergePeers(std::list<Peer::id>&& newpeers){
  l_peer.merge(newpeers, cmpId);
  l_peer.sort();
  l_peer.unique();
}

void Peer::purgePeers(const Network& network){
  for(auto it = l_peer.begin(); it != l_peer.end();){
    if(!network.isUp(*it)) { it = l_peer.erase(it); }
    else { it++; }
  }
}

void Peer::addSubPiece(size_t piece){
  file.addSubPiece(piece);
}

void Peer::fileDownload(size_t piece){
  dw_pieces.insert(piece);
}

void Peer::unfileDownload(size_t piece){
  dw_pieces.erase(piece);
}

bool Peer::isDownloading(size_t piece) const{
  return dw_pieces.find(piece) != dw_pieces.end();
}

size_t Peer::getNbDownload() const{
  return dw_pieces.size();
}

void Peer::filePending(size_t piece){
  pend_pieces.insert(piece);
}

void Peer::unfilePending(size_t piece){
  pend_pieces.erase(piece);
}
bool Peer::isPending(size_t piece) const{
  return pend_pieces.find(piece) != pend_pieces.end();
}

void Peer::fileLeecher(Peer::id id){
  s_leecher.insert(id);
}

void Peer::unfileLeecher(Peer::id id){
  s_leecher.erase(id);
}

bool Peer::isLeechingTo(Peer::id id) const{
  return s_leecher.find(id) != s_leecher.end();
}

size_t Peer::getNbLeecher() const{
  return s_leecher.size();
}

std::list<size_t> Peer::getIncomplete(std::vector<uint8_t> v_pieces) const {
  std::list<size_t> l_intpiece;
  size_t sz_file = v_pieces.size();
  for(size_t i = 0; i < sz_file; i++){
    if(!isPieceComplete(i) && v_pieces[i] == File::nb_subpieces){
      l_intpiece.push_front(i);
    }
  }

  return l_intpiece;
}

void Peer::setBehaviour
  (Peer::bitfreq& new_onbitfrequest, Peer::onevent& new_onentrance,
   Peer::onpiece& new_onpiececomplete, Peer::onpiece& new_onpiececancel,
   Peer::onevent& new_onpiecefetching, Peer::onevent& new_onpeerfetching,
   Peer::onunch& new_onunchoking, Peer::onunch& new_onoptunchoking,
   Peer::onevent& new_onfilecomplete)
{
  on_bitfrequest = new_onbitfrequest;
  on_entrance = new_onentrance;
  on_piececomplete = new_onpiececomplete;
  on_piececancel = new_onpiececancel;
  on_piecefetching = new_onpiecefetching;
  on_peerfetching = new_onpeerfetching;
  on_unchoking = new_onunchoking;
  on_optunchoking = new_onoptunchoking;
  on_filecomplete = new_onfilecomplete;
}

/*******************************************************************************
  Network
*******************************************************************************/

Peer::Network::Network(size_t sz_file, size_t nb_annoucedpeers):
  avai_id(0), sz_file(sz_file) {};

std::vector<Peer::id> Peer::Network::drawPeers(std::mt19937& rng) const{
  std::vector<Peer::id> tmp;
  tmp.reserve(peers.size());
  for(const auto& id_peer: peers){
    tmp.push_back(id_peer.first);
  }
  std::shuffle(tmp.begin(), tmp.end(), rng);
  return tmp;
}

Peer::id Peer::Network::addPeer(bool is_filecomplete, File::kbyte upbw,
                                File::kbyte dwbw, size_t nb_unchpeer,
                                size_t nb_optunchpeer,
                                Peer::bitfreq& on_bitfreq,
                                Peer::onevent& on_entrance,
                                Peer::onpiece& on_piececomplete,
                                Peer::onpiece& on_piececancel,
                                Peer::onevent& on_piecefetching,
                                Peer::onevent& on_peerfetching,
                                Peer::onunch& on_unchoking,
                                Peer::onunch& on_optunchoking,
                                Peer::onevent& on_filecomplete,
                                Peer::Schedule& schedule,
                                std::mt19937& rng)
{
  peers[avai_id] = Peer(File(sz_file, is_filecomplete), upbw, dwbw, nb_unchpeer,
                        nb_optunchpeer, on_bitfreq, on_entrance,
                        on_piececomplete, on_piececancel, on_piecefetching,
                        on_peerfetching, on_unchoking, on_optunchoking,
                        on_filecomplete);

  peers[avai_id].onEntrance(avai_id, *this, schedule, rng);

  return avai_id++;
}

void Peer::Network::removePeer(Peer::id pid){
  peers.erase(pid);
}

Peer& Peer::Network::getPeer(Peer::id pid){
  return peers.at(pid);
}

bool Peer::Network::isUp(Peer::id pid) const{
  return peers.find(pid) != peers.end();
}
