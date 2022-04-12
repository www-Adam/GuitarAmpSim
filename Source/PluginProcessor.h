/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define inputId "input"
#define inputName "Input"

#define driveId "drive"
#define driveName "Drive"

#define lowId "low"
#define lowName "Low"

#define midId "mid"
#define midName "Mid"

#define highId "high"
#define highName "High"

#define outputId "output"
#define outputName "Output"

//==============================================================================
/**
*/
class MouzAudioProcessor : public juce::AudioProcessor, juce::AudioProcessorValueTreeState::Listener
{
public:
	//==============================================================================
	MouzAudioProcessor();
	~MouzAudioProcessor() override;

	//==============================================================================
	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

	void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	//==============================================================================
	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;

	//==============================================================================
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram(int index) override;
	const juce::String getProgramName(int index) override;
	void changeProgramName(int index, const juce::String& newName) override;

	//==============================================================================
	void getStateInformation(juce::MemoryBlock& destData) override;
	void setStateInformation(const void* data, int sizeInBytes) override;

	//==============================================================================
	void updateHighPassFilter(const float& freq);
	void updatePreClipFilter(const float& freq);
	void updateLowFilter(const float& gain);
	void updateMidFilter(const float& gain);
	void updateHighFilter(const float& gain);

	//==============================================================================
	juce::AudioProcessorValueTreeState treeState;

private:
	
	float rawInput;
	float rawDrive;
	float rawLow;
	float rawMid;
	float rawHigh;
	float rawOutput;
	
	double lastSampleRate;
	double projectSampleRate{ 48000.0 };

	void setAllSampleRates(float value);

	// Static filters ==============================================================
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highPassFilter;
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> preClipFilter;

	// Tone filters ================================================================
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowFilter;
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> midFilter;
	juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highFilter;

	// Input & Output gain + oversampling ==========================================
	juce::dsp::Gain<float> inputGain;
	juce::dsp::Gain<float> outputGain;
	juce::dsp::Oversampling<float> oversamplingProcessor;

	// Parameters ==================================================================
	juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
	void parameterChanged(const juce::String& parameterID, float newValue) override;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MouzAudioProcessor)
};
