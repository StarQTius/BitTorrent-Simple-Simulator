# P2P-Simple-Simulator

Simulateur simple d'un réseau P2P
Ce projet fait partie du TIPE que je devais présenter au tétraconcours. Il s'agit d'un programme simulant un réseau P2P avec le protocole _BitTorrent_.

Le programme simule simplement le standard BitTorrent et n'implémente pas les fonctionnalités avancées que vous pouvez rencontrer dans les clients modernes.

L'objectif du projet était de démontrer la robustesse du protocole _BitTorrent_ au _free riding_ grâce à la simulation. Si vous voulez en savoir davantage, je vous invite à consulter [les diapositives](https://github.com/StarQTius/P2P-Simple-Simulator/blob/master/biblio/Pr%C3%A9sentation.pdf) qui étaient destinées à la présentation orale de mon TIPE.

## Installation
Clonez le dépôt, entrez dans le dossier `P2P-Simple-Simulator` puis exécuter la commande `make`
```
git clone https://github.com/StarQTius/P2P-Simple-Simulator
cd P2P-Simple-Simulator
make
```
...puis exécutez le programme `bittorent`.

Si vous avez lu [la présentation](https://github.com/StarQTius/P2P-Simple-Simulator/blob/master/biblio/Pr%C3%A9sentation.pdf), vous devriez pouvoir vous servir du programme. Si vous avez besoin d'aide, n'hésitez pas à me contacter.

## Bilbiographie

Une petite liste des articles sur lesquels je me suis basé pour réaliser ce projet :
- [Incentives Build Robustness in BitTorrent](http://bittorrent.org/bittorrentecon.pdf)
- [Modeling and performance analysis of BitTorrent-like peer-to-peer networks](https://dl.acm.org/doi/10.1145/1030194.1015508)
- [Fairness, incentives and performance in peer-to-peer networks](https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.121.9069&rep=rep1&type=pdf)
