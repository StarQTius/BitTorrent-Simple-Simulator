#L'essentiel est fait, il faut rajouter quelques types à Data_Event et étudier le
#protocole :
#	+partition des fichiers en pièces
# +la dimension p2p
# +le partage de bande passantes
# +la liste de pair sortie par le tracker
# +le choix des pairs dans cette liste
# +le processus d'unchoking
# +le processus d'optimistic unchoking
# +implementer le comportement des pairs (via le journal d'évenements)

#Faire requestRandom, requestPiece
#utiliser un set pour stocker les pièces téléchargées (requestRandom)

cmpflags = \
	-Wall \
	-std=c++14

linkflags =

VPATH =

src = \
	file.cpp \
	main.cpp \
	peer.cpp \
	bittorrent.cpp

obj = $(src:.cpp=.o)

all: $(obj)
	g++ -o bittorrent $(notdir $^) $(linkflags)

debug:
	DEBUG = -ggdb
debug: all

clean:
	rm -f *.o

mrproper: clean
	rm -rf bittorrent

%.o: %.cpp
	g++ -c $< $(cmpflags) $(DEBUG)
