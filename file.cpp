#include "file.hpp"

/*******************************************************************************
  File - Definitions
*******************************************************************************/

const File::kbyte File::sz_subpiece = 16;
const uint8_t File::nb_subpieces = 16;

/*******************************************************************************
  File - Methods
*******************************************************************************/

File::File(size_t nb_pieces, bool is_complete){
  if(is_complete){
    pieces = std::vector<uint8_t>(nb_pieces, nb_subpieces);
  }
  else{
    pieces = std::vector<uint8_t>(nb_pieces, 0);
  }
}

bool File::isPieceComplete(size_t i_piece) const{
  return pieces[i_piece] == nb_subpieces;
}

bool File::isEmpty() const{
  size_t sz_file = pieces.size();
  for(size_t i = 0; i < sz_file; i++){
    if(isPieceComplete(i)){
      return false;
    }
  }

  return true;
}

bool File::isComplete() const{
  size_t sz_file = pieces.size();
  for(size_t i = 0; i < sz_file; i++){
    if(!isPieceComplete(i)){
      return false;
    }
  }

  return true;
}

void File::addSubPiece(size_t i_piece){
  if(pieces[i_piece] < nb_subpieces){
    pieces[i_piece]++;
  }
}

const std::vector<uint8_t>& File::getPieces() const{
  return pieces;
}
