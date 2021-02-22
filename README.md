# OceanFFT

Realistic ocean wave simulation, primarily based on [J. Tessendorf's paper](https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.161.9102&rep=rep1&type=pdf), using OpenGL compute shaders.
Checkout the demo video [here](https://youtu.be/zAtOXnCkMaw).

![demo_image](https://user-images.githubusercontent.com/35342732/108680218-96d06400-7513-11eb-88b8-5014d2df6c56.jpg)

In the quest for more realism, a [different spectrum](https://archimer.ifremer.fr/doc/00091/20226/17877.pdf) is used than the Phillips spectrum mentioned in the original paper. Stockham formulation of FFT is used to better map the problem to the GPU and avoid expensive bit-reversal operation required for the vanilla Cooley-Tukey algorithm.

## Build Instructions

1. Clone recursively with submodules via `git clone --recurse-submodules git://github.com/achalpandeyy/OceanFFT.git`
2. Run CMake in the root directory

## Dependencies

Uses my OpenGL framework [Ogle](https://github.com/achalpandeyy/Ogle) as a submodule.

## References
- For ocean physics: [Fynn-Jorin Flügge's thesis](https://tore.tuhh.de/handle/11420/1439?locale=en), [Jump Trajectory's video for Unity](https://www.youtube.com/watch?v=kGEqaX4Y4bQ&t=179s&ab_channel=JumpTrajectory), [dli/waves](https://github.com/dli/waves)

- For FFT and the math behind it: [Eric Bainville's blog](http://www.bealto.com/gpu-fft2.html), [Introduction to the Stockham FFT](http://wwwa.pikara.ne.jp/okojisan/otfft-en/stockham1.html), [3b1b's video on Fourier Transform](https://youtu.be/spUNpyF58BY), [Reducible's video on FFT](https://youtu.be/h7apO7q16V0)