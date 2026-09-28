#include <assert.h>
#include <stdio.h>
#include "cv_input_detect.h"
static const uint8_t pattern[3][16] = {
 {0,1,1,0,0,1,0,1,1,0,1,0,0,1,1,0},
 {1,0,1,1,0,0,1,0,0,1,0,1,1,0,0,1},
 {0,0,1,0,1,1,1,0,1,0,0,1,0,1,1,1}
};
int main(void) {
 uint16_t samples[16], strength;
 for(unsigned p=0;p<3;++p) {
  // Different offsets, including the live 500/760 empty-jack response.
  for(unsigned base=0;base<=750;base+=50) {
   for(unsigned i=0;i<16;++i) samples[i]=base+pattern[p][i]*256+(i%3);
   assert(cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
   assert(strength>=127 && strength<=129);
  }
  // DC input / unplugged patch cable: no correlation with our test pattern.
  for(unsigned dc=0;dc<=1023;++dc) {
   for(unsigned i=0;i<16;++i) samples[i]=dc;
   assert(!cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
  }
  for(unsigned i=0;i<16;++i) samples[i]=500+pattern[p][i]*256;
  samples[0]=samples[1]=628; // Two ambiguous bits are allowed.
  assert(cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
  samples[2]=628;
  assert(!cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
  for(unsigned i=0;i<16;++i) samples[i]=500+pattern[p][i]*80;
  assert(!cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
  for(unsigned i=0;i<16;++i) samples[i]=760-pattern[p][i]*256;
  assert(!cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
  for(unsigned i=0;i<16;++i) samples[i]=i*64;
  assert(!cv_input_follows_pattern(samples,pattern[p],16,50,2,&strength));
 }
 puts("CV pattern detection: offset drift, DC rejection, ambiguity, weak/inverted/unrelated signals passed");
}
