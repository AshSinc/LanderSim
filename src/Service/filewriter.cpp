#include "filewriter.h"

void Service::Writer::writeToFile(std::string file, std::string text){
    if(file == "NAV")
        p_file = &navFile;
    else if(file == "THRUST")
        p_file = &thrustFile;
    else if(file == "EST")
        p_file = &estFile;
    else if(file == "PARAMS")
        p_file = &paramsFile;
    else if(file == "GNC")
        p_file = &gncFile;
    else if(file == "PRE")
        p_file = &preApproachFile;
    else
        return;

    if (p_file->is_open())
        (*p_file) << text << "\n";
    else 
        std::cout << "Unable to open file " + file + "\n";
}

void Service::Writer::openFiles(){
    navFile.open(NAV_PATH + "nav.txt", std::ios_base::app);
    thrustFile.open(NAV_PATH + "thrust.txt", std::ios_base::app);
    estFile.open(NAV_PATH + "estimates.txt", std::ios_base::app);
    paramsFile.open(NAV_PATH + "params.txt", std::ios_base::app);
    preApproachFile.open(NAV_PATH + "preapproach.txt", std::ios_base::app);
    gncFile.open(NAV_PATH + "gnc.txt", std::ios_base::app);
    std::cout << "Files opened\n";
}

void Service::Writer::closeFiles(){
    navFile.close();
    thrustFile.close();
    estFile.close();
    paramsFile.close();
    preApproachFile.close();
    gncFile.close();
    std::cout << "Files closed\n";
}

void Service::Writer::clearOutputFolders(){
    //set up directories
    try{
        std::filesystem::remove_all(OUT_PATH);
        std::filesystem::create_directory(OUT_PATH);
        std::filesystem::create_directory(NAV_PATH);
        std::filesystem::create_directory(OPTICS_PATH);
        std::filesystem::create_directory(OPTICS_FEATURE_PATH);
        std::filesystem::create_directory(OPTICS_MATCH_PATH);
    }
    catch (const std::exception &e){
        std::cerr << e.what() << std::endl;
    }
}