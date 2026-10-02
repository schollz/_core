// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../lib/core_engine/core_engine.h"
#include "../../lib/start_tempo.h"
#include <jansson.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>
#include <cctype>
#include <utility>

namespace ecto {
// Named fields, not a memory dump: portable between CPU architectures and
// future engine layouts. New fields retain the engine's defaults on old patches.
#define CORE_STATE_NUMBERS(X) \
 X(version) X(tempo) X(start_tempo) X(volume) X(bank) X(slot) X(rune) X(division) X(trigger_mode) X(pitch) \
 X(brightness) X(amen_behavior) X(sample_mapping) X(reset_input) X(amiga) X(saturation) \
 X(smear) X(jitter) X(chaos) X(jump_probability) X(retrigger_probability) X(filter_index) X(filter_type) X(sequence_length)
#define CORE_STATE_BOOLS(X) X(stopped) X(muted) X(stay_in_sync) X(clock_stop) X(clock_trigger) X(clock_slice)
inline json_t *stateJson(const CoreState &s) {
    auto *j=json_object();
#define NUMBER(f) json_object_set_new(j,#f,json_integer(s.f));
    CORE_STATE_NUMBERS(NUMBER)
#undef NUMBER
#define BOOLEAN(f) json_object_set_new(j,#f,json_boolean(s.f));
    CORE_STATE_BOOLS(BOOLEAN)
#undef BOOLEAN
    auto array=[&](const char *name,auto *data,size_t count){auto *a=json_array();for(size_t i=0;i<count;++i)json_array_append_new(a,json_integer(data[i]));json_object_set_new(j,name,a);};
    array("effects",s.effects,16);array("effect_params",&s.effect_params[0][0],48);
    array("bipolar",s.bipolar,3);array("runes",&s.runes[0][0],112);array("sequence",s.sequence,64);
    char text[32];snprintf(text,sizeof text,"%016llx",(unsigned long long)s.random_state);json_object_set_new(j,"random_state",json_string(text));
    snprintf(text,sizeof text,"%016llx",(unsigned long long)s.random_increment);json_object_set_new(j,"random_increment",json_string(text));return j;
}
inline bool stateFromJson(json_t *j,CoreState &s) {
    if(!json_is_object(j))return false;
#define NUMBER(f) if(auto *v=json_object_get(j,#f)){if(!json_is_integer(v))return false;auto n=json_integer_value(v);if(n<std::numeric_limits<decltype(s.f)>::min()||n>std::numeric_limits<decltype(s.f)>::max())return false;s.f=decltype(s.f)(n);}
    CORE_STATE_NUMBERS(NUMBER)
#undef NUMBER
#define BOOLEAN(f) if(auto *v=json_object_get(j,#f)){if(!json_is_boolean(v))return false;s.f=json_is_true(v);}
    CORE_STATE_BOOLS(BOOLEAN)
#undef BOOLEAN
    auto array=[&](const char *name,auto *data,size_t count,int max){auto *a=json_object_get(j,name);if(!a)return true;if(!json_is_array(a)||json_array_size(a)!=count)return false;for(size_t i=0;i<count;++i){auto *v=json_array_get(a,i);auto n=json_integer_value(v);if(!json_is_integer(v)||n<0||n>max)return false;data[i]=n;}return true;};
    if(!array("effects",s.effects,16,1)||!array("effect_params",&s.effect_params[0][0],48,255)||!array("bipolar",s.bipolar,3,1)||!array("runes",&s.runes[0][0],112,1)||!array("sequence",s.sequence,64,64))return false;
    for(auto pair:{std::pair<const char*,uint64_t*>("random_state",&s.random_state),{"random_increment",&s.random_increment}}){
        if(auto *v=json_object_get(j,pair.first)){auto *text=json_string_value(v);if(!text||strlen(text)!=16)return false;for(unsigned i=0;i<16;++i)if(!std::isxdigit(static_cast<unsigned char>(text[i])))return false;char *end;auto n=strtoull(text,&end,16);if(*end)return false;*pair.second=n;}
    }
    return start_tempo_valid(s.start_tempo)&&s.tempo>=30&&s.tempo<=300&&s.version==1&&s.bank<16&&s.slot<16&&s.rune<7&&s.trigger_mode<4&&s.sample_mapping<2&&s.amen_behavior<3&&s.reset_input>=-1&&s.reset_input<=3&&s.sequence_length<=64;
}
}
