![image](data/preview.png)

# Build instructions for Plasma 6

```
git clone https://github.com/guiodic/material-decoration.git
cd material-decoration
mkdir build
cd build
cmake .. -DQT_MAJOR_VERSION=6 -DQT_VERSION_MAJOR=6
make
sudo make install
```
for Arch and derivatives, please install the AUR package 
[material-kwin-decoration-git](https://aur.archlinux.org/packages/material-kwin-decoration-git)

NOTE: the master branch is aligned with the latest Plasma versions. For earlier
ones, see the other branches

## material-decoration

Material-ish window decoration theme for KWin.

### Locally Integrated Menus

This hides the AppMenu icon button and draws the menu in the titlebar.

Make sure you add the AppMenu button in System Settings > Application Style >
Window Decorations > Buttons Tab.

### Search Button

To show/hide SearchButton (the lens), edit ~/.config/kdecoration_materialrc

```
[Windeco]
SearchEnabled=true
```
or

```
[Windeco]
SearchEnabled=false
```
then restart kwin: `systemctl --user restart plasma-kwin_x11.service`

....

TODO/Bugs ([Issue #1](https://github.com/Zren/material-decoration/issues/1)):

* Open Submenu on Shortcut (eg: `Alt+F`)
* Display mnemonics when holding `Alt`
