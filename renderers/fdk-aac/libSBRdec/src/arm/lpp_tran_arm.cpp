/* -----------------------------------------------------------------------------
Software License for The Fraunhofer FDK AAC Codec Library for Android

© Copyright  1995 - 2018 Fraunhofer-Gesellschaft zur Förderung der angewandten
Forschung e.V. All rights reserved.

 1.    INTRODUCTION
The Fraunhofer FDK AAC Codec Library for Android ("FDK AAC Codec") is software
that implements the MPEG Advanced Audio Coding ("AAC") encoding and decoding
scheme for digital audio. This FDK AAC Codec software is intended to be used on
a wide variety of Android devices.

AAC's HE-AAC and HE-AAC v2 versions are regarded as today's most efficient
general perceptual audio codecs. AAC-ELD is considered the best-performing
full-bandwidth communications codec by independent studies and is widely
deployed. AAC has been standardized by ISO and IEC as part of the MPEG
specifications.

Patent licenses for necessary patent claims for the FDK AAC Codec (including
those of Fraunhofer) may be obtained through Via Licensing
(www.vialicensing.com) or through the respective patent owners individually for
the purpose of encoding or decoding bit streams in products that are compliant
with the ISO/IEC MPEG audio standards. Please note that most manufacturers of
Android devices already license these patent claims through Via Licensing or
directly from the patent owners, and therefore FDK AAC Codec software may
already be covered under those patent licenses when it is used for those
licensed purposes only.

Commercially-licensed AAC software libraries, including floating-point versions
with enhanced sound quality, are also available from Fraunhofer. Users are
encouraged to check the Fraunhofer website for additional applications
information and documentation.

2.    COPYRIGHT LICENSE

Redistribution and use in source and binary forms, with or without modification,
are permitted without payment of copyright license fees provided that you
satisfy the following conditions:

You must retain the complete text of this software license in redistributions of
the FDK AAC Codec or your modifications thereto in source code form.

You must retain the complete text of this software license in the documentation
and/or other materials provided with redistributions of the FDK AAC Codec or
your modifications thereto in binary form. You must make available free of
charge copies of the complete source code of the FDK AAC Codec and your
modifications thereto to recipients of copies in binary form.

The name of Fraunhofer may not be used to endorse or promote products derived
from this library without prior written permission.

You may not charge copyright license fees for anyone to use, copy or distribute
the FDK AAC Codec software or your modifications thereto.

Your modified versions of the FDK AAC Codec must carry prominent notices stating
that you changed the software and the date of any change. For modified versions
of the FDK AAC Codec, the term "Fraunhofer FDK AAC Codec Library for Android"
must be replaced by the term "Third-Party Modified Version of the Fraunhofer FDK
AAC Codec Library for Android."

3.    NO PATENT LICENSE

NO EXPRESS OR IMPLIED LICENSES TO ANY PATENT CLAIMS, including without
limitation the patents of Fraunhofer, ARE GRANTED BY THIS SOFTWARE LICENSE.
Fraunhofer provides no warranty of patent non-infringement with respect to this
software.

You may use this FDK AAC Codec software or modifications thereto only for
purposes that are authorized by appropriate patent licenses.

4.    DISCLAIMER

This FDK AAC Codec software is provided by Fraunhofer on behalf of the copyright
holders and contributors "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
including but not limited to the implied warranties of merchantability and
fitness for a particular purpose. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE for any direct, indirect, incidental, special, exemplary,
or consequential damages, including but not limited to procurement of substitute
goods or services; loss of use, data, or profits, or business interruption,
however caused and on any theory of liability, whether in contract, strict
liability, or tort (including negligence), arising in any way out of the use of
this software, even if advised of the possibility of such damage.

5.    CONTACT INFORMATION

Fraunhofer Institute for Integrated Circuits IIS
Attention: Audio and Multimedia Departments - FDK AAC LL
Am Wolfsmantel 33
91058 Erlangen, Germany

www.iis.fraunhofer.de/amm
amm-info@iis.fraunhofer.de
----------------------------------------------------------------------------- */

/**************************** SBR decoder library ******************************

   Author(s):   Arthur Tritthart

   Description: (ARM optimised) LPP transposer subroutines

*******************************************************************************/

#if defined(__arm__)

#define FUNCTION_LPPTRANSPOSER_func1

#ifdef FUNCTION_LPPTRANSPOSER_func1

/* Note: This code requires only 43 cycles per iteration instead of 61 on
 * ARM926EJ-S */
static void lppTransposer_func1(FIXP_DBL *lowBandReal, FIXP_DBL *lowBandImag,
                                FIXP_DBL **qmfBufferReal,
                                FIXP_DBL **qmfBufferImag, int loops, int hiBand,
                                int dynamicScale, int descale, FIXP_SGL a0r,
                                FIXP_SGL a0i, FIXP_SGL a1r, FIXP_SGL a1i,
                                const int fPreWhitening,
                                FIXP_DBL preWhiteningGain,
                                int preWhiteningGains_sf) {
  FIXP_DBL real1, real2, imag1, imag2, accu1, accu2;

  real2 = lowBandReal[-2];
  real1 = lowBandReal[-1];
  imag2 = lowBandImag[-2];
  imag1 = lowBandImag[-1];
  for (int i = 0; i < loops; i++) {
    accu1 = fMultDiv2(a0r, real1);
    accu2 = fMultDiv2(a0i, imag1);
    accu1 = fMultAddDiv2(accu1, a1r, real2);
    accu2 = fMultAddDiv2(accu2, a1i, imag2);
    real2 = fMultDiv2(a1i, real2);
    accu1 = accu1 - accu2;
    accu1 = accu1 >> dynamicScale;

    accu2 = fMultAddDiv2(real2, a1r, imag2);
    real2 = real1;
    imag2 = imag1;
    accu2 = fMultAddDiv2(accu2, a0i, real1);
    real1 = lowBandReal[i];
    accu2 = fMultAddDiv2(accu2, a0r, imag1);
    imag1 = lowBandImag[i];
    accu2 = accu2 >> dynamicScale;

    accu1 <<= 1;
    accu2 <<= 1;
    accu1 += (real1 >> descale);
    accu2 += (imag1 >> descale);
    if (fPreWhitening) {
      accu1 = scaleValueSaturate(fMultDiv2(accu1, preWhiteningGain),
                                 preWhiteningGains_sf);
      accu2 = scaleValueSaturate(fMultDiv2(accu2, preWhiteningGain),
                                 preWhiteningGains_sf);
    }
    qmfBufferReal[i][hiBand] = accu1;
    qmfBufferImag[i][hiBand] = accu2;
  }
}
#endif /* #ifdef FUNCTION_LPPTRANSPOSER_func1 */

#endif /* __arm__ */
