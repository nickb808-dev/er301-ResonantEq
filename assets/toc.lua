-- toc.lua — package manifest for reseq v0.5.2
return {
  name    = "reseq",
  title   = "Resonant EQ",
  version = "0.5.2",
  units   = {
    { title = "Resonant EQ",    moduleName = "ResonantEQ" },    -- stereo, main only
    { title = "Resonant EQ MO", moduleName = "ResonantEQMO" },  -- mono, combs + per-comb feedback
  },
}
