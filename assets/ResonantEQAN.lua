-- ResonantEQAN.lua — ER-301 wrapper for Resonant EQ AN (mono analyzer) v0.6.0
--
-- Serge Resonant EQ + a per-band ENVELOPE FOLLOWER bank.  Ten fixed resonant
-- bandpass filters:
--   In1 → In → [10 resonant bandpass, per-band CV]
--                 ├─ Σ all bands ──────────────────────→ Main  → Out1  (audio)
--                 └─ env follower on each band ─────────→ Env1…Env10 → Out2…Out11
--   Regen folds Main back into the input (one control, scaled per band via the
--   gain-weighted Main sum): boosted bands regenerate more, following your EQ
--   curve.  EnvRise / EnvFall set the follower attack/release, EnvGain scales the
--   10-band spectral-CV bank.  channelCount=11 + subOutLabels for the stolmine
--   picker; vanilla firmware surfaces Main + Env1 as a stereo pair.

local app      = app
local Class    = Class or require "Base.Class"
local Unit     = require "Unit"
local GainBias = require "Unit.ViewControl.GainBias"
local Encoder  = require "Encoder"

local libreseq = require "reseq.libreseq"

local BAND_LABELS = {"29", "61", "115", "218", "411", "777", "1.5k", "2.8k", "5.2k", "11k"}
local BAND_DESC   = {"29 Hz", "61 Hz", "115 Hz", "218 Hz", "411 Hz", "777 Hz",
                     "1500 Hz", "2800 Hz", "5200 Hz", "11000 Hz"}

local ResonantEQAN = Class {}
ResonantEQAN:include(Unit)

function ResonantEQAN:init(args)
  args.title    = "Resonant EQ AN"
  args.mnemonic = "RA"
  -- Multi-output: Main (audio) + Env1…Env10 (per-band envelope CVs).
  -- channelCount=11 + subOutLabels for the stolmine local picker; vanilla
  -- ignores subOutLabels and surfaces only Out1(Main)+Out2(Env1) as a pair.
  args.channelCount = 11
  args.subOutLabels = {"main", "env1", "env2", "env3", "env4", "env5",
                       "env6", "env7", "env8", "env9", "env10"}
  Unit.init(self, args)
end

function ResonantEQAN:onLoadGraph(channelCount)
  local eq = self:addObject("eq", libreseq.ResonantEQAN())

  connect(self, "In1", eq, "In")

  for i = 1, 10 do
    local p = self:addObject("band" .. i, app.GainBias())
    local r = self:addObject("band" .. i .. "Range", app.MinMax())
    p:hardSet("Bias", 0.0)
    connect(p, "Out", r, "In")
    connect(p, "Out", eq, "Band" .. i)
    self:addMonoBranch("band" .. i, p, "In", p, "Out")
  end

  -- Regen — Main → input feedback, one control, scaled per band [0,1].
  local regenParam = self:addObject("regenParam", app.GainBias())
  local regenRange = self:addObject("regenRange", app.MinMax())
  regenParam:hardSet("Bias", 0.0)
  connect(regenParam, "Out", regenRange, "In")
  connect(regenParam, "Out", eq, "Regen")
  self:addMonoBranch("regen", regenParam, "In", regenParam, "Out")

  -- Envelope follower: Rise / Fall (attack/release) + Gain (CV-bank scale).
  local function ctl(name, inlet, bias)
    local p = self:addObject(name .. "Param", app.GainBias())
    local r = self:addObject(name .. "Range", app.MinMax())
    p:hardSet("Bias", bias)
    connect(p, "Out", r, "In")
    connect(p, "Out", eq, inlet)
    self:addMonoBranch(name, p, "In", p, "Out")
  end
  ctl("envRise", "EnvRise", 0.6)
  ctl("envFall", "EnvFall", 0.25)
  ctl("envGain", "EnvGain", 1.0)

  -- Drift — analog component tolerance [0,1] (0 = perfect … 1 = vintage-loose).
  local driftParam = self:addObject("driftParam", app.GainBias())
  local driftRange = self:addObject("driftRange", app.MinMax())
  driftParam:hardSet("Bias", 0.0)
  connect(driftParam, "Out", driftRange, "In")
  connect(driftParam, "Out", eq, "Drift")
  self:addMonoBranch("drift", driftParam, "In", driftParam, "Out")

  -- Level [0,2] — Main audio out gain.
  local levelParam = self:addObject("levelParam", app.GainBias())
  local levelRange = self:addObject("levelRange", app.MinMax())
  levelParam:hardSet("Bias", 1.0)
  connect(levelParam, "Out", levelRange, "In")
  connect(levelParam, "Out", eq, "Level")
  self:addMonoBranch("level", levelParam, "In", levelParam, "Out")

  connect(eq, "Main", self, "Out1")
  for i = 1, 10 do
    connect(eq, "Env" .. i, self, "Out" .. (i + 1))
  end
end

function ResonantEQAN:onLoadViews(objects, branches)
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

  controls.regen = GainBias {
    button      = "regen",
    description = "Regen — Main → input (scaled per band)",
    branch      = branches.regen,
    gainbias    = objects.regenParam,
    range       = objects.regenRange,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  controls.envRise = GainBias {
    button      = "rise",
    description = "Env Rise (attack)",
    branch      = branches.envRise,
    gainbias    = objects.envRiseParam,
    range       = objects.envRiseRange,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.6,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  controls.envFall = GainBias {
    button      = "fall",
    description = "Env Fall (release)",
    branch      = branches.envFall,
    gainbias    = objects.envFallParam,
    range       = objects.envFallRange,
    biasMap     = Encoder.getMap("[0,1]"),
    initialBias = 0.25,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  controls.envGain = GainBias {
    button      = "egain",
    description = "Env Gain (CV bank scale)",
    branch      = branches.envGain,
    gainbias    = objects.envGainParam,
    range       = objects.envGainRange,
    biasMap     = Encoder.getMap("[0,2]"),
    initialBias = 1.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
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
  controls.level = GainBias {
    button      = "level",
    description = "Unity = 1",
    branch      = branches.level,
    gainbias    = objects.levelParam,
    range       = objects.levelRange,
    biasMap     = Encoder.getMap("[0,2]"),
    initialBias = 1.0,
    gainMap     = Encoder.getMap("[-1,1]"),
  }
  expanded[11] = "regen"
  expanded[12] = "rise"
  expanded[13] = "fall"
  expanded[14] = "egain"
  expanded[15] = "drift"
  expanded[16] = "level"

  return controls, { expanded = expanded, collapsed = {} }
end

return ResonantEQAN
