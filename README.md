# Hrv
Plugin Max pour l'analyse de données de multiples capteurs de fréquence cardiaque.

## Compilation sur windows

- [Télécharger et installer msys2](https://www.msys2.org/) . Ne pas cocher "run msys2 now" dans l'installateur.
- Lancer msys2 mingw64
- Installer pacboy avec la commande `pacman -S pactoys`
- Installer git avec `pacman -Syu git`
- Installer cmake avec `pacboy -Syu cmake`
- Installer gcc avec `pacboy -Syu gcc`
- Installer ninja avec `pacboy -Syu ninja`
- Pas sur si 100% nécessaire mais, fermer le shell et en réouvrir un nouveau
- Cloner le repo avec `git clone <url git>` et cd dans le dossier
- Générer les fichiers de build en mode débug avec `cmake -S $PWD/max -B build_debug -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Debug`
- Build avec `cmake --build build_debug/`
- [Télécharger et installer max 8](https://cycling74.com/downloads/older) (non testé avec max 9)
- Aller dans le dossier `build_debug/` et copiez le dll dans le dossier max. Collez le dans le dossier `Documents/Max 8/Library`
- Si vous essayez d'instancier votre objet dans max8 vous devriez avoir une erreur 126. Max a besoin de DLL de msys64 pour faire marcher l'objet.
- Pour contourner ce problème, ouvrez un terminal mingw64 et lancez max dedans. max ne sera pas dans le path alors rendez-vous y avec ` cd /c/Program\ Files/Cycling\ \'74/Max\ 8/`
- Lancez maintenant Max 8 avec `./Max.exe`
- Vous devriez être en mesure de créer et de tester votre objet max !!!!