#include "main.hpp"

#include <fstream>

int main(){
  std::cout << "Period of leechers arrival : ";
  std::cin >> BitTorrent::leecherarrival_period;
  std::cout << "Period of freeriders arrival : ";
  std::cin >> BitTorrent::freeriderarrival_period;
  std::cout << "Period of seeding : ";
  std::cin >> BitTorrent::seeding_period;
  std::cout << "File size : ";
  std::cin >> BitTorrent::nb_piece;
  std::cout << "Freeriders annoucement period : ";
  std::cin >> BitTorrent::FreeRider::annoucement_period;
  std::cout << "Freeriders peer request period factor : ";
  std::cin >> BitTorrent::FreeRider::annoucement_period_increase;
  std::cout << "Date of end : "; Peer::Schedule::timestamp t_end;
  std::cin >> t_end;

  Peer::Network network(BitTorrent::nb_piece, BitTorrent::nb_peerbyannoucement);
  Peer::Schedule schedule;
  std::mt19937 rng({std::chrono::system_clock::to_time_t
                    (std::chrono::system_clock::now())});

  Peer::id seed = network.addPeer(
    true, BitTorrent::bw_up, BitTorrent::bw_dw, BitTorrent::nb_unchpeer,
    BitTorrent::nb_optunchpeer, BitTorrent::Seeder::onBitfRequest,
    BitTorrent::Seeder::onEntrance, BitTorrent::Seeder::onPieceComplete,
    BitTorrent::Seeder::onPieceCancel, BitTorrent::Seeder::onPieceFetching,
    BitTorrent::Seeder::onPeerFetching, BitTorrent::Seeder::onUnchoking,
    BitTorrent::Seeder::onOptUnchoking, BitTorrent::Seeder::onFileComplete,
    schedule, rng);

  Peer::EventData::Spawner spl;
  spl.is_filecomplete = false;
  spl.upbw = BitTorrent::bw_up;
  spl.dwbw = BitTorrent::bw_dw;
  spl.nb_unchpeer = BitTorrent::nb_unchpeer;
  spl.nb_optunchpeer = BitTorrent::nb_optunchpeer;
  spl.on_bitfreq = &BitTorrent::Leecher::onBitfRequest;
  spl.on_entrance = &BitTorrent::Leecher::onEntrance;
  spl.on_piececomplete = &BitTorrent::Leecher::onPieceComplete;
  spl.on_piececancel = &BitTorrent::Leecher::onPieceCancel;
  spl.on_piecefetching = &BitTorrent::Leecher::onPieceFetching;
  spl.on_peerfetching = &BitTorrent::Leecher::onPeerFetching;
  spl.on_unchoking = &BitTorrent::Leecher::onUnchoking;
  spl.on_optunchoking = &BitTorrent::Leecher::onOptUnchoking;
  spl.on_filecomplete = &BitTorrent::Leecher::onFileComplete;
  spl.rt_nextspawn = BitTorrent::leecherarrival_period;

  Peer::EventData::Spawner spfr;
  spfr.is_filecomplete = false;
  spfr.upbw = 0;
  spfr.dwbw = BitTorrent::bw_dw;
  spfr.nb_unchpeer = 0;
  spfr.nb_optunchpeer = 0;
  spfr.on_bitfreq = &BitTorrent::FreeRider::onBitfRequest;
  spfr.on_entrance = &BitTorrent::FreeRider::onEntrance;
  spfr.on_piececomplete = &BitTorrent::FreeRider::onPieceComplete;
  spfr.on_piececancel = &BitTorrent::FreeRider::onPieceCancel;
  spfr.on_piecefetching = &BitTorrent::FreeRider::onPieceFetching;
  spfr.on_peerfetching = &BitTorrent::FreeRider::onPeerFetching;
  spfr.on_unchoking = &BitTorrent::FreeRider::onUnchoking;
  spfr.on_optunchoking = &BitTorrent::FreeRider::onOptUnchoking;
  spfr.on_filecomplete = &BitTorrent::FreeRider::onFileComplete;
  spfr.rt_nextspawn = BitTorrent::freeriderarrival_period;

  schedule.pushEvent(schedule.getDate(), spl, BitTorrent::addPeer);
  schedule.pushEvent(schedule.getDate(), spfr, BitTorrent::addPeer);
  while(schedule.getDate() < t_end){
    while(schedule.getDate() < t_end){
      schedule.forward(network, rng);
    }
    std::cout << "Date of seeders peak : " << BitTorrent::t_seederpeak << std::endl;
    std::cout << "Date of beginning of measurement : "; Peer::Schedule::timestamp t_begin;
    std::cin >> t_begin;
    displayStatistic(t_begin, t_end);
    std::cout << "Extending to date : ";
    std::cin >> t_end;
  }

  return EXIT_SUCCESS;
}

