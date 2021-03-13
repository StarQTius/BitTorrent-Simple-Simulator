#ifndef H_FILE
  #define H_FILE

  #include <exception>
  #include <fstream>
  #include <string>
  #include <utility>
  #include <vector>

  #include <iostream>

  class File{
    /**Definitions**/
    public:
      typedef long double kbyte;
      static const kbyte sz_subpiece;
      static const uint8_t nb_subpieces;

    /**Methods**/
    public:
      File() = default;
      File(size_t, bool);
      bool isPieceComplete(size_t) const;
      bool isEmpty() const;
      bool isComplete() const;
      size_t getSize() const { return pieces.size(); };
      void addSubPiece(size_t);
      const std::vector<uint8_t>& getPieces() const;

    /**Attributes**/
    private:
      std::vector<uint8_t> pieces;
  };
#endif
