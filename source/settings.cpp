#include "settings.h"

#include <filesystem>
#include <fstream>
#include <iostream>

Settings gSettings;
void Settings::Save()
{
    std::ofstream out_file("mpfont.ini");
    for (auto it : gSettings.dic)
    {
        out_file << it.first << ":" << it.second << "\n";
    }

    for (auto p : projectList)
        out_file << "project:" << p << "\n";

    out_file.close();
}

void Settings::Load()
{
    std::ifstream in_file("mpfont.ini");
    if (in_file.is_open())
    {
        dic.clear();
        std::string line;
        while (std::getline(in_file, line))
        {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            // Skip empty lines or comments
            if (line.empty() || line[0] == ';' || line[0] == '#')
                continue;

            // Find the colon separator
            auto delimiter_pos = line.find(':');
            if (delimiter_pos != std::string::npos)
            {
                std::string key = line.substr(0, delimiter_pos);
                std::string value = line.substr(delimiter_pos + 1);

                // Trim whitespace from key and value
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                if (key == "project")
                {
                    projectList.push_back(value);
                }
                else if (!key.empty()) 
                {
                    dic[key] = value;
                }
            }
        }
        in_file.close();
    }
}


