# PBI-14: Demo 7 - Voice Command + Wake Word

## Overview
Implement Demo 7: Voice Command + Wake Word so the robot can wake from standby mode and respond to voice commands without continuous listening.

## Problem Statement
Continuous voice listening consumes power and may trigger false positives. Wake word detection allows the robot to conserve power and only activate when needed.

## User Stories
- As a user, I want to wake the robot with a wake word, so that it activates when I need it.
- As a user, I want voice commands after wake word, so that I can control the robot hands-free.

## Technical Approach
- Implement wake word detection (e.g., "Hey Johnny" or similar).
- Integrate wake word with ASR pipeline (activate on wake word).
- Create standby mode that conserves power when not in use.
- Test wake word detection accuracy and false positive rate.
- Test voice command recognition after wake word activation.
- Document wake word configuration and voice command vocabulary.

## UX/UI Considerations
- Wake word feedback: indication when wake word detected.
- Standby status: clear indication of standby vs active mode.

## Acceptance Criteria
- Wake word detection implemented and working.
- ASR activates on wake word detection.
- Standby mode conserves power effectively.
- Wake word detection accuracy >95%.
- False positive rate <1%.
- Voice commands work after wake word activation.
- All CoS from backlog met.

## Physical Robot Changes
- **ReSpeaker microphone array**: Uses same microphone mounting as PBI 10 (Conversational Companion).
  - If PBI 10 not completed: Mount ReSpeaker microphone array per PBI 10 specifications.
  - Mounting location: Upper deck or mast (optimal for voice pickup).
- **No additional wiring changes** - microphone communicates with Jetson via USB.
- **Physical testing**: Requires voice testing in various acoustic environments.

## Dependencies
- PBI 10 (Conversational Companion) - ASR infrastructure.
- ReSpeaker microphone array hardware.

## Open Questions
- Wake word selection: which phrase to use?
- Standby power consumption target: how much power to save?

## Related Tasks
See [Tasks for PBI 14](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


