{
  'includes': [ 'common-libicu.gypi' ],
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'variables': {
          'configure_options': ['--enable-debug']
        },
        'defines': [ 'DEBUG', '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
          },
        },
      },
      'Release': {
        'variables': {
          'configure_options': ['--enable-release']
        },
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
      },
      'VCLibrarianTool': {
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
      },
    },
    'conditions': [
      ['OS == "win"', {
        'defines': [
          'WIN32'
        ],
      }]
    ],
  },

  'targets': [
    {
      'target_name': 'action_before_build',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'unpack_libicu_dep',
          'inputs': [
            './icu4c-<@(libicu_version)-src.tgz'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/configure'
          ],
          'action': ['python','./extract.py','./icu4c-<@(libicu_version)-src.tgz','<(SHARED_INTERMEDIATE_DIR)', 'icu/source/configure']
        },
        {
          'action_name': 'configure_libicu',
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/configure'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/Makefile'
          ],
          'action': [
                'python', './cdconfigure.py',
                '<(SHARED_INTERMEDIATE_DIR)/icu/source/',
                'CFLAGS=-fPIC', 'CXXFLAGS=-fPIC', # required on x64
                '--enable-static', '--disable-shared',
                # we can't use --disable-tools since the build process
                # uses bin/icupkg internally
                '--disable-extras', '--disable-icuio',
                '--disable-layout', '--disable-tests',
                '--disable-samples', '<@(configure_options)',
                '--with-data-packaging=static'
          ]
        }
      ]
    },
    {
      'target_name': 'build',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'action_before_build'
      ],
      'actions': [
        {
          'action_name': 'build_libicu',
          'inputs': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/Makefile'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicudata.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicui18n.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicutu.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicuuc.a'
          ],
          'action': ['make', '-C', '<(SHARED_INTERMEDIATE_DIR)/icu/source/',
                     '-j', '2']
        }
      ]
    },
    {
      'target_name': 'libicu',
      'type': 'static_library',
      'dependencies': [
        'build'
      ],
      'direct_dependent_settings': {
        'include_dirs': [ '<(SHARED_INTERMEDIATE_DIR)/icu/source/common/' ],
      },
      'link_settings': {
        'libraries': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicui18n.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicuuc.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicudata.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicutu.a',
        ]
      },
      'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicui18n.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicuuc.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicudata.a',
            '<(SHARED_INTERMEDIATE_DIR)/icu/source/lib/libicutu.a',
      ]
    }
  ]
}
