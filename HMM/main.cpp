// Copyright (c) 2015, Pacific Biosciences of California, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted (subject to the limitations in the
// disclaimer below) provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//  * Neither the name of Pacific Biosciences nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY PACIFIC
// BIOSCIENCES AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

// Author: Bo Han

#include <stdio.h>
#include <boost/program_options.hpp>
#include "fasta.hpp"
#include "polyA_hmm_model.hpp"
#include "kernel_color.h"

void usage(const char*);
void setDefaultHMM(PolyAHmmMode&);
template <bool showColor> void trimPolyA(const std::string&, const PolyAHmmMode&);
void printHeader(const std::string&, size_t);

int main(int argc, const char* argv[])
{
    boost::program_options::options_description opts(R"(this program trims the polyA tail specifically from PacBio Iso-Seq data\n)");
    /** options **/
    std::string input_fa_file;
    std::string model_file;
    std::string train_polya_file;
    std::string train_nonpolya_file;
    std::string train_model_file;
    bool show_color = false;
    try {
        opts.add_options()
        ("help,h", "display this help message and exit")
        ("input,i", boost::program_options::value<std::string>(&input_fa_file)->required(), "The input fasta file with polyA")
        ("model,m", boost::program_options::value<std::string>(&model_file)->default_value(""), "HMM model file to use")
        ("polyA_training,a", boost::program_options::value<std::string>(&train_polya_file)->default_value(""), "Fasta file with polyA sequences for training with maximum-likelihood estimation")
        ("non_polyA_training,b", boost::program_options::value<std::string>(&train_nonpolya_file)->default_value(""), "Fasta file with non-polyA sequences for training with maximum-likelihood estimation")
        ("new_model,n", boost::program_options::value<std::string>(&train_model_file)->default_value(""), "New trained model file to output")
        ("color,c", boost::program_options::bool_switch(&show_color), "To color polyA sequences in the output instead of trimming away them")
        ;
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
        boost::program_options::notify(vm);
        if (vm.count("help")) {
            std::cerr << opts << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << opts << std::endl;
        exit(EXIT_FAILURE);
    }

    PolyAHmmMode hmm;

    // initializing HMM model
    if (!train_polya_file.empty() && !train_nonpolya_file.empty()) {
        // if both training data are set, use that to train the data
        if (!model_file.empty()) {
            fprintf(stderr, "cannot specify -m with -a/-b");
            exit(EXIT_FAILURE);
        }
        FastaReader<> plA_file{ train_polya_file };
        FastaReader<> nplA_file{ train_nonpolya_file };
        hmm.maximumLikelihoodEstimation(plA_file.begin(), plA_file.end(), nplA_file.begin(), nplA_file.end());
        if (!train_model_file.empty())
            hmm.write(train_model_file);
    }
    else if (!model_file.empty()) {
        // read saved HMM model from file
        if (!hmm.read(model_file))
            return EXIT_FAILURE;
    }
    else {
        if (!train_polya_file.empty() || !train_nonpolya_file.empty()) {
            fprintf(stderr, "need to specify both -a and -b");
            exit(EXIT_FAILURE);
        }
        // using default HMM model
        setDefaultHMM(hmm);
    }

    // trim
    if(show_color)
        trimPolyA<true>(input_fa_file, hmm);
    else
        trimPolyA<false>(input_fa_file, hmm);
    
    return EXIT_SUCCESS;
}

void setDefaultHMM(PolyAHmmMode& hmm)
{
    hmm.initialProb(PolyAHmmMode::States::POLYA)    = 0.99283668;
    hmm.initialProb(PolyAHmmMode::States::NONPOLYA) = 0.00716332;

    hmm.transProb(PolyAHmmMode::States::POLYA, PolyAHmmMode::States::NONPOLYA)    = 3.16493e-07;
    hmm.transProb(PolyAHmmMode::States::POLYA, PolyAHmmMode::States::POLYA)       = 1.0 - hmm.transProb(PolyAHmmMode::States::POLYA, PolyAHmmMode::States::NONPOLYA);
    hmm.transProb(PolyAHmmMode::States::NONPOLYA, PolyAHmmMode::States::POLYA)    = 2.74842e-09;
    hmm.transProb(PolyAHmmMode::States::NONPOLYA, PolyAHmmMode::States::NONPOLYA) = 1.0 - hmm.transProb(PolyAHmmMode::States::NONPOLYA, PolyAHmmMode::States::POLYA);

    hmm.emitProb(PolyAHmmMode::States::POLYA, to_idx['A'])    = 0.928165;
    hmm.emitProb(PolyAHmmMode::States::POLYA, to_idx['C'])    = 0.025917;
    hmm.emitProb(PolyAHmmMode::States::POLYA, to_idx['G'])    = 0.024170;
    hmm.emitProb(PolyAHmmMode::States::POLYA, to_idx['T'])    = 0.021748;
    hmm.emitProb(PolyAHmmMode::States::NONPOLYA, to_idx['A']) = 0.271806;
    hmm.emitProb(PolyAHmmMode::States::NONPOLYA, to_idx['C']) = 0.249539;
    hmm.emitProb(PolyAHmmMode::States::NONPOLYA, to_idx['G']) = 0.281787;
    hmm.emitProb(PolyAHmmMode::States::NONPOLYA, to_idx['T']) = 0.196867;
}

template <bool showColor>
void trimPolyA(const std::string& input_fa, const PolyAHmmMode& hmm)
{
    FastaReader<> fa_reader{ input_fa };
    size_t polyalen;
    for (auto& fa : fa_reader) {
        const Matrix<int>& path = hmm.calculateVirtabi(fa.seq_.rbegin(), fa.seq_.size());
        for (polyalen = 0; polyalen < path.size(); ++polyalen) {
            if (path[polyalen] == PolyAHmmMode::States::NONPOLYA) {
                break;
            }
        }
        fprintf(stderr, "%s\t%zu\n", fa.name_.c_str(), polyalen);
        if (polyalen < fa.size()) {
            printHeader(fa.name_, polyalen);
            if (showColor) {
                fprintf(stdout, "%s" KERNAL_RED "%s \n" KERNAL_RESET,
                      fa.seq_.substr(0, fa.seq_.size() - polyalen).c_str()
                    , fa.seq_.substr(fa.seq_.size() - polyalen).c_str());
            }
            
            if (!showColor) {
                fprintf(stdout, "%s\n", fa.seq_.substr(0, fa.seq_.size() - polyalen).c_str());
            }
        }
    }
}


void printHeader(const std::string& s, size_t polyalen)
{
    // format:
    // <movie_name>/<ZMW name>/<start>_<end>_<CCS>
    fprintf(stdout, ">");
    auto l = s.cbegin();
    while(*l != '/') {
        fprintf(stdout, "%c", *l++);
    }
    // l is not point to 1st '\'
    fprintf(stdout, "/");
    ++l;
    
    while(*l != '/') {
        fprintf(stdout, "%c", *l++);
    }
    // l is not point to the 2nd '\'
    fprintf(stdout, "/");
    ++l;

    auto r = l;
    while(*r++ != '_');
    int start = std::stod(std::string{l, r}); // this will likely throw exception is the format is Iso-Seq specific
    l = r;
    while(*r++ != '_');
    int end = std::stod(std::string{l, r}); // this will likely throw exception is the format is Iso-Seq specific
    if(start < end)
    { // + strand, update end
        end -= polyalen;
    }
    else
    { // - strand, update start
        end += polyalen;
    }
    fprintf(stdout, "%d_%d_CCS\n", start, end);
}