{
  'includes': [ 'deps/common-libicu.gypi' ],
  'variables': {
      'libicu%':'internal',
  },
  'targets': [
    {
      'target_name': 'icu_bidi',
      'conditions': [
        ['libicu != "internal"', {
            'libraries': [ "<!@(icu-config --ldflags)" ],
            'cflags': [ "<!@(icu-config --cppflags)" ]
        },
        {
            'dependencies': [
              'deps/libicu.gyp:libicu'
            ]
        }
        ]
      ],
      'sources': [
        'src/node_icu_bidi.cc',
      ],
    }
  ]
}
