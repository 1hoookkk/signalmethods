#include "../../plugin/source/dsp/DeskDrive.h"
#include <RTNeural/RTNeural.h>
#include <fstream>
#include <vector>
#include <iterator>
#include <cstring>
#include <cstdio>
#include <stdexcept>

int main (int argc, char** argv)
{
    try
    {
        if (argc != 5) throw std::runtime_error ("runner target input.f32 output.f32 drive_db | runner model input.f32 output.f32 model.json");
        std::ifstream input (argv[2], std::ios::binary);
        if (! input) throw std::runtime_error ("Cannot read input");
        std::vector<char> bytes ((std::istreambuf_iterator<char> (input)), {});
        if (bytes.empty() || bytes.size() % sizeof (float)) throw std::runtime_error ("Invalid float stream");
        std::vector<float> samples (bytes.size() / sizeof (float));
        std::memcpy (samples.data(), bytes.data(), bytes.size());
        trench::DeskDrive desk;
        std::unique_ptr<RTNeural::Model<float>> model;
        bool residual = false;
        const std::string mode (argv[1]);
        if (mode == "target")
        {
            const double drive = std::stod (argv[4]);
            if (! std::isfinite (drive) || drive < 0 || drive > 60) throw std::runtime_error ("Drive must be between 0 and 60 dB");
            desk.prepare (48000);
            desk.setTrims (std::pow (10.0, drive / 40.0) / 10.0, 1.0);
            desk.setEnabled (true);
        }
        else if (mode == "model")
        {
            std::ifstream file (argv[4]);
            if (! file) throw std::runtime_error ("Cannot read model");
            nlohmann::json document;
            file >> document;
            if (document.contains ("workshop_runtime")) residual = document["workshop_runtime"].value ("residual_input", false);
            file.clear();
            file.seekg (0);
            model = RTNeural::json_parser::parseJson<float> (file, true);
            if (! model || model->getInSize() != 1 || model->getOutSize() != 1) throw std::runtime_error ("Expected mono model");
            model->reset();
            const float zero = 0;
            for (int i = 0; i < 12000; ++i) model->forward (&zero);
        }
        else throw std::runtime_error ("Unknown mode");
        for (float& x : samples)
        {
            if (! std::isfinite (x)) throw std::runtime_error ("Nonfinite input");
            x = model ? model->forward (&x) + (residual ? x : 0.0f) : desk.process (x);
            if (! std::isfinite (x)) throw std::runtime_error ("Nonfinite output");
        }
        std::ofstream output (argv[3], std::ios::binary);
        output.write (reinterpret_cast<const char*> (samples.data()), (std::streamsize) bytes.size());
        if (! output) throw std::runtime_error ("Cannot write output");
        std::printf ("%s: %zu samples at 48000 Hz\n", mode.c_str(), samples.size());
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf (stderr, "%s\n", error.what());
        return 1;
    }
}
