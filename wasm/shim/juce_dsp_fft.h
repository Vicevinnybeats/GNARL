// GNARL: JUCE's fallback FFT for the browser build (wasm/README.md).
//
// Adapted from JUCE 6.0.5, modules/juce_dsp/frequency/juce_FFT.cpp
// (FFTFallback and its FFTConfig), which JUCE licenses under the GPL v3 among
// others. Copyright (c) 2020 - Raw Material Software Limited.
//
// Why a copy and not kissfft: the FFT the desktop runs on Windows and on
// Linux (without libfftw3f) is this one, and Vital's spectral wavetable
// morphing is sensitive to the FFT's ROUNDING - a keyframe's near-silent bins
// have phases that are rounding noise, the morph interpolates from them, and
// the loudest morphed frame sets the whole table's normalisation. With
// kissfft a custom wavetable came out 0.3 dB louder than the desktop's
// (tests/test_web.py); with the same algorithm, the same frames. Arithmetic
// is unchanged from JUCE's; only the JUCE types are replaced.
#pragma once

#include <cmath>
#include <complex>
#include <memory>
#include <vector>

namespace dsp {

  class FFT {
    public:
      explicit FFT(int order) :
          size_(1 << order), forward_(size_, false), inverse_(size_, true), scratch_(static_cast<size_t>(size_)) { }

      int getSize() const { return size_; }

      // JUCE's FFTFallback::performRealOnlyForwardTransform: all N complex bins
      // interleaved in d[0..2N).
      void performRealOnlyForwardTransform(float* d, bool = false) {
        if (size_ == 1)
          return;
        for (int i = 0; i < size_; ++i)
          scratch_[i] = { d[i], 0.0f };
        forward_.perform(scratch_.data(), reinterpret_cast<std::complex<float>*>(d));
      }

      // JUCE's FFTFallback::performRealOnlyInverseTransform.
      void performRealOnlyInverseTransform(float* d) {
        if (size_ == 1)
          return;
        auto* input = reinterpret_cast<std::complex<float>*>(d);
        for (int i = size_ >> 1; i < size_; ++i)
          input[i] = std::conj(input[size_ - i]);

        inverse_.perform(input, scratch_.data());
        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i)
          scratch_[i] *= scale;

        for (int i = 0; i < size_; ++i) {
          d[i] = scratch_[i].real();
          d[i + size_] = scratch_[i].imag();
        }
      }

    private:
      typedef std::complex<float> Complex;

      // JUCE's FFTFallback::FFTConfig, unchanged in its arithmetic.
      struct Config {
        struct Factor { int radix, length; };

        Config(int size, bool inverse) : fft_size(size), is_inverse(inverse), twiddles(static_cast<size_t>(size)) {
          double inverse_factor = (inverse ? 2.0 : -2.0) * 3.141592653589793238 / static_cast<double>(size);

          if (size <= 4) {
            for (int i = 0; i < size; ++i) {
              double phase = i * inverse_factor;
              twiddles[i] = { static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)) };
            }
          }
          else {
            for (int i = 0; i < size / 4; ++i) {
              double phase = i * inverse_factor;
              twiddles[i] = { static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)) };
            }
            for (int i = size / 4; i < size / 2; ++i) {
              Complex other = twiddles[i - size / 4];
              twiddles[i] = { inverse ? -other.imag() : other.imag(), inverse ? other.real() : -other.real() };
            }
            twiddles[size / 2] = { -1.0f, 0.0f };
            for (int i = size / 2; i < size; ++i) {
              int index = size / 2 - (i - size / 2);
              twiddles[i] = std::conj(twiddles[index]);
            }
          }

          int root = static_cast<int>(std::sqrt(static_cast<double>(size)));
          int divisor = 4, n = size;
          for (int i = 0; i < 32; ++i) {
            while ((n % divisor) != 0) {
              if (divisor == 2) divisor = 3;
              else if (divisor == 4) divisor = 2;
              else divisor += 2;
              if (divisor > root)
                divisor = n;
            }
            n /= divisor;
            factors[i].radix = divisor;
            factors[i].length = n;
          }
        }

        void perform(const Complex* input, Complex* output) const { perform(input, output, 1, 1, factors); }

        void perform(const Complex* input, Complex* output, int stride, int stride_in, const Factor* facs) const {
          Factor factor = *facs++;
          Complex* original_output = output;
          Complex* output_end = output + factor.radix * factor.length;

          if (stride == 1 && factor.radix <= 5) {
            for (int i = 0; i < factor.radix; ++i)
              perform(input + stride * stride_in * i, output + i * factor.length, stride * factor.radix, stride_in, facs);
            butterfly(factor, output, stride);
            return;
          }

          if (factor.length == 1) {
            do {
              *output++ = *input;
              input += stride * stride_in;
            } while (output < output_end);
          }
          else {
            do {
              perform(input, output, stride * factor.radix, stride_in, facs);
              input += stride * stride_in;
              output += factor.length;
            } while (output < output_end);
          }
          butterfly(factor, original_output, stride);
        }

        void butterfly(const Factor factor, Complex* data, int stride) const {
          switch (factor.radix) {
            case 1: return;
            case 2: butterfly2(data, stride, factor.length); return;
            case 4: butterfly4(data, stride, factor.length); return;
            default: break;
          }
          // Radix 3 or 5 cannot occur for a power-of-two size, which is all
          // Vital asks for; JUCE's generic path is left out.
        }

        void butterfly2(Complex* data, const int stride, const int length) const {
          Complex* data_end = data + length;
          const Complex* tw = twiddles.data();
          for (int i = length; --i >= 0;) {
            Complex s = *data_end;
            s *= (*tw);
            tw += stride;
            *data_end++ = *data - s;
            *data++ += s;
          }
        }

        void butterfly4(Complex* data, const int stride, const int length) const {
          int length_x2 = length * 2;
          int length_x3 = length * 3;
          int stride_x2 = stride * 2;
          int stride_x3 = stride * 3;
          const Complex* twiddle1 = twiddles.data();
          const Complex* twiddle2 = twiddle1;
          const Complex* twiddle3 = twiddle1;

          for (int i = length; --i >= 0;) {
            Complex s0 = data[length] * *twiddle1;
            Complex s1 = data[length_x2] * *twiddle2;
            Complex s2 = data[length_x3] * *twiddle3;
            Complex s3 = s0; s3 += s2;
            Complex s4 = s0; s4 -= s2;
            Complex s5 = *data; s5 -= s1;

            *data += s1;
            data[length_x2] = *data;
            data[length_x2] -= s3;
            twiddle1 += stride;
            twiddle2 += stride_x2;
            twiddle3 += stride_x3;
            *data += s3;

            if (is_inverse) {
              data[length] = { s5.real() - s4.imag(), s5.imag() + s4.real() };
              data[length_x3] = { s5.real() + s4.imag(), s5.imag() - s4.real() };
            }
            else {
              data[length] = { s5.real() + s4.imag(), s5.imag() - s4.real() };
              data[length_x3] = { s5.real() - s4.imag(), s5.imag() + s4.real() };
            }
            ++data;
          }
        }

        const int fft_size;
        const bool is_inverse;
        Factor factors[32];
        std::vector<Complex> twiddles;
      };

      int size_;
      Config forward_;
      Config inverse_;
      std::vector<Complex> scratch_;
  };

} // namespace dsp