void displayStatistic(Peer::Schedule::timestamp t_begin,
                      Peer::Schedule::timestamp t_end)
{
  uint64_t nb_totleecher = 0;
  uint64_t nb_totfreerider = 0;
  unsigned long l_uptime = 0;
  unsigned long fr_uptime = 0;
  unsigned long l_dwperiod = 0;
  unsigned long fr_dwperiod = 0;
  uint64_t nb_lregsubpiece = 0;
  uint64_t nb_loptsubpiece = 0;
  uint64_t nb_lsubpiecefromseed = 0;
  uint64_t nb_frregsubpiece = 0;
  uint64_t nb_froptsubpiece = 0;
  uint64_t nb_frsubpiecefromseed = 0;
  std::vector<double> avaiability_pieces(BitTorrent::nb_piece, 0);
  std::vector<uint64_t> nb_subpiecesbyopt(BitTorrent::nb_piece, 0);
  std::vector<uint64_t> nb_subpiecerandom(BitTorrent::nb_piece, 0);
  for(const auto& pid_stat: BitTorrent::statistic){
    if(pid_stat.second.t_entry >= t_begin && pid_stat.second.t_leave > 0){
      switch(pid_stat.second.type){
        case BitTorrent::Stat::Type::LEECHER: {
          nb_totleecher++;
          l_uptime += pid_stat.second.t_leave - pid_stat.second.t_entry;
          l_dwperiod += pid_stat.second.t_completed - pid_stat.second.t_entry;
          nb_lregsubpiece += pid_stat.second.nb_subpiecebyunch;
          nb_loptsubpiece += pid_stat.second.nb_subpiecebyoptunch;
          nb_lsubpiecefromseed += pid_stat.second.nb_subpiecefromseed;
          for(size_t i = 0; i < BitTorrent::nb_piece; i++){
            avaiability_pieces[i] += ((double)(pid_stat.second.t_leave - pid_stat.second.t_obtpiece[i])) / (t_end - t_begin);
            nb_subpiecesbyopt[i] += pid_stat.second.nb_subpiecebyopt[i];
            nb_subpiecerandom[i] += pid_stat.second.nb_subpiecerandom[i];
          }
        }
        break;
        case BitTorrent::Stat::Type::SEEDER:
        break;
        case BitTorrent::Stat::Type::FREERIDER: {
          nb_totfreerider++;
          fr_uptime += pid_stat.second.t_leave - pid_stat.second.t_entry;
          fr_dwperiod += pid_stat.second.t_completed - pid_stat.second.t_entry;
          nb_frregsubpiece += pid_stat.second.nb_subpiecebyunch;
          nb_froptsubpiece += pid_stat.second.nb_subpiecebyoptunch;
          nb_frsubpiecefromseed += pid_stat.second.nb_subpiecefromseed;
          for(size_t i = 0; i < BitTorrent::nb_piece; i++){
            nb_subpiecesbyopt[i] += pid_stat.second.nb_subpiecebyopt[i];
            nb_subpiecerandom[i] += pid_stat.second.nb_subpiecerandom[i];
          }
        }
        break;
      }
    }
  }
  if(nb_totleecher > 0){
    l_uptime /= nb_totleecher;
    l_dwperiod /= nb_totleecher;
    for(size_t i = 0; i < BitTorrent::nb_piece; i++){
      avaiability_pieces[i] /= nb_totleecher;
    }
  }
  if(nb_totfreerider > 0){
    fr_uptime /= nb_totfreerider;
    fr_dwperiod /= nb_totfreerider;
  }

  std::cout << "Period of leechers arrival : " << BitTorrent::leecherarrival_period << std::endl;
  std::cout << "Period of freeriders arrival : " << BitTorrent::freeriderarrival_period << std::endl;
  std::cout << "Period of seeding : " << BitTorrent::seeding_period << std::endl;
  std::cout << "File size : " << BitTorrent::nb_piece << std::endl;
  std::cout << "Freeriders annoucement period : " << BitTorrent::FreeRider::annoucement_period << std::endl;
  std::cout << "Freeriders peer request period factor : " << BitTorrent::FreeRider::annoucement_period_increase << std::endl;
  std::cout << "Date of end : " << t_end << std::endl;
  std::cout << "Number of considered leechers : " << nb_totleecher << std::endl;
  std::cout << "Number of considered free-riders : " << nb_totfreerider << std::endl;
  std::cout << "Leechers average uptime : " << l_uptime << std::endl;
  std::cout << "Free-rider average uptime : " << fr_uptime << std::endl;
  std::cout << "Leechers average download period : " << l_dwperiod << std::endl;
  std::cout << "Free-rider average download period : " << fr_dwperiod << std::endl;
  std::cout << "Number of subpiece downloaded by the leechers through regular unchoking : " << nb_lregsubpiece << std::endl;
  std::cout << "Number of subpiece downloaded by the leechers through optimistic unchoking : " << nb_loptsubpiece << std::endl;
  std::cout << "Number of subpiece downloaded by the leechers from seeds : " << nb_lsubpiecefromseed << std::endl;
  std::cout << "Number of subpiece downloaded by the free-riders through regular unchoking : " << nb_frregsubpiece << std::endl;
  std::cout << "Number of subpiece downloaded by the free-riders through optimistic unchoking : " << nb_froptsubpiece << std::endl;
  std::cout << "Number of subpiece downloaded by the free-riders from seeds : " << nb_frsubpiecefromseed << std::endl;
  auto lw_piece = std::min_element(avaiability_pieces.begin(), avaiability_pieces.end());
  auto hg_piece = std::max_element(avaiability_pieces.begin(), avaiability_pieces.end());
  std::cout << "Lowest piece avaiability : " << *lw_piece
            << " (Piece " << std::to_string(lw_piece - avaiability_pieces.begin())
            << " - number of subpieces obtained through optimistic unchoking : " << nb_subpiecesbyopt[lw_piece - avaiability_pieces.begin()]
            << " - number of subpieces obtained randomly : " << nb_subpiecerandom[lw_piece - avaiability_pieces.begin()]
            << " / " << File::nb_subpieces * nb_totleecher
            << ")" << std::endl;
  std::cout << "Highest piece avaiability : " << *hg_piece
            << " (Piece " << std::to_string(hg_piece - avaiability_pieces.begin())
            << " - number of subpieces obtained through optimistic unchoking : " << nb_subpiecesbyopt[hg_piece - avaiability_pieces.begin()]
            << " - number of subpieces obtained randomly : " << nb_subpiecerandom[hg_piece - avaiability_pieces.begin()]
            << " / " << File::nb_subpieces * nb_totleecher
            << ")" << std::endl;
}
