Hello! This is my attempt to create a Vulkan renderer for old Unreal Engine based games (Rune, Unreal Tournament, etc.). Inspired by [kentie](https://kentie.net/article/d3d10drv/)

This is a work in progress and I have no commitments.
For now I am concentrating only on Windows support for quick start, but later might add linux build (who knows).
At the moment I can play Unreal Tournament 1999 v436 is playable, I have not tried other games yet.

What is implemented
===================
 - Baseline functionality. I beleive all existing functionality is there, though I can miss something.

What is missing
===============
 - Optimization. Code is not optimized at all and was written for correctness first. I am planning to optimize it at some point (this should not be hard).
 - Mipmap support is missing (easy to add)
 - Build scripts. I did not want to spend time on not fun stuff, so here you are.. VS project which is setup for Unreal Tournament Debug Win32

What could be added
===================
 - RayTraced something, not sure though, because there is not so much freedom all triangles come as pretransformed triangle soup in view space, so not that much useful for ray tracing.
 - Soft particles (should be easy)


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





