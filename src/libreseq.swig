/* libreseq.swig — SWIG interface for the Resonant EQ ER-301 Objects.
 * Two units: ResonantEQ (stereo) and ResonantEQMO (mono, multi-out + feedback),
 * plus BandsGraphic (phosphor band-value scope, followable from either unit). */

%module reseq_libreseq

%include <od/glue/mod.cpp.swig>

%{
#undef SWIGLUA
#include <ResonantEQ.h>
#include <ResonantEQMO.h>
#include <BandsGraphic.h>
#define SWIGLUA
using namespace reseq;
%}

%include <ResonantEQ.h>
%include <ResonantEQMO.h>
%include <BandsGraphic.h>
