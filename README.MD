Hello! This is my attempt to create a Vulkan renderer for old Unreal Engine based games (Rune, Unreal Tournament, etc.). Inspired by [kentie](https://kentie.net/article/d3d10drv/)

This is a work in progress and I have no commitments.
For now I am concentrating only on Windows support for quick start, but later might add linux build (who knows).


Prerequisites
=============

You need to have original game files to run the game. This repo will only include files necessary to build a driver.


Building
========

To build a driver dll you need to have header and lib files for your game.
You can either create a 'game' folder here and put them there or just make a symlink.

Game folder structure:
`
game\<GameName>\Core\Inc
game\<GameName>\Core\Lib
game\<GameName>\Engine\Inc
game\<GameName>\Engine\Lib
`

After this, open solution, select correct configuration and build a dll. Resulting filesshould appear in 'packages' folder, copy them to your game 'System' folder.





