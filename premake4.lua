solution 'utilities'
    configurations {'Release', 'Debug'}
    location 'build'

    project 'rainmaker'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/lodepng.hpp', 'source/html.hpp', 'source/lodepng.cpp', 'source/rainmaker.cpp'}

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
            includedirs {'/usr/local/include'}
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            includedirs {'/usr/local/include'}


    project 'esToCsv'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/esToCsv.cpp'}

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
            includedirs {'/usr/local/include'}
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            includedirs {'/usr/local/include'}


    project 'datToEs'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/datToEs.cpp'}

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
            includedirs {'/usr/local/include'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            includedirs {'/usr/local/include'}


    project 'shiftTheParadigm'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/lodepng.hpp', 'source/lodepng.cpp', 'source/shiftTheParadigm.cpp'}

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
            includedirs {'/usr/local/include'}
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
            includedirs {'/usr/local/include'}
