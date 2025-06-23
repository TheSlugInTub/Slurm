#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>

void ExecuteCommand(const std::string& input);
    
std::vector<std::string> commands;
std::filesystem::path currentPath;

void SaveSession(const char* filename)
{
    std::ofstream file(filename);

    if (!file.is_open())
    {
        std::cout << "Error opening file to write.\n";
        return;
    }
    
    for (int i = 0; i < commands.size(); i++)
    {
        file << commands[i] << std::endl;
        file.flush();
    }

    file.close();
}

void LoadSession(const char* filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cout << "Session file does not found, either the filename is incorrect or it doesn't exist.\n";
        return;
    }

    std::string cmd;

    while (std::getline(file, cmd))
    {
        ExecuteCommand(cmd);
    }
}

void ExecuteCommand(const std::string& input)
{
    if (input.substr(0, 3) == "cd ")
    {
        std::filesystem::path newPath(input.substr(3));
    
        if (std::filesystem::exists(newPath))
        {
            std::filesystem::current_path(newPath);
            currentPath = std::filesystem::current_path();
        }else 
        {
            std::cout << "Invalid directory.\n";
        }
    
        commands.push_back(input);
    }
    else if (input == "cls")
    {
        std::cout << "\033[2J\033[1;1H";
        commands.push_back(input);
    }
    else if (input == "exit")
    {
        exit(0);
    }
    else if (input == "savesession")
    {
        SaveSession("D:/session.txt");
    }
    else if (input == "loadsession")
    {
        LoadSession("D:/session.txt");
    }
    else 
    {
        system(input.c_str());
        commands.push_back(input);
    }
}

int main(int argc, char** argv)
{
    currentPath = std::filesystem::current_path();
    std::string input;

    while (1)
    {
        std::string pathString(currentPath.string());
        std::cout << pathString << " > ";
        std::getline(std::cin, input);

        ExecuteCommand(input);
    }

    return 0;
}
