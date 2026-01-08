project('ssh-proxy-tray', 'cpp',
        version: '0.2.2-1',
        default_options : [
          'cpp_std=c++20',
          'warning_level=3',
          'buildtype=release',      # enables optimizations (-O3, NDEBUG)
          'optimization=3',         # maximum possible optimization level
          'b_ndebug=true',          # disables assert()
          'b_lto=true',             # enable Link-Time Optimization
          'b_pie=true',             # position-independent executable (ASLR)
          'strip=true',             # remove debug symbols for minimal binary size
          'b_staticpic=false'       # faster build if static PIC is not needed
        ])

qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Widgets'])

subdir('src')
