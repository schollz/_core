(
s.waitForBoot{
	{
		var snd;
		var count;
		snd = SoundIn.ar(0);
		snd = Pan2.ar(snd);
		count = Stepper.kr(Impulse.kr(1),max:10000000000);
		count.poll;
		DetectSilence.ar(snd,48.neg.dbamp,1,doneAction:2);
		Out.ar(0,snd);
	}.play;
}
)