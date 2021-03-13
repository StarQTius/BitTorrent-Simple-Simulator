/**File, princialement la biblio**/
public:
  typedef std::vector<std::vector<File>> library;
  struct CmpCatRank;
  static const kbyte sz_piece = 256;
  static std::pair<library, size_t> createLibrary(const std::string&);
private:
  size_t cat;
  size_t rank;
struct File::CmpCatRank{
  bool operator()(const File&, const File&) const;
};
/*******************************************************************************
  File - Statics
*******************************************************************************/
/**
  Charge l'univers à partir d'un fichier:
    -fname: nom du fichier contenant les données pour initialiser l'univers
    NB: Voir en-tête du fichier 'file.hpp' pour les spécifications du format
    des données
    NB: Si une exception est levée, 'fs.close()' est appelé et l'exception est
    relancée
**/
std::pair<File::library, size_t> File::createLibrary(const std::string& fname){
  std::fstream fs;
  fs.open(fname.c_str(), std::fstream::in);
  size_t nb_files = 0;
  std::vector<std::vector<File>> lib;

  try{
    size_t cat = 0;
    fs >> cat;
    lib.resize(cat);

    size_t rank = 0;
    size_t val = 0;
    while(!fs.eof() && (cat != 0 || rank != 0)){
      fs >> val;
      if(rank == 0){
        rank = val;
        nb_files += val;
        cat--;
        lib.at(cat).resize(rank);
      }
      else{
        rank--;
        lib.at(cat).at(rank) = File(cat, rank, val, true);
      }
    }
  }
  catch(std::exception& e){
    fs.close();
    throw;
  }
  fs.close();

  return std::make_pair(lib, nb_files);
}
/*******************************************************************************
  File - CmpCatRank
*******************************************************************************/

/**
  Compare deux fichier selon un ordre lexicographique strict (catégorie puis
  rang):
    -a, b : fichiers à comparer
    +retourne true si a < b selon l'ordre lexicographique, false sinon
**/
bool File::CmpCatRank::operator()(const File& a, const File& b) const{
  return (a.cat < b.cat) || ((a.cat == b.cat) && (a.rank < b.rank));
}
