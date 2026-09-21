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
