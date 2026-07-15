-- toc.lua — package manifest for reseq v0.4.0
return {
  name    = "reseq",
  title   = "Resonant EQ",
  version = "0.4.0",
  units   = {
    { title = "Resonant EQ",    moduleName = "ResonantEQ" },    -- stereo, main only
    { title = "Resonant EQ MO", moduleName = "ResonantEQMO" },  -- mono, combs + per-comb feedback
  },
}
