-- ResonantEQMO.lua — ER-301 wrapper for Resonant EQ MO (mono, multi-out + FB) v0.5.0
--
-- Mono Serge Resonant EQ with the hardware's three outs, plus per-comb feedback:
--   In1 → In → [10 resonant bandpass, per-band CV]
--                 ├─ Σ all bands ─────────────────────→ Main   → Out1
--                 ├─ Σ odd bands (29/115/411/1.5k/5.2k)→ Comb 1 → Out2
--                 └─ Σ even bands (61/218/777/2.8k/11k)→ Comb 2 → Out3
--   Comb1FB / Comb2FB fold each comb back into the input independently — rising
--   feedback regenerates that comb's resonances and self-oscillates at its band
--   frequencies (bounded/soft-limited), so you can howl one half of the spectrum
--   while the other stays clean.  On stolmine firmware the three outs are pickable
--   sub-outs; on vanilla they degrade to Main + Comb 1 as a stereo pair.

local app      = app
local Class    = Class or require "Base.Class"
local Unit     = require "Unit"
local GainBias = require "Unit.ViewControl.GainBias"
local Encoder  = require "Encoder"
local BandsView = require "reseq.BandsView"

local libreseq = require "reseq.libreseq"

local BAND_LABELS = {"29", "61", "115", "218", "411", "777", "1.5k", "2.8k", "5.2k", "11k"}
local BAND_DESC   = {"29 Hz", "61 Hz", "115 Hz", "218 Hz", "411 Hz", "777 Hz",
                     "1500 Hz", "2800 Hz", "5200 Hz", "11000 Hz"}

local ResonantEQMO = Class {}
ResonantEQMO:include(Unit)

function ResonantEQMO:init(args)
  args.title    = "Resonant EQ MO"
  args.mnemonic = "RM"
  -- Multi-output: Main (all), Comb 1 (odd), Comb 2 (even).  channelCount=3 +
  -- subOutLabels for the stolmine local picker; vanilla ignores subOutLabels
  -- and surfaces only Out1(Main)+Out2(Comb 1) as a stereo pair.
  args.channelCount = 3
  args.subOutLabels = {"main", "comb1", "comb2"}
  Unit.init(self, args)
end

function ResonantEQMO:onLoadGraph(channelCount)
  local eq = self:addObject("eq", libreseq.ResonantEQMO())

  connect(self, "In1", eq, "In")

  for i = 1, 10 do
    local p = self:addObject("band" .. i, app.GainBias())
    local r = self:addObject("band" .. i .. "Range", app.MinMax())
    p:hardSet("Bias", 0.0)
    connect(p, "Out", r, "In")
    connect(p, "Out", eq, "Band" .. i)
    self:addMonoBranch("band" .. i, p, "In", p, "Out")
  end

  -- Feedback (each comb → input), CV-able [0,1], independent per comb.
  local fb1Param = self:addObject("fb1Param", app.GainBias())
  local fb1Range = self:addObject("fb1Range", app.MinMax())
  fb1Param:hardSet("Bias", 0.0)
  connect(fb1Param, "Out", fb1Range, "In")
  connect(fb1Param, "Out", eq, "Comb1FB")
  self:addMonoBranch("comb1fb", fb1Param, "In", fb1Param, "Out")

  local fb2Param = self:addObject("fb2Param", app.GainBias())
  local fb2Range = self:addObject("fb2Range", app.MinMax())
  fb2Param:hardSet("Bias", 0.0)
  connect(fb2Param, "Out", fb2Range, "In")
  connect(fb2Param, "Out", eq, "Comb2FB")
  self:addMonoBranch("comb2fb", fb2Param, "In", fb2Param, "Out")

  -- Drift — analog component tolerance [0,1] (0 = perfect … 1 = vintage-loose).
  local driftParam = self:addObject("driftParam", app.GainBias())
  local driftRange = self:addObject("driftRange", app.MinMax())
  driftParam:hardSet("Bias", 0.0)
  connect(driftParam, "Out", driftRange, "In")
  connect(driftParam, "Out", eq, "Drift")
  self:addMonoBranch("drift", driftParam, "In", driftParam, "Out")

  local levelParam = self:addObject("levelParam", app.GainBias())
  local levelRange = self:addObject("levelRange", app.MinMax())
  levelParam:hardSet("Bias", 1.0)
  connect(levelParam, "Out", levelRange, "In")
  connect(levelParam, "Out", eq, "Level")
  self:addMonoBranch("level", levelParam, "In", levelParam, "Out")

  connect(eq, "Main",  self, "Out1")
  connect(eq, "Comb1", self, "Out2")
  connect(eq, "Comb2", self, "Out3")
end

function ResonantEQMO:onLoadViews(objects, branches)
  local controls = {}
  local expanded = {}

  for i = 1, 10 do
    local name = "band" .. i
    controls[name] = GainBias {
      button      = BAND_LABELS[i],
      description = BAND_DESC[i],
      branch      = branches[name],
      gainbias    = objects[name],
      range       = objects[name .. "Range"],
      biasMap     = Encoder.getMap("[-1,1]"),
      initialBias = 0.0,
      gainMap     = Encoder.getMap("[-1,1]"),
    }
    expanded[i] = name
  end

  controls.comb1fb = GainBias {
    button      = "fb1",
    description = "Comb 1 feedback — odd bands → input (regen / self-osc)",
    branch      = branches.comb1fb,
    gainbias    = objects.fb1Param,
    range       = objects.fb1Range,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  expanded[11] = "comb1fb"

  controls.comb2fb = GainBias {
    button      = "fb2",
    description = "Comb 2 feedback — even bands → input (regen / self-osc)",
    branch      = branches.comb2fb,
    gainbias    = objects.fb2Param,
    range       = objects.fb2Range,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  expanded[12] = "comb2fb"

  controls.drift = GainBias {
    button      = "drift",
    description = "Drift — analog component tolerance",
    branch      = branches.drift,
    gainbias    = objects.driftParam,
    range       = objects.driftRange,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  expanded[13] = "drift"

  controls.level = GainBias {
    button      = "level",
    description = "Output Level (unity = 1)",
    branch      = branches.level,
    gainbias    = objects.levelParam,
    range       = objects.levelRange,
    biasMap     = Encoder.getMap("[0,2]"),
    initialBias = 1.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  expanded[14] = "level"

  -- Live band-value phosphor scope (Dirac/Varia-style), leading the strip.
  controls.bands = BandsView {
    name  = "bands",
    mo    = objects.eq,
    width = app.SECTION_PLY,
  }
  table.insert(expanded, 1, "bands")

  return controls, { expanded = expanded, collapsed = {} }
end

return ResonantEQMO
