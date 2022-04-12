/*
  ==============================================================================

	This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MouzAudioProcessor::MouzAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
	),
	treeState(*this, nullptr, "PARAMETERS", createParameterLayout())

#endif
{
	treeState.addParameterListener(inputId, this);
	treeState.addParameterListener(driveId, this);
	treeState.addParameterListener(lowId, this);
	treeState.addParameterListener(midId, this);
	treeState.addParameterListener(highId, this);
	treeState.addParameterListener(outputId, this);
}

MouzAudioProcessor::~MouzAudioProcessor()
{
	treeState.removeParameterListener(inputId, this);
	treeState.removeParameterListener(driveId, this);
	treeState.removeParameterListener(lowId, this);
	treeState.removeParameterListener(midId, this);
	treeState.removeParameterListener(highId, this);
	treeState.removeParameterListener(outputId, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MouzAudioProcessor::createParameterLayout()
{
	std::vector <std::unique_ptr<juce::RangedAudioParameter>> params;
	params.reserve(6);

	auto pInput = std::make_unique<juce::AudioParameterFloat>(inputId, inputName, -24.0, 24.0, 0.0);
	auto pDrive = std::make_unique<juce::AudioParameterFloat>(driveId, driveName, 0.0, 10.0, 0.0);
	auto pLow = std::make_unique<juce::AudioParameterFloat>(lowId, lowName, -6.0, 6.0, 0.0);
	auto pMid = std::make_unique<juce::AudioParameterFloat>(midId, midName, -6.0, 6.0, 0.0);
	auto pHigh = std::make_unique<juce::AudioParameterFloat>(highId, highName, -6.0, 6.0, 0.0);
	auto pOutput = std::make_unique<juce::AudioParameterFloat>(outputId, outputName, -24.0, 24.0, 0.0);

	params.push_back(std::move(pInput));
	params.push_back(std::move(pDrive));
	params.push_back(std::move(pLow));
	params.push_back(std::move(pMid));
	params.push_back(std::move(pHigh));
	params.push_back(std::move(pOutput));

	return { params.begin(), params.end() };
}

void MouzAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
	if (parameterID == inputId) {
		rawInput = newValue;
		inputGain.setGainDecibels(rawInput);
		DBG("Input is: " << newValue);
	}
	else if (parameterID == driveId) {
		rawDrive = pow(10, newValue * 0.25);
		DBG("Drive is: " << newValue);
	}
	else if (parameterID == lowId) {
		rawLow = newValue;
		updateLowFilter(rawLow);
		DBG("Low is: " << newValue);
	}
	else if (parameterID == midId) {
		rawMid = newValue;
		updateMidFilter(rawMid);
		DBG("Mid is: " << newValue);
	}
	else if (parameterID == highId) {
		rawHigh = newValue;
		updateHighFilter(rawHigh);
		DBG("High is: " << newValue);
	}
	else if (parameterID == outputId) {
		rawOutput = newValue;
		outputGain.setGainDecibels(rawOutput);
		DBG("Output is: " << newValue);
	}
}

//==============================================================================
const juce::String MouzAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool MouzAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool MouzAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool MouzAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double MouzAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int MouzAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int MouzAudioProcessor::getCurrentProgram()
{
	return 0;
}

void MouzAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String MouzAudioProcessor::getProgramName(int index)
{
	return {};
}

void MouzAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void MouzAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	juce::dsp::ProcessSpec spec;
	spec.maximumBlockSize = samplesPerBlock;
	spec.sampleRate = sampleRate * oversamplingProcessor.getOversamplingFactor();
	spec.numChannels = getTotalNumOutputChannels();
	lastSampleRate = spec.sampleRate;
	projectSampleRate = sampleRate;

	highPassFilter.prepare(spec);
	highPassFilter.reset();
	*highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(projectSampleRate, 200);

	preClipFilter.prepare(spec);
	preClipFilter.reset();
	*preClipFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(projectSampleRate, 1500.0, 0.5, 6.0);

	lowFilter.prepare(spec);
	lowFilter.reset();
	*lowFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(projectSampleRate, 200, 0.5, pow(10, *treeState.getRawParameterValue(lowId) * 0.05));

	midFilter.prepare(spec);
	midFilter.reset();
	*midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(projectSampleRate, 1000.0, 0.5, pow(10, *treeState.getRawParameterValue(midId) * 0.05));

	highFilter.prepare(spec);
	highFilter.reset();
	*highFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(projectSampleRate, 4000, 0.333, pow(10, *treeState.getRawParameterValue(highId) * 0.05));

	inputGain.prepare(spec);
	inputGain.reset();
	inputGain.setGainDecibels(*treeState.getRawParameterValue(inputId));

	outputGain.prepare(spec);
	outputGain.reset();
	outputGain.setGainDecibels(*treeState.getRawParameterValue(outputId));

	rawDrive = pow(10, *treeState.getRawParameterValue(driveId) * 0.25);
}

void MouzAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MouzAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	juce::ignoreUnused(layouts);
	return true;
#else
	// This is the place where you check if the layout is supported.
	// In this template code we only support mono or stereo.
	// Some plugin hosts, such as certain GarageBand versions, will only
	// load plugins that support stereo bus layouts.
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
		&& layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
#endif

	return true;
#endif
}
#endif

void MouzAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear(i, 0, buffer.getNumSamples());

	juce::dsp::AudioBlock<float> block(buffer);

	inputGain.process(juce::dsp::ProcessContextReplacing<float>(block));
	highPassFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
	preClipFilter.process(juce::dsp::ProcessContextReplacing<float>(block));

	for (int channel = 0; channel < totalNumInputChannels; ++channel)
	{
		//auto* channelData = block.getChannelPointer(channel);
		auto* inputData = buffer.getReadPointer(channel);
		auto* outputData = buffer.getWritePointer(channel);

		for (int sample = 0; sample < block.getNumSamples(); ++sample)
		{
			outputData[sample] = (3.1415926535 / 2.0) * atan((exp((0.1 * inputData[sample]) / (0.0253 * 1.68)) - 1) * (rawDrive * 16));
		}
	}

	lowFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
	midFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
	highFilter.process(juce::dsp::ProcessContextReplacing<float>(block));
	outputGain.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void MouzAudioProcessor::setAllSampleRates(float value)
{
	*highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(value, 200);
	*preClipFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(value, 1500.0, 0.5, 6.0);
	*lowFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(value, 200, 0.5, pow(10, *treeState.getRawParameterValue(lowId) * 0.05));
	*midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(value, 1000.0, 0.5, pow(10, *treeState.getRawParameterValue(midId) * 0.05));
	*highFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(value, 4000, 0.333, pow(10, *treeState.getRawParameterValue(highId) * 0.05));
}

void MouzAudioProcessor::updateHighPassFilter(const float& freq) {
	*highPassFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(lastSampleRate, 200);
}

void MouzAudioProcessor::updatePreClipFilter(const float& freq) {
	*preClipFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(lastSampleRate, 1500, 0.5, 6.0);
}

void MouzAudioProcessor::updateLowFilter(const float& gain) {
	*lowFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(lastSampleRate, 200.0, 0.5, pow(10, gain * 0.05));
}

void MouzAudioProcessor::updateMidFilter(const float& gain) {
	*midFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(lastSampleRate, 1000.0, 0.5, pow(10, gain * 0.05));
};

void MouzAudioProcessor::updateHighFilter(const float& gain) {
	*highFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(lastSampleRate, 4000.0, 0.333, pow(10, gain * 0.05));
}

//==============================================================================
bool MouzAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MouzAudioProcessor::createEditor()
{
	//return new MouzAudioProcessorEditor(*this);
	return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void MouzAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
	// Save Parameters
	juce::MemoryOutputStream stream(destData, false);
	treeState.state.writeToStream(stream);
}

void MouzAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	// Recall Parameters
	auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));

	if (tree.isValid())
	{
		treeState.state = tree;
	}
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new MouzAudioProcessor();
}
