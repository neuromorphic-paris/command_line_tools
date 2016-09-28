-- Define the install prefix
prefix = nil
    -- Installation under Linux
    if (os.is('linux')) then
        prefix = '/usr/local'
        os.execute('sudo chown -R `whoami` ' .. prefix .. ' && sudo chmod -R 751 ' .. prefix)

    -- Installation under Mac OS X
    elseif (os.is('macosx')) then
        prefix = '/usr/local'

    -- Other platforms
    else
        print(string.char(27) .. '[31mThe installation is not supported on your platform' .. string.char(27) .. '[0m')
        os.exit()
    end

solution 'utilities'
    configurations {'Release', 'Debug'}
    location 'build'

    project 'rainmaker'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/lodepng.hpp', 'source/html.hpp', 'source/lodepng.cpp', 'source/rainmaker.cpp'}

        -- Define the include paths
        includedirs {path.join(prefix, 'include')}
        libdirs {path.join(prefix, 'lib')}

        -- Link the dependencies
        links {'opalkellyfrontpanel'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}


    project 'okaToCsv'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/okaToCsv.cpp'}

        -- Define the include paths
        includedirs {path.join(prefix, 'include')}
        libdirs {path.join(prefix, 'lib')}

        -- Link the dependencies
        links {'opalkellyfrontpanel'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}


    project 'datToOka'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/datToOka.cpp'}

        -- Define the include paths
        includedirs {path.join(prefix, 'include')}
        libdirs {path.join(prefix, 'lib')}

        -- Link the dependencies
        links {'opalkellyfrontpanel'}

        -- Declare the configurations
        configuration 'Release'
            targetdir 'build/Release'
            defines {'NDEBUG'}
            flags {'OptimizeSpeed'}
        configuration 'Debug'
            targetdir 'build/Debug'
            defines {'DEBUG'}
            flags {'Symbols'}

        -- Linux specific settings
        configuration 'linux'
            buildoptions {'-std=c++11'}
            linkoptions {'-std=c++11'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}

