# Perfect Dark VR

> ## Experimental online net play (this branch: `net-play`)
>
> This fork adds **online Combat Simulator** to Perfect Dark VR by merging the
> [perfect-dark-pc-port](https://github.com/perfect-dark-pc-port/perfect_dark) team's
> experimental `port-net` netcode into Alex-LeTux's VR port. Quest players can play each other,
> and Quest and PC (`port-net` build) players can play together. It is janky and experimental:
> Combat Simulator only (no co-op; the upstream netcode doesn't support it), and expect bugs.
>
> **Install (Quest):** sideload the APK from the latest
> [release](../../releases). It installs as a separate app, "Perfect Dark VR (NTSC) [netplay]",
> next to the regular one. Pick your own NTSC 1.1 ROM in its launcher, the same as the regular build.
>
> **Play:**
> - **Host:** in the launcher pick **Host**, then **Start**. Set up Combat Simulator, wait for
>   players to join, then start the match.
> - **Join:** in the launcher pick **Join**, type the host's IP (`1.2.3.4` or `1.2.3.4:port`) with the
>   headset keyboard, then **Start**.
> - Same Wi-Fi works as is. Over the internet, the host forwards **UDP 27100** on their router to
>   their headset (or uses a VPN such as Tailscale or ZeroTier).
> - Everyone needs the same ROM and the same build.
>
> Credits: Alex-LeTux (Perfect Dark VR) and the perfect-dark-pc-port team (`port-net` netcode).
> The netcode details are in [docs/netplay.md](docs/netplay.md).

**VR-only fork of Perfect Dark**  
**Targeting standalone VR headsets and PCVR**  
This fork is focused on developing VR features.

Release Trailer: https://www.youtube.com/watch?v=wkQiyeYn7QU  
Preview: https://www.youtube.com/watch?v=O23-4PhqpYE  

If you want to support the VR build, you can support me on Ko-fi: https://ko-fi.com/alexletux  

## Features
- **Full VR Immersion:** 6DoF support for modern VR headsets and motion controllers.
- **Multi-Region Support:** Play with NTSC, PAL, or JPN ROMs.
- **Accessibility:** Dedicated Left-handed mode included.
- **Enhanced Graphics:** Built-in support for Community HD Texture Packs.
- **Objective Checkpoints (optional):** turn on *Auto-Save on Objective* under VR Options → Weapons & Gameplay. Each time you complete an objective the game takes a checkpoint, and *Load Checkpoint* appears in the pause menu and on the mission-failed screen. Checkpoints are kept in memory for the current mission only (solo play, not net play); they are lost when you leave or restart the mission or close the app.


## How to play 
You must already have a Perfect Dark ROM to run the game. Starting from version 2.0, it is also possible to play with PAL and JPN ROMs using the appropriate builds.

**On PCVR**   
- Rename your Perfect Dark ROM according to its region, then place it in the "data" folder:
  - For **NTSC** ROMs, rename it to: `pd.ntsc-final.z64`
  - For **PAL** ROMs, rename it to: `pd.pal-final.z64`
  - For **JPN** ROMs, rename it to: `pd.jpn-final.z64`

### On Meta Quest Standalone
- **Installation:** Use SideQuest or your preferred side-loading tool to install the `.apk` file to your Meta Quest.
- **Loading the game:** Launch the app, press "Select ROM", and select the ROM from your headset's Downloads folder.

# Community Texture Packs

This project supports the **Community Texture Packs** created and maintained by the Perfect Dark community.

Special thanks to **Parabolee / Retro Foundry** and everyone who contributed to these texture packs. Full credits for the original authors and contributors can be found on the official project page:

https://github.com/retro-foundry/Perfect-Dark-Plus-HD-Textures

________________________________________________________  

This VR build is based on the PC port of Perfect Dark: https://github.com/perfect-dark-pc-port/perfect_dark  
And on the Android port of Perfect Dark: https://github.com/izzy2lost/perfect_dark
