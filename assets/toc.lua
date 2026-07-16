-- toc.lua — package manifest for reseq v0.6.1
return {
  name    = "reseq",
  title   = "Resonant EQ",
  author  = "nickb808",
  version = "0.6.1",
  units   = {
    { title = "Resonant EQ",    moduleName = "ResonantEQ" },    -- stereo, main only
    { title = "Resonant EQ MO", moduleName = "ResonantEQMO" },  -- mono, combs + per-comb feedback
    { title = "Resonant EQ AN", moduleName = "ResonantEQAN" },  -- mono analyzer, Main + 10 env outs
  },
}
