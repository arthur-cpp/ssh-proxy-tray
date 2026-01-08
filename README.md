# SSH Proxy Tray

Simple tray app for enabling a local SSH SOCKS proxy. Click the tray icon to turn the proxy on/off or switch between “system proxy” and “local‑only” modes.

## Overview
- Starts a local SOCKS proxy via SSH and shows its status in the tray
- Optional “Use as system proxy” to configure GNOME system proxy automatically
- Autostart and auto‑connect options

## Before You Start (SSH config)
Define your server once in `~/.ssh/config` and refer to it by an alias (default alias used by the app is `nl`, default SOCKS port is `1080`). Example:

```
Host nl
  HostName 203.0.113.23
  User alice
  Port 22
  IdentityFile ~/.ssh/id_ed25519
  ServerAliveInterval 60
  ServerAliveCountMax 3
  Compression yes
```

- Replace `203.0.113.23` with your server’s real IP or hostname.
- Make sure your key exists and is usable (ssh‑agent recommended).
- If you use a different alias, set it in Settings.
- If port 1080 is busy, choose another port in Settings.

## Install and Run
- If you want a Debian package, run `./build.sh` to build it, then install the generated `.deb` from `build-deb/...`.
- After installation, launch “SSH Proxy Tray” from your applications menu, or run `ssh-proxy-tray` in a terminal.

## Using The App
- SSH Proxy On/Off: start or stop the proxy
- Use as system proxy: toggle between system proxy and local‑only modes
- Autostart / Auto‑connect: control startup behavior
- Settings…: change SSH alias and SOCKS port

## Troubleshooting
- “Port is already in use”: pick another SOCKS port in Settings
- “Failed to connect”: test with `ssh <alias>` and verify `~/.ssh/config`
- System proxy didn’t change: ensure GNOME/`gsettings` is available

## Changes
- See `debian/changelog` for version history.

## License
- MIT.

## Copyright
Copyright (c) 2025–2026, Arthur V. (arthur‑cpp)
