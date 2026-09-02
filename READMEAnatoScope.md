# TurboVNC

This is a fork of https://github.com/TurboVNC/turbovnc


## Fork overview

Main features:

* handling of inactive clients (no user input)
  * showing a popup warning about incoming shutdown
  * sending a signal to the parent process
  * sending a warning to clients via `WRNEVENT`
  * cf `-inactwarntimeout` / `-inacttimeout` / `-inactsignal`
  * tracing inactivity in the logs (`-inactlogtimeout`)
* extensions to the RFB protocol
  * `CLSRVVER`: to allow client / server to exchange their respective version numbers (cf `-clsrvverfile`)
  * `MAXDELAY`: an alternative to `JPEGQLVL` for *automatic* image quality adaptation
  * `VISEVENT`: to allow a client to control inactivity popup
  * `WRNEVENT`: to send warnings to a client
* custom parameters to the interframe comparisons (cf `-interframeeps` /  `-interframeallowconv`)
* tracing of the network round-trip time (cf `-congtraceintervalms`)

There are bits of documentation in `doc/index.html`, `doc/performance.txt` and `unix/Xvnc/programs/Xserver/Xvnc.man.in`. (Tip: check out the changes from upstream.)


## How-to

### Release

The fork is versioned as `x.y.z-anatoscope-a.b.c`, where `x.y.z` is the upstream TurboVNC version, and `a.b.c` is the version of our changes.

To create a `x.y.z-anatoscope-a.b.c` release:

* Make sure the version number is up to date in  `CMakeLists.txt` and `doc/index.html`
* Make sure `ChangeLogAnatoScope.md` is up to date
* Tag the release commit with the version number `x.y.z-anatoscope-a.b.c` to create the release pipeline
