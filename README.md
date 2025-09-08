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

NOTE: the master branch is aligned with the latest Plasma version. For earlier
ones, see the other branches

## material-decoration

Material-ish window decoration theme for KWin.


### Locally Integrated Menus

This hides the AppMenu icon button and draws the menu in the titlebar. 
It works on X11 and Wayland.
On Wayland, GTK apps don't export the menu. You need to start them with `GDK_BACKEND=x11` environmental variable

Make sure you add the AppMenu button in System Settings > Application Style >
Window Decorations > Buttons Tab.

The menu is set to be “always shown” by default. If you want it to be shown only when the mouse hovers over it, edit `~/.config/kdecoration_materialrc`

```
[Windeco]
MenuAlwaysShow=false
```
then restart kwin: `systemctl --user restart plasma-kwin_x11.service`


### Search Button

To hide SearchButton (the lens), edit `~/.config/kdecoration_materialrc` 

```
[Windeco]
SearchEnabled=false
```
then restart kwin: `systemctl --user restart plasma-kwin_x11.service`

Disabled menu actions (the gray ones) are not displayed in the search by default.
If you wish to activate them:

```
[Windeco]
ShowDisabledActions=true
```

If you frefer an Hamburger Menu:

```
[Windeco]
HamburgerMenu=true
```


....

TODO/Bugs ([Issue #1](https://github.com/Zren/material-decoration/issues/1)):

* Open Submenu on Shortcut (eg: `Alt+F`)
* Display mnemonics when holding `Alt`
