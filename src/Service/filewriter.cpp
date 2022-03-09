#include "filewriter.h"

#include <filesystem>

void Service::Writer::writeToFile(std::string file, std::string text){
    if(file == "NAV")
        p_file = &navFile;
    
    if (p_file->is_open()){
        *p_file << text;
        p_file->close();
    }
    else 
        std::cout << "Unable to open file " + file + "\n";
}

void Service::Writer::openFiles(){
    navFile.open(NAV_PATH + "nav.txt", std::ios_base::app);
    std::cout << "Files opened\n";
}

void Service::Writer::closeFiles(){
    navFile.close();
    std::cout << "Files closed\n";
}

void Service::Writer::clearOutputFolders(){
    //set up directories
    try{
        std::filesystem::remove_all(OUT_PATH);

        std::filesystem::create_directory(OUT_PATH);

        std::filesystem::create_directory(NAV_PATH);

        std::filesystem::create_directory(EST_PATH);

        std::filesystem::create_directory(OPTICS_PATH);

        std::filesystem::create_directory(OPTICS_FEATURE_PATH);

        std::filesystem::create_directory(OPTICS_MATCH_PATH);

    }
    catch (const std::exception &e){
        std::cerr << e.what() << std::endl;
    }

}