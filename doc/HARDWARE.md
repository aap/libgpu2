# Model vs real silicon (DTL-T10000)

First hardware comparison campaign, 2026-09-02/03, against a DTL-T10000
(retail-equivalent GS revision; the machine reports CPUID=2e14,
BoardID=4126, ROMGEN=2003-1031).  Workflow: `tools/hwtest.py add` →
`dsedb -d devtool -nokbd -r run tools/ps2/gsrun.elf host0:hwtest` →
`tools/hwtest.py compare` (see tools/hwprep.py for the bundle format and
the PSMCT32 seeding bijection).  Streams are replayed UNGATED — none of
gsreplay's model workarounds (MTBA clearing, dropping registers
0x11-0x13) are applied on hardware.

## Suite result (85 frames: OSDSYS r614/o519 + 83 Ridge Racer V)

- **51/85 frames bit-identical** to the model across all 4 MB of VRAM:
  every menu/2D/composite frame and several 3D ones.
- The other 34 (all 3D gameplay) differ in 0.001%-6.4% of words, mean
  per-channel delta below ~2.  All differences are LSB-scale RGB; no
  structural divergence anywhere.
- The ungated 1998-dialect content (RRV's writes to register 0x11, MTBA
  usage) provoked nothing on retail silicon in any frame.
- One dump (045) carries an orphaned PATH1 GIFtag (a VU1 transfer in
  flight across the dump boundary); mainlined into the single FIFO it
  wedges the GIF.  hwprep reframes per path at tag boundaries — the same
  arbitration the real GIF performs — and the frame then replays
  bit-identical.

## Run-to-run nondeterminism (six runs of r614)

Real silicon does not reproduce itself exactly: across six runs of the
identical stream, 203 words were ever involved, of which

- **143 jitter words** vary between runs (±1 LSB in one RGB channel,
  concentrated in a few pages).  The model's value is one of the
  observed silicon values in 125/143 (87%).  Signature is consistent
  with a blend/AA destination-read hazard whose outcome depends on
  pipeline/eDRAM timing.
- **60 systematic words** are stable across all six runs and differ
  from the model by exactly +1 in one channel (model = silicon + 1).
  Alpha bytes of the affected pixels include partial-coverage values
  (0x0b, 0x24), i.e. AA1 edge pixels, alongside gradient pixels — a
  reproducible rounding difference in the blend/interpolation tail:
  the model rounds where the silicon truncates.

Consequences for methodology: a single-run silicon comparison has a
noise floor (~100-200 words on blend-heavy content); "model correct"
should be judged against the multi-run envelope, and only the stable
residue is a genuine model-vs-silicon arithmetic difference.  On r614
that residue is 60 words in 1,048,576 — 0.006%.

## Model-side notes

- The model has no CSR/IMR/BUSDIR at all (control-port writes exit as
  unknown registers), so the silicon revision id has no model
  counterpart to compare against.  For the record, this TOOL's GS
  reads CSR = 0x5508682c: chip ID 0x55, revision 0x08
  (`dsedb -d devtool dq 0x12001000 1`).
- gsreplay's replay path (GS_PutPort 0x7f per vsync) drives the model's
  old unmerged display circuit; the real PCRTC merge is pseudo-register
  0x101, reachable only through gpu2reg.
