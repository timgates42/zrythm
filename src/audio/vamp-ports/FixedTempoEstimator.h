/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2009 Chris Cannam and QMUL.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#ifndef _FIXED_TEMPO_ESTIMATOR_PLUGIN_H_
#define _FIXED_TEMPO_ESTIMATOR_PLUGIN_H_

#include "vamp-sdk/Plugin.h"

/**
 * Example plugin that estimates the tempo of a short fixed-tempo sample.
 */

class FixedTempoEstimator : public Vamp::Plugin
{
public:
  FixedTempoEstimator (float inputSampleRate);
  virtual ~FixedTempoEstimator ();

  bool initialise (
    size_t channels,
    size_t stepSize,
    size_t blockSize);
  void reset ();

  InputDomain getInputDomain () const
  {
    return FrequencyDomain;
  }

  std::string getIdentifier () const;
  std::string getName () const;
  std::string getDescription () const;
  std::string getMaker () const;
  int         getPluginVersion () const;
  std::string getCopyright () const;

  size_t getPreferredStepSize () const;
  size_t getPreferredBlockSize () const;

  ParameterList getParameterDescriptors () const;
  float getParameter (std::string id) const;
  void  setParameter (std::string id, float value);

  OutputList getOutputDescriptors () const;

  FeatureSet process (
    const float * const * inputBuffers,
    Vamp::RealTime        timestamp);

  FeatureSet getRemainingFeatures ();

protected:
  class D;
  D * m_d;
};

#endif
