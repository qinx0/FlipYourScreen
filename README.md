# Flip Your Screen

Have you every thought **"I suck at doing flipped ship"** or any other gamemode, well this mod is your solution!

Instead of you rotating your head 180°, Like some people or streamers you might know, the mod rotates the camera/screen 180° whenever your gravity flips — so your cube/ship/ball always appears **right-side up**, as if you're tilting your head instead of the world flipping. It can also flip your ui for you if you want to be even more disoriented.

## How it works
- Hooks into `PlayerObject::flipGravity` to detect every gravity switch
- Smoothly animates a camera wrapper node 180° using `CCEaseInOut`
- Resets instantly on death/restart

## Settings
- **Enabled** — Master Switch for the mod
- **Rotation Speed** — Controls how fast the camera rotates (default: 0.15s)
- **Flip UI Too** — Toggle whether the level UI rotates with the camera

**The flip settings can also be changed in the pause menu with two buttons**

## Suggestion/Bugs
If you find any bugs or want me to change something just DM me on discord, *qinx___*
