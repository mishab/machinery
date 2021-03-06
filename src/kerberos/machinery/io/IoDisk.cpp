#include "machinery/io/IoDisk.h"

namespace kerberos
{
    void IoDisk::setup(const StringMap & settings)
    {
        Io::setup(settings);
        
        // --------------------------
        // Get name from instance
        
        std::string instanceName = settings.at("name");
        setInstanceName(instanceName);

        // --------------------------
        // Check if need to draw timestamp
        
        bool drawTimestamp = (settings.at("ios.Disk.markWithTimestamp") == "true");
        setDrawTimestamp(drawTimestamp);
        cv::Scalar color = getColor(settings.at("ios.Disk.timestampColor"));
        setTimestampColor(color);
        
        std::string timezone = settings.at("timezone");
        std::replace(timezone.begin(), timezone.end(), '-', '/');
        std::replace(timezone.begin(), timezone.end(), '$', '_');
        setTimezone(timezone);
        
        // -------------------------------------------------------------
        // Filemanager is mapped to a directory and is used by an image
        // to save to the correct directory.
        
        setFileFormat(settings.at("ios.Disk.fileFormat"));
        m_fileManager.setBaseDirectory(settings.at("ios.Disk.directory"));
    }
    
    cv::Scalar IoDisk::getColor(const std::string name)
    {
        std::map<std::string, cv::Scalar> m_colors;
        
        m_colors["white"] = cv::Scalar(255,255,255);
        m_colors["black"] = cv::Scalar(0,0,0);
        m_colors["red"] = cv::Scalar(0,0,255);
        m_colors["green"] = cv::Scalar(0,255,0);
        m_colors["blue"] = cv::Scalar(255,0,0);
        
        return m_colors.at(name);
    }

    std::string IoDisk::buildPath(std::string pathToImage)
    {
        // -----------------------------------------------
        // Get timestamp, microseconds, random token, and instance name
        
        std::string instanceName = getInstanceName();
        kerberos::helper::replace(pathToImage, "instanceName", instanceName);

        return pathToImage;
    }
    
    void IoDisk::drawDateOnImage(Image & image, std::string timestamp)
    {
        if(m_drawTimestamp)
        {
            struct tm tstruct;
            char buf[80];
            
            time_t now = std::atoi(timestamp.c_str());
            
            char * timeformat = "%d-%m-%Y %X";
            if(m_timezone != "")
            {
                setenv("TZ", m_timezone.c_str(), 1);
                tzset();
            }
            
            tstruct = *localtime(&now);
            strftime(buf, sizeof(buf), timeformat, &tstruct);
            
            cv::putText(image.getImage(), buf, cv::Point(10,30), cv::FONT_HERSHEY_SIMPLEX, 0.5, getTimestampColor());
        }
    }

    bool IoDisk::save(Image & image)
    {
        // ----------------------------------------
        // The naming convention that will be used
        // for the image.
        
        std::string pathToImage = getFileFormat();

        // ---------------------
        // Replace variables

        pathToImage = buildPath(pathToImage);

        std::string timestamp = kerberos::helper::getTimestamp();
        kerberos::helper::replace(pathToImage, "timestamp", timestamp);
        drawDateOnImage(image, timestamp);

        std::string microseconds = kerberos::helper::getMicroseconds();
        std::string size = kerberos::helper::to_string((int)microseconds.length());
        kerberos::helper::replace(pathToImage, "microseconds", size + "-" + microseconds);

        std::string token = kerberos::helper::to_string(rand()%1000);
        kerberos::helper::replace(pathToImage, "token", token);

        // ---------------------------------------------------------------------
        // Save original version & generate unique timestamp for current image
        
        return m_fileManager.save(image, pathToImage);
    }
    
    bool IoDisk::save(Image & image, JSON & data)
    {
        // ----------------------------------------
        // The naming convention that will be used
        // for the image.
        
        std::string pathToImage = getFileFormat();

        // ------------------------------------------
        // Stringify data object: build image path
        // with data information.
        
        static const std::string kTypeNames[] = { "Null", "False", "True", "Object", "Array", "String", "Number" };
        for (JSONValue::ConstMemberIterator itr = data.MemberBegin(); itr != data.MemberEnd(); ++itr)
        {
            std::string name = itr->name.GetString();
            std::string type = kTypeNames[itr->value.GetType()];
            
            if(type == "String")
            {
                std::string value = itr->value.GetString();
                kerberos::helper::replace(pathToImage, name, value);
            }
            else if(type == "Number")
            {
                std::string value = kerberos::helper::to_string(itr->value.GetInt());
                kerberos::helper::replace(pathToImage, name, value);
            }
            else if(type == "Array")
            {
                std::string arrayString = "";
                for (JSONValue::ConstValueIterator itr2 = itr->value.Begin(); itr2 != itr->value.End(); ++itr2)
                {
                    type = kTypeNames[itr2->GetType()];
                    
                    if(type == "String")
                    {
                        arrayString += itr2->GetString();
                    }
                    else if(type == "Number")
                    {
                       arrayString += kerberos::helper::to_string(itr2->GetInt());
                    }
                    
                    arrayString += "-";
                }
                kerberos::helper::replace(pathToImage, name, arrayString.substr(0, arrayString.size()-1));
            }
        }
        
        /// ---------------------
        // Replace variables

        pathToImage = buildPath(pathToImage);

        // -------------------------------------------------------
        // Add path to JSON object, so other IO devices can use it
        
        JSONValue path;
        JSON::AllocatorType& allocator = data.GetAllocator();
        path.SetString(pathToImage.c_str(), allocator);
        data.AddMember("pathToImage", path, allocator);
        
        // ------------------
        // Draw date on image
        
        drawDateOnImage(image, data["timestamp"].GetString());
        
        // -------------------------
        // Save original version

        BINFO << "IoDisk: saving image " + pathToImage;
        return m_fileManager.save(image, pathToImage);
    }
}
