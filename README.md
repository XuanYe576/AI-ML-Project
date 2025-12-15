
![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)

# Sound Pitch Recognition Methods

## Hidden Markov Model (HMM)
- Probabilistic model for sequential data
- States: phonemes or pitch levels
- Transition probabilities between states
- Emission probabilities for observations

## Progress
- Interface is completed
	- sound input
	- user input
	- write out sound file
- Next step
	- txt2xml.py
	- process .wav/ with YIN/ frequency method

### Fourier Transform
$$X(k) = \sum_{n=0}^{N-1} x(n) e^{-j2\pi kn/N}$$

Where:
- $x(n)$ = time-domain signal
- $N$ = number of samples
- $k$ = frequency bin index

## Feature Extraction
- MFCC (Mel-Frequency Cepstral Coefficients)
- Zero-crossing rate
- Spectral centroid

## Pitch Detection
- Autocorrelation
- YIN algorithm
- Fundamental frequency estimation

## Goal
sheet music by recognizing notes, durations, and other essential musical elements from audio inputs.

### Key Capabilities
- Detect pitches (notes) and map them to musical notation (e.g., A4, C#5)
- Identify and represent rests, chords, and polyphony where possible
- metadata: key sign, time signature, tempo
- MusicXML || MIDI

### Scope & Constraints
- Train by single instrument or a solo vocal. (AI/ML, Acoustic Numerical Physics Aided)
- Western equal-tempered and 12-tone scale
- Real-time performance / offline transcription

### Success Benchmark
- Pitch detection F0 Average Error: ≤ 20 cents
- Note onset detection F1 score (Fisher Score Based)
- Correct mapping to notated pitches and durations
- Output file validates as MusicXML or MIDI file

### Methods
- SP: STFT, spectral analysis, peak detection
- MFCC, spectral centroid, and zero-crossing rate for auxiliary features
- Pitch detection: autocorrelation, YIN, or deep learning-based F0 estimators
- Segmentation for onsets/offsets: energy envelope, spectral flux, or supervised onset detectors
- SM: HMM / CRF / RNN / Transformer
- Chord recognition / polyphony strategies: non-negative matrix factorization

### Data & Evaluation
- MAPS, MusicNet, maestro? corpora?
- Evaluate F0 error (cents), onset/offset F1
- MusicXML/MIDI validation?

### Milestones & Deliverables
1. MVP
	- Input: WAV/MP3 -> output: MusicXML or MIDI
	- Components: STFT + pitch detection + onset detection + quantization rule for note durations
2. Quantization and reduce pitch error
	- ML models for onset/pitch refinement; 
3. melody + harmony
	- Multi-pitch detection / chord inference
4. UIUX
	- record, score editors, export to PDF, batch processing
5. Single note recognization from STFT
    - trained on STFT of clear audio with note and instrument identification (train on multiple aligned samples of same note)
    - one recognition model for each instrument/sound
    - find probability of model instrument/sound
    - find probable parameters for transformations of note/sound (amplitude, pitch change (only for sounds))
5. Multi note recognization
    - one recognition model for each instrument/sound
    - trained on chords and stacked notes of one instrument
    - find probability of model instrument/sound
    - find probability peaks of amplitudes and pitches for some short time interval
7. note recognition in audio track
    - trained on STFT of audio track, with instrument and reference midi notes
    - find peaks in where the note most likely is (multiple passes (for chords) with multiple recognition models (multiinstrument))
    - find probable parameters for transformations of note/sound (time shift, amplitude, pitch change (only for sounds))


### Tools & Libraries Recommendations
- Python libraries: librosa, madmom, essentia, numpy, scipy
- Deep learning: PyTorch or TensorFlow for neural onset/pitch models
- music21 for handling MusicXML/MIDI and rendering

### Combine
- start with MAPS/Maestro/MusicNet and build a small validation set
- energy/onset detection + YIN pitch estimation + quantization to MusicXML
- Evaluate and iterate with more advanced models for pitch/onset

> Note: polyphonic transcribing scenarios.

## License

MIT License

Copyright (c) 2024 NoteRecord contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### Third-party licenses
- Qt: LGPLv3/GPLv3/Commercial (verify your chosen Qt package and deployment model).
- Python libraries (e.g., librosa, numpy, scipy, music21, PyTorch/TensorFlow): see their respective licenses.
- MusicXML schema: © MakeMusic, Inc.; see MusicXML license for details.
