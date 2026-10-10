# Load ocean audio with the correct type and enable looping

Suggested priority: P2. Type: bug.

## Problem and evidence

`project/scripts/player.gd:89` loads `ocean_ambience.mp3` as `AudioStreamWAV`. The resource loaded in Godot 4.7.2 is `AudioStreamMP3`, and the cast returns `null`. Consequently, the block assigning the stream and starting playback never executes.

## Proposal

Use `AudioStreamMP3` and its loop configuration, or configure looping in the imported resource and consume an `AudioStream`. Keep the Music bus and volume transition. Provide a clear diagnostic if the resource fails to load. Extracting audio into a component can happen later without blocking this fix.

## Acceptance criteria

- [ ] OceanAmbience receives a non-null stream and starts playback.
- [ ] Looping uses the API of the resource's actual type.
- [ ] A test checks stream assignment/configuration.
- [ ] A run with audio confirms looping and fading when entering/leaving the ocean; headless alone does not prove audible sound.
