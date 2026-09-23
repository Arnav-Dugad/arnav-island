# Animation system

Continuity is represented as data, not as a queue of transitions. Each scalar owns
position, velocity, destination, epoch, mass, stiffness and damping. Retargeting
samples both position and velocity at the new epoch before changing the target.
The damped oscillator is solved analytically for underdamped, critical and
overdamped regimes; elapsed seconds determine the result.

`MotionEngine.h` contains centralized Balanced, Fluid, Playful, Snappy and Calm
tokens. The island body is deliberately heavier than the output-volume response.
The renderer converts the analytic trajectory into adaptive cubic Hermite pieces.
Quarter, midpoint and three-quarter approximation error is bounded to 0.002 DIP
during subdivision; a denser unit test checks the resulting representation.
The cubics are uploaded once per input change and sampled by DWM. They are not
frame callbacks or ordinary bezier easings. Absolute QPC start times align the
renderer with the physical model. There is no dependency on 60 Hz.

Width, height, corner radius, content reveal, drag position and volume are
interruptible. The same body and clip survive every state change. Text is not
stretched during body morphs. The volume bar independently chases its target.
Drag uses saturating resistance and a bounded release velocity. Rest has no
continuous motion. Reduced motion sets geometry directly and retains a small
critically damped content reveal.

The native Animation Lab exposes spring preset, stiffness, damping, mass,
expand/collapse/reverse, a media-sized destination, an impulse, and an explicit
300 ms UI-thread stall test. The stall is only a developer test command.

Not implemented yet: artwork shared-element morphs, arbitrary path morphs,
blur/reflection animation, staggered content, touch InteractionTracker, a full
gesture system and direct editing of every channel. The lab's media button tests
geometry; it does not manufacture a media session. Current content is laid out
for the expanded panel and is clipped in smaller lab-only geometry states.

Release gates: capture high-refresh motion, verify QPC continuity against actual
presentation timing, remove the transient input-envelope limitation, test rapid
direction reversal under CPU/GPU load, add reduced-motion and high-contrast UIA
acceptance runs. A numerical continuity test is not a visual smoothness certificate.

## v0.3 motion channels
Artwork X/Y, size and opacity are retained spring channels. Compact/Home/Media destinations retarget them from current position and velocity. The source bitmap is not recreated during a glide. A new track can replace the bitmap without reallocating the surface; a two-art crossfade remains future work.

Charging uses a one-shot spring opacity impulse; it does not run a perpetual pulse. Magnetic feedback shifts a lightweight highlight by at most two DIPs and compresses it on press. Labels are not transformed, preserving text clarity. Reduced motion disables spatial art retargeting and large body motion. Focus timers refresh once per second only while running; monitoring stops when the relevant view is hidden.

Native acrylic is deliberately interior-only and is hidden while the body is moving. File absorption is represented by immediate shelf insertion, expansion and a settling highlight pulse; no shell drag-image shared-element transition is claimed.

## v0.4 handoff and choreography

`MotionTokens` centralizes artwork, opacity, icon, navigation, ring and content springs. Icon feedback animates retained transforms; the selected navigation indicator retargets continuously. Reordered navigation hit regions follow the current spring positions.

Two artwork surfaces blend with a critically damped scalar. An interrupted change snapshots the visible composite before beginning the new handoff. Unit tests verify byte-identical continuity at the interruption and bounded storage across repeated changes. Cover color velocity is not claimed to be conserved; spatial artwork/body springs retain their physical state.

Content visibility depends smoothly on body height. Expanded content appears only when enough geometry is available, and the compact header appears as the body settles toward compact dimensions. The projected opacity and derivative are approximated with the existing adaptive Hermite curve mechanism, without a UI-thread frame loop. Sparse native motion captures confirmed removal of label/art overlap.

Glance arcs use actual battery or elapsed timer snapshots. Their endpoint markers rotate on compositor springs; there is no fabricated continuously looping ring. Focus arc text/ring redraws at the timer's 1 Hz update cadence.

## v0.5 details

The seek bar and handle use retained compositor transforms, with the central icon spring for emphasis. Playback advancement uses time-based linear compositor animation; it does not rely on a per-frame UI timer. Interactive scrubbing follows pointer position without queuing seek commands. The drop tile uses centralized `MotionTokens::drop` and `dropFade`, shrinks into a shelf row and fades; reduced motion omits its spatial motion. Audio route feedback is one-shot and tied to confirmed state, not an optimistic click animation.
## v0.6: small surfaces and continuous atmosphere

Live card geometry: 360 × 154 DIP. Mini resting geometry: 72 × 34 DIP. Body shape still uses the existing velocity-preserving spring. A separate low-height content gate lets Live controls emerge as space becomes available. Command Center navigation remains in the large state. Moving from the small card to another page replaces its content at the gate; this is not yet a shared-element transition for every label.

Shelf peek uses MotionTokens::peek for scale/opacity. Artwork atmosphere uses MotionTokens::atmosphere for three retained gradient channels. Changing color mid-animation samples the current channel spring; it preserves velocity and never starts a color redraw loop. Reduced motion resolves both immediately. Existing album cover handoff remains separate from the background atmosphere.

Additional cadence tests evaluate the analytical/Hermite trajectory at 60/90/120/144/165/240 Hz and enforce <0.004 DIP approximation error. They do not measure monitor presentation, FPS, VRR, input latency or dropped frames. Visual animations are sampled by DirectComposition; the transient 30 ms HWND input-region timer is not a render loop.
