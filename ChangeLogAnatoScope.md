3.0.2-anatoscope-1.14.1
====

Upload to the multi-region orchestrator artifact registry https://gitlab.com/anatoscope/circle/dev/orchestrator/gestion/-/issues/167

3.0.2-anatoscope-1.14.0
====

Remove all related code associated to `CHGBKCOL` extension

3.0.2-anatoscope-1.13.0
====

Add a rfb extension to allow client to change default Xorg background color.

3.0.2-anatoscope-1.12.0
=====

Tight security with VNC auth as preferred method to make extensions work again after the "Fix security type handling" https://github.com/novnc/noVNC/pull/1677
Fix forward declaration build errors.

3.0.2-anatoscope-1.11.0
=====

Add a timeout to the pointer event lock held by a client during pointer drags, to mitigate deadlocks sometimes caused by network issues.

3.0.2-anatoscope-1.10.0
=====

Add a "self-sufficient" Turbovnc docker image based on Ubuntu

3.0.2-anatoscope-1.9.0
=====

Fix WebSocket subprotocol priority between binary and base64 always getting base64.

3.0.2-anatoscope-1.8.0
=====

Go back to ubuntu-20.04 to be more compatible.

3.0.2-anatoscope-1.7.7
=====

Docker image from scratch.

3.0.2-anatoscope-1.7.6
=====

Use ubuntu:22.04 as base image

3.0.2-anatoscope-1.7.5
=====

Use the old tar.xz dpkg package compression.

3.0.2-anatoscope-1.7.4
=====

Docker image, pipeline for pushing to alpha.

3.0.2-anatoscope-1.7.3
=====

Rebase on TurboVNC 3.0.2.

2.2.6-anatoscope-1.7.3
=====

Move inactivity check and warning drawing to timer so it executes when no events from clients.

2.2.6-anatoscope-1.7.2
=====

Allow everything (except underscores) in the Anatoscope version number to be able to have dashes.

2.2.6-anatoscope-1.7.1
=====

Pushing a tag triggers a build and an upload of a generic GitLab package.

2.2.6-anatoscope-1.7.0
=====

Extension to send and receive a version string.

2.2.6-anatoscope-1.6.0
=====

Quality info event for auto quality.

2.2.6-anatoscope-1.5.1
=====

Add changelog.

2.2.6-anatoscope-1.5.0
=====

Option to terminate app and show warnings on user inactivity.

2.2.6-anatoscope-1.4.0
=====

Option to allow colour variation in interframe comparison.

2.2.6-anatoscope-1.3.0
=====

Update TurboVNC to 2.2.6.

2.2.5-anatoscope-1.2.1
=====

Fix a *streaming lag* regression at the beginning of the connection.

2.2.5-anatoscope-1.2.0
=====

Fix a *streaming lag* regression caused by adaptation to ping.

2.2.5-anatoscope-1.1.0
=====

Adapt quality level as an extension.

2.2.5-anatoscope-1.0.0
=====

Adapt quality level automatically.
