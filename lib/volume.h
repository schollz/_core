#ifndef VOLUME_H
#define VOLUME_H 1

/*
(
var divisor = 1;
var start = -36;
var start2 = -57;
var end = 0;
var numberOfSteps = 192;
var cutoff = 160;
var increment = (end-start)/numberOfSteps;
var increment2 = (end-start2)/numberOfSteps;
var array = Array.series(numberOfSteps, start, increment);
var array2 = Array.series(numberOfSteps, start2, increment2);
array.postln;
("#define VOLUME_DIVISOR "++divisor.asInteger).postln;
("#define VOLUME_STEPS "++numberOfSteps.asInteger).postln;
("const int32_t volume_vals["++numberOfSteps.asInteger++"] = {").postln;
array.do({ arg v,i;
        if (i<1,{
                "0,".postln;
        },{
                if (i<cutoff,{
                        v = (array2[i] * (cutoff-i) / cutoff) + (v *i / cutoff);
                });
                (((65536*(v.dbamp)*divisor).round.asInteger.asString)++",").postln;

        });
});
"};".postln;
)
*/
#define VOLUME_DIVISOR 1
#define VOLUME_STEPS 192
const int32_t volume_vals[192] = {
    0,     97,    102,   107,   113,   118,   124,   130,   137,   143,   150,
    158,   165,   173,   182,   190,   200,   209,   219,   230,   240,   252,
    264,   276,   289,   302,   316,   331,   346,   362,   378,   396,   414,
    432,   452,   472,   493,   515,   538,   561,   586,   612,   639,   666,
    695,   725,   757,   789,   823,   858,   894,   932,   971,   1012,  1054,
    1098,  1143,  1190,  1239,  1290,  1342,  1397,  1453,  1512,  1572,  1635,
    1700,  1767,  1837,  1909,  1984,  2061,  2141,  2223,  2309,  2397,  2488,
    2583,  2681,  2781,  2886,  2993,  3104,  3219,  3338,  3460,  3586,  3716,
    3851,  3989,  4132,  4280,  4432,  4588,  4750,  4916,  5087,  5264,  5445,
    5633,  5825,  6023,  6227,  6437,  6653,  6876,  7104,  7339,  7580,  7828,
    8083,  8345,  8614,  8891,  9175,  9466,  9765,  10072, 10387, 10710, 11042,
    11382, 11730, 12088, 12454, 12829, 13214, 13608, 14011, 14425, 14848, 15281,
    15724, 16177, 16641, 17116, 17601, 18097, 18605, 19123, 19653, 20195, 20748,
    21312, 21889, 22478, 23079, 23692, 24318, 24957, 25608, 26272, 26949, 27639,
    28342, 29059, 29789, 30533, 31290, 32061, 32846, 33563, 34295, 35043, 35808,
    36589, 37388, 38204, 39037, 39889, 40760, 41649, 42558, 43487, 44435, 45405,
    46396, 47408, 48443, 49500, 50580, 51684, 52812, 53964, 55142, 56345, 57574,
    58831, 60115, 61426, 62767, 64136,
};

#endif