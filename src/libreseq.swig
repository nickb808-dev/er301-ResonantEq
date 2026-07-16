/* libreseq.swig — SWIG interface for the Resonant EQ ER-301 Objects.
 * Three units: ResonantEQ (stereo), ResonantEQMO (mono, multi-out + feedback),
 * ResonantEQAN (mono analyzer: Main + 10 per-band envelope followers), plus
 * BandsGraphic (phosphor band-value scope, followable from any unit). */

%module reseq_libreseq

%include <od/glue/mod.cpp.swig>

%{
#undef SWIGLUA
#include <ResonantEQ.h>
#include <ResonantEQMO.h>
#include <ResonantEQAN.h>
#include <BandsGraphic.h>
#define SWIGLUA
using namespace reseq;
%}

%include <ResonantEQ.h>
%include <ResonantEQMO.h>
%include <ResonantEQAN.h>
%include <BandsGraphic.h>
