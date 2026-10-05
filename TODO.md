# TODO

Open work on the BLACK PC port. Findings and addresses live in `NOTES.md`; this is the short list.

## Verify (fixed, waiting for a playtest)

- [x] Fades: the game's gamma ramp (fade factor), now applied at present (`ramp_texture` in loader/src/d3d/device.cpp).
      User confirmed the fade works, and the in-game brightness slider now works too.
- [ ] Picture vs the Xbox Series: compare screenshots of the same still screen with the same brightness slider setting
      (method in NOTES.md, "To check"). Also check the other fades: loading screen before missions, mission start after
      the intro (played and skipped), intro cinematic -> loading screen, level-end trigger -> fade to black.
- [ ] Anything else that is timed in seconds (blur ramps, objective text typing, HUD animations) now runs at the
      right speed.
- [ ] Higher internal resolution (`--scale N`, default 3 = 2560x1440 widescreen, window fitted to the screen,
      DPI-aware): check that every effect still lines up (light shafts, reload blur, bloom, fades, video, menus) at 1-4.
- [ ] Widescreen (default; the game's own 16:9 mode, `--normal-aspect` for 4:3): menus, HUD, videos, briefings.
- [ ] Window resizing at a fixed aspect, F11 fullscreen, black bars when maximised.
- [x] Mouse look: rotates the view directly (user: "feels awesome").
- [ ] Auto-aim switch (Game > Disable auto-aim, `--no-autoaim`; default on = original): confirm in play that the view
      no longer drifts onto enemies with it ticked, and that nothing else changes.
- [ ] Keyboard layout: the user's binding is in (see loader/README.md); confirm in play.
- [ ] 60 fps (`--fps 60`, not the default): world step 1/60 s, audio fine, weapon refire and reload blur now match
      30 fps (measured). Check the rest in play: other weapons/fire modes, animations, AI, effects, scripted scenes;
      decide whether 60 becomes the default.
- [ ] Reverb (bin 11 send into an FDN reverb): level and character against the Xbox (`--reverb-gain`).

## Next

- [x] Custom render resolution: `--resolution WxH | H | native`, fractional `--scale` (any size, not just multiples).
- [x] Render size changes while running: `native` follows the window (and F11), F10 cycles 1 / 1.5 / 2 x the window.
- [ ] Faint grid in the game's blur at high render sizes (blur offsets meant for the 160x120/80x60 buffers): give
      those small buffers their own, smaller scale (per-target scale for viewport/scissor/rects/pre-transformed vertices).
- [ ] Resolution: better downsampling when the render size is much larger than the window (box filter instead of
      one bilinear tap).
- [ ] Rebindable keys (a config file) if the in-game button remapping is not enough.
- [x] Settings remembered between launches (black.ini: render size, AA, fps, aspect, fullscreen, gain/gamma, mouse, reverb).
- [ ] Also remember the window size and position.
- [x] Port options in a Windows menu bar (vanilla port). In-game Apt menu entries: later, when modifying the game
      (scoping in NOTES.md, "Front end / custom menu options").

## Audio

- [ ] Reverb: exact room from the DSP image (effect 16's state is DSP-format; the HLE uses I3DL2 values by ear).
- [ ] Bins 17/18 (effects 13/14 -> outputs 0xE20/0xE40) and 27-30 (delay effects 18-21) are not mixed.
- [ ] LFE (subwoofer) bin is dropped.
- [ ] 3D sound: cones, doppler and the per-voice rolloff curve are ignored (distance + pan only).
- [ ] Output often reaches full scale (soft limiter active); check voice/mix-bin headroom against the Xbox.

## Open questions

- [ ] Where the Xbox re-fits the viewport to a smaller render target (the light-shaft pass relies on it; the HLE
      re-fits on every SetRenderTarget, see `fit_viewport`).
