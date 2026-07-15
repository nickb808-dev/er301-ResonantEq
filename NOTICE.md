# NOTICE — Resonant EQ attribution & provenance

**Resonant EQ** is an original implementation of a ten‑band fixed‑frequency
resonant filter bank for the Orthogonal Devices **ER‑301** Sound Computer,
inspired by the **Serge Resonant Equalizer** (a hardware analog module designed
by Serge Tcherepnin; the Eurorack adaptation "RS‑REQ" is by Random*Source).

## What is and isn't borrowed

- **Concept & band frequencies.** The idea of a bank of fixed resonant bandpass
  filters at the classic Serge centre frequencies (29 · 61 · 115 · 218 · 411 ·
  777 · 1500 · 2800 · 5200 · 11000 Hz) is drawn from the Serge design. Ideas,
  functionality, and the frequency table are not protected by copyright, and the
  original is analog hardware — there is no software source to copy or derive
  from.
- **Implementation.** The DSP is entirely original: ten parallel Cytomic
  state‑variable filters (Andrew Simper, 2013 — a published, freely usable
  topology), a per‑band CV → gain/Q voicing map, and host‑tuned normalisation,
  written natively against the ER‑301 SDK. No third‑party source code is
  included.

## Naming / trademark

The unit is named **"Resonant EQ,"** not "Serge." "Serge" is a trademark of its
owner; this unit is an independent tribute/clone and implies no affiliation with
or endorsement by Serge Modular / Random*Source.

---
*This NOTICE records provenance; it is not legal advice. See LICENSE for terms.*
