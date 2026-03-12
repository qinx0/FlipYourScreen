# Flip Your Screen

Have you every thought **"I suck at doing flipped ship"** or any other gamemode, well this mod is your solution!

Instead of you rotating your head 180°, like some people or streamers you might know, the mod rotates the screen 180° whenever your gravity flips — so your cube/ship/ball always appears **right-side up**, as if you're tilting your head instead of the world flipping. It can also flip your UI if you want to be even more disoriented.

## How it works
- Hooks into `PlayerObject::flipGravity` to detect every gravity switch
- Smoothly animates the screen 180° using `CCEaseInOut`
- Optionally keeps the HUD (progress bar, percentage, etc.) upright while the gameplay rotates
- Resets instantly on death/restart

## Settings
- **Enabled** — Master switch for the mod
- **Rotation Speed** — Controls how fast the screen rotates (default: 0.3s)
- **Flip UI Too** — Toggle whether the level UI rotates with the screen

**The flip settings can also be changed in the pause menu with two buttons**

## Suggestions/Bugs
If you find any bugs or want me to change something just DM me on discord, *qinx___*
