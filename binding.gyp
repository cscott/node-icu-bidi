{
  'includes': [ 'deps/common-libicu.gypi' ],
  'variables': {
      'libicu%':'internal',
  },
  'targets': [
    {
      'target_name': 'icu_bidi',
      "include_dirs": ["<!(node -e \"require('nan')\")"],
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
    },
    {
      "target_name": "action_after_build",
      "type": "none",
      "dependencies": [ "<(module_name)" ],
      "copies": [
        {
          "files": [ "<(PRODUCT_DIR)/<(module_name).node" ],
          "destination": "<(module_path)"
        }
      ]
    }
  ]
}
