import("stdfaust.lib");
freq = nentry("freq[unit:Hz]", 440, 20, 8000, 1);
gain = nentry("gain", 0.5, 0, 1, 0.01);
gate = button("gate");
wave = hslider("osc/wave[style:radio{'SAW':0;'SQR':1;'TRI':2}]", 0, 0, 2, 1);
env = en.adsr(0.005, 0.12, 0.7, 0.25, gate);
osc = os.sawtooth(freq), os.square(freq), os.triangle(freq) : ba.selectn(3, wave);
process = osc * env * gain <: _,_;
declare options "[midi:on]";
