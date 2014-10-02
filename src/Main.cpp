//------------------------------------------------------------------------------
//
// Copyright 2013-2014 BBC Research and Development
//
// Author: Chris Needham
//
// This file is part of Audio Waveform Image Generator.
//
// Audio Waveform Image Generator is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// Audio Waveform Image Generator is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// Audio Waveform Image Generator.  If not, see <http://www.gnu.org/licenses/>.
//
//------------------------------------------------------------------------------

#include "AudioFileReader.h"
#include "Config.h"
#include "MonoConverter.h"
#include "Mp3AudioFileReader.h"
#include "Options.h"
#include "SndFileAudioFileReader.h"
#include "WaveformBuffer.h"
#include "WaveformColors.h"
#include "WaveformGenerator.h"
#include "WaveformRescaler.h"
#include "WavFileWriter.h"

#include <boost/filesystem.hpp>

#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

//------------------------------------------------------------------------------

static_assert(std::numeric_limits<int>::max() >= 2147483647L, "size of int");

//------------------------------------------------------------------------------

std::ostream& output_stream = std::cout;
std::ostream& error_stream  = std::cerr;

//------------------------------------------------------------------------------

static std::unique_ptr<AudioFileReader> createAudioFileReader(
    const boost::filesystem::path& ext)
{
    if (ext == ".wav" || ext == ".flac") {
        return std::unique_ptr<AudioFileReader>(new SndFileAudioFileReader);
    }
    else if (ext == ".mp3") {
        return std::unique_ptr<AudioFileReader>(new Mp3AudioFileReader);
    }
    else {
        return std::unique_ptr<AudioFileReader>(nullptr);
    }
}

//------------------------------------------------------------------------------

static bool convertAudioFormat(
    const boost::filesystem::path& input_filename,
    const boost::filesystem::path& output_filename)
{
    Mp3AudioFileReader reader;

    if (!reader.open(input_filename.c_str())) {
        return false;
    }

    WavFileWriter writer(output_filename.c_str());

    return reader.run(writer);
}

//------------------------------------------------------------------------------

static bool generateWaveformData(
    const boost::filesystem::path& input_filename,
    const boost::filesystem::path& output_filename,
    const int samples_per_pixel,
    const int bits)
{
    const boost::filesystem::path input_file_ext  = input_filename.extension();
    const boost::filesystem::path output_file_ext = output_filename.extension();

    const std::unique_ptr<AudioFileReader> audio_file_reader =
        createAudioFileReader(input_file_ext);

    if (audio_file_reader == nullptr) {
        error_stream << "Unknown file type: " << input_filename << '\n';
        return false;
    }

    if (!audio_file_reader->open(input_filename.c_str())) {
        return false;
    }

    WaveformBuffer buffer;
    FixedScaleFactor scale_factor(samples_per_pixel);
    WaveformGenerator processor(buffer, scale_factor);

    if (!audio_file_reader->run(processor)) {
        return false;
    }

    assert(output_file_ext == ".dat" || output_file_ext == ".json");

    if (output_file_ext == ".dat") {
        return buffer.save(output_filename.c_str(), bits);
    }
    else {
        return buffer.saveAsJson(output_filename.c_str(), bits);
    }
}

//------------------------------------------------------------------------------

static bool convertWaveformData(
    const boost::filesystem::path& input_filename,
    const boost::filesystem::path& output_filename)
{
    WaveformBuffer buffer;

    if (!buffer.load(input_filename.c_str())) {
        return false;
    }

    bool success = true;

    const boost::filesystem::path output_file_ext = output_filename.extension();

    if (output_file_ext == ".json") {
        success = buffer.saveAsJson(output_filename.c_str());
    }
    else if (output_file_ext == ".txt") {
        success = buffer.saveAsText(output_filename.c_str());
    }

    return success;
}

//------------------------------------------------------------------------------

int main(int argc, const char* const* argv)
{
    Options options;

    if (!options.parseCommandLine(argc, argv)) {
        return 1;
    }
    else if (options.getHelp()) {
        options.showUsage(output_stream);
        return 0;
    }
    else if (options.getVersion()) {
        options.showVersion(output_stream);
        return 0;
    }

    const boost::filesystem::path input_filename  = options.getInputFilename();
    const boost::filesystem::path output_filename = options.getOutputFilename();

    const boost::filesystem::path input_file_ext  = input_filename.extension();
    const boost::filesystem::path output_file_ext = output_filename.extension();

    bool success;

    if (input_file_ext == ".mp3" && output_file_ext == ".wav") {
        success = convertAudioFormat(
            input_filename,
            output_filename
        );
    }
    else if ((input_file_ext  == ".mp3" || input_file_ext  == ".wav" || input_file_ext == ".flac") &&
             (output_file_ext == ".dat" || output_file_ext == ".json")) {
        success = generateWaveformData(
            input_filename,
            output_filename,
            options.getSamplesPerPixel(),
            options.getBits()
        );
    }
    else if (input_file_ext == ".dat" &&
             (output_file_ext == ".txt" || output_file_ext == ".json")) {
        success = convertWaveformData(
            input_filename,
            output_filename
        );
    }
    else {
        error_stream << "Can't generate " << output_filename
                     << " from " << input_filename << '\n';
        success = false;
    }

    return success ? 0 : 1;
}

//------------------------------------------------------------------------------
