# PBI-10: Demo 3 - Conversational Companion (Local ASR + LLM)

## Overview
Implement Demo 3: Conversational Companion using local ASR and LLM so the robot can understand voice commands and engage in conversation using on-device AI.

## Problem Statement
The robot should be able to understand natural language commands and engage in conversation without relying on cloud services for privacy and offline operation.

## User Stories
- As a user, I want to give voice commands, so that I can control the robot hands-free.
- As a user, I want to have conversations with the robot, so that it feels more interactive and engaging.

## Technical Approach
- Integrate ReSpeaker microphone array with ROS 2.
- Implement Whisper ASR for speech-to-text (local processing).
- Integrate local LLM for conversation and command understanding.
- Create voice command interface for robot control.
- Test conversation quality and command recognition accuracy.
- Document ASR/LLM configuration and voice command vocabulary.

## UX/UI Considerations
- Voice feedback: audio responses from robot.
- Command confirmation: indication when command recognized.

## Acceptance Criteria
- ReSpeaker microphone array integrated with ROS 2.
- Whisper ASR processes speech to text locally.
- Local LLM generates responses and understands commands.
- Voice commands control robot functions.
- Conversation quality acceptable (natural, responsive).
- Command recognition accuracy >90%.
- All CoS from backlog met.

## Physical Robot Changes
- **ReSpeaker microphone array mounting**: Mount ReSpeaker microphone array on robot.
  - Mounting location: Upper deck or mast (optimal for voice pickup).
  - Mounting orientation: Forward-facing or omnidirectional depending on array type.
  - Cable routing: USB cable from microphone array to Jetson (ensure secure routing).
  - Power: Microphone array powered via USB from Jetson.
- **No wiring changes to Teensy** - microphone communicates directly with Jetson.
- **Physical testing**: Requires voice testing in various acoustic environments.

## Dependencies
- ReSpeaker microphone array hardware.
- Local LLM model (e.g., Llama, Mistral) running on Jetson.
- PBI 1 (Teensy Core Firmware) - requires robot control interface.
- PBI 6 (Aluminum Chassis Build) - microphone mounting may depend on chassis design.

## Open Questions
- Which LLM model: size vs performance tradeoff for Jetson.
- Voice command vocabulary: which commands to support?
- Microphone mounting location: upper deck vs mast (affects voice pickup quality).

## Related Tasks
See [Tasks for PBI 10](./tasks.md) (to be created)

**Parent Backlog**: [Backlog.md](../backlog.md)


