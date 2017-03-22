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
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

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
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}


    project 'esToCsv'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/esToCsv.cpp'}

        -- Define the include paths
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

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
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}


    project 'datToEs'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/datToEs.cpp'}

        -- Define the include paths
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

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


    project 'shiftTheParadigm'
        -- General settings
        kind 'ConsoleApp'
        language 'C++'
        location 'build'
        files {'source/lodepng.hpp', 'source/lodepng.cpp', 'source/shiftTheParadigm.cpp'}

        -- Define the include paths
        includedirs {'/usr/local/include'}
        libdirs {'/usr/local/lib'}

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
            links {'pthread'}

        -- Mac OS X specific settings
        configuration 'macosx'
            buildoptions {'-std=c++11', '-stdlib=libc++'}
            linkoptions {'-std=c++11', '-stdlib=libc++'}
