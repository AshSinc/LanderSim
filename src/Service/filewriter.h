#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace Service{
    const bool OUTPUT_TEXT = true;
    const bool OUTPUT_OPTICS = true;
    const std::string OUT_PATH = "output/";
    const std::string NAV_PATH = OUT_PATH + "nav/";
    const std::string OPTICS_PATH = OUT_PATH + "optics/";
    const std::string OPTICS_FEATURE_PATH = OUT_PATH + "feature/";
    const std::string OPTICS_MATCH_PATH = OUT_PATH + "match/";

    class Writer{
        private:

        std::ofstream* p_file;

        public:

        std::ofstream navFile;
        std::ofstream thrustFile;
        std::ofstream estFile;
        std::ofstream paramsFile;

        void writeToFile(std::string file, std::string text);
        void openFiles();
        void closeFiles();
        void clearOutputFolders();
    };
}