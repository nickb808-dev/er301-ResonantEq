-- ResonantEQ.lua — ER-301 unit wrapper for Resonant EQ (STEREO) v0.5.0
--
-- The plain stereo Serge Resonant EQ: ten fixed-frequency resonant bandpass
-- filters summed to one output, per channel.  Bands (Hz): 29 · 61 · 115 · 218 ·
-- 411 · 777 · 1.5k · 2.8k · 5.2k · 11k.  Each band is bipolar and CV-able
-- (cut ‹ flat › boost › resonate).  Stereo in → stereo out; both channels share
-- the band settings.  See the "Resonant EQ MO" unit for the mono multi-output
-- (Main/Comb 1/Comb 2) version with feedback.

local app      = app
local Class    = Class or require "Base.Class"
local Unit     = require "Unit"
local GainBias = require "Unit.ViewControl.GainBias"
local Encoder  = require "Encoder"
local BandsView = require "reseq.BandsView"

local libreseq = require "reseq.libreseq"

local BAND_LABELS = {"29", "61", "115", "218", "411", "777", "1.5k", "2.8k", "5.2k", "11k"}
local BAND_DESC   = {"29 Hz", "61 Hz", "115 Hz", "218 Hz", "411 Hz", "777 Hz",
                     "1.5 kHz", "2.8 kHz", "5.2 kHz", "11 kHz"}

local ResonantEQ = Class {}
ResonantEQ:include(Unit)

function ResonantEQ:init(args)
  args.title    = "Resonant EQ"
  args.mnemonic = "RE"
  Unit.init(self, args)
end

function ResonantEQ:onLoadGraph(channelCount)
  local eq = self:addObject("eq", libreseq.ResonantEQ())

  connect(self, "In1", eq, "InL")
  if channelCount > 1 then
    connect(self, "In2", eq, "InR")
  end

  for i = 1, 10 do
    local p = self:addObject("band" .. i, app.GainBias())
    local r = self:addObject("band" .. i .. "Range", app.MinMax())
    p:hardSet("Bias", 0.0)
    connect(p, "Out", r, "In")
    connect(p, "Out", eq, "Band" .. i)
    self:addMonoBranch("band" .. i, p, "In", p, "Out")
  end

  local levelParam = self:addObject("levelParam", app.GainBias())
  local levelRange = self:addObject("levelRange", app.MinMax())
  levelParam:hardSet("Bias", 1.0)
  connect(levelParam, "Out", levelRange, "In")
  connect(levelParam, "Out", eq, "Level")
  self:addMonoBranch("level", levelParam, "In", levelParam, "Out")

  connect(eq, "OutL", self, "Out1")
  if channelCount > 1 then
    connect(eq, "OutR", self, "Out2")
  end
end

function ResonantEQ:onLoadViews(objects, branches)
  local controls = {}
  local expanded = {}

  for i = 1, 10 do
    local name = "band" .. i
    controls[name] = GainBias {
      button      = BAND_LABELS[i],
      description = BAND_DESC[i] .. "  cut ‹ flat › boost › resonate",
      branch      = branches[name],
      gainbias    = objects[name],
      range       = objects[name .. "Range"],
      biasMap     = Encoder.getMap("[-1,1]"),
      initialBias = 0.0,
      gainMap     = Encoder.getMap("[-1,1]"),
    }
    expanded[i] = name
  end

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
  expanded[11] = "level"

  -- Live band-value phosphor scope (Dirac/Varia-style), leading the strip.
  controls.bands = BandsView {
    name  = "bands",
    eq    = objects.eq,
    width = app.SECTION_PLY,
  }
  table.insert(expanded, 1, "bands")

  return controls, { expanded = expanded, collapsed = {} }
end

return ResonantEQ
