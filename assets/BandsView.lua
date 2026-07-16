-- BandsView — display-only ViewControl hosting the Resonant EQ band-value
-- phosphor scope. Ten bipolar bars (29 Hz … 11 kHz left→right): up = boost
-- into resonance (crest hits full phosphor past the +0.5 ring line), down =
-- cut into the cancellation notch. Trails as band CVs move. Modeled on
-- Varia's SlopeView / Dirac's GrainFieldView (one control-width phosphor viz,
-- no editable parameters).
--
-- Usage: BandsView { name = "bands", eq = <ResonantEQ> } or
--        BandsView { name = "bands", mo = <ResonantEQMO> }

local app = app
local Class = require "Base.Class"
local ViewControl = require "Unit.ViewControl"
local libreseq = require "reseq.libreseq"
local ply = app.SECTION_PLY

local BandsView = Class {}
BandsView:include(ViewControl)

function BandsView:init(args)
  ViewControl.init(self, args.name or "bands")
  self:setClassName("reseq.BandsView")
  local eq = args.eq or args.mo
      or app.logError("%s.init: eq/mo is missing.", self)
  local width = args.width or ply   -- one control-width slot, like Varia/Dirac

  self.bands = libreseq.BandsGraphic(0, 0, width, 64)
  if args.eq then
    self.bands:follow(args.eq)
  else
    self.bands:followMO(args.mo)
  end

  local graphic = app.Graphic(0, 0, width, 64)
  graphic:addChild(self.bands)
  self:setControlGraphic(graphic)
  self:setMainCursorController(self.bands)

  for i = 1, (width // ply) do
    self:addSpotDescriptor{ center = (i - 0.5) * ply }
  end

  -- Sub display: Daisaku Ikeda epigraph (display has no editable parameters).
  self.subGraphic = app.Graphic(0, 0, 128, 64)
  local lines = {
    { "When resonant harmonies", 54 },   -- y descending from the top
    { "arise between this vast",  45 },
    { "outer cosmos and the",     36 },
    { "inner human cosmos,",      27 },
    { "poetry is born.",          18 },
    { "- Daisaku Ikeda",           9 },
  }
  for _, ln in ipairs(lines) do
    local label = app.Label(ln[1], 8)
    label:fitToText(0)
    label:setCenter(64, ln[2])
    self.subGraphic:addChild(label)
  end
end

return BandsView
