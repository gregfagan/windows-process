{
    'targets': [{
        'target_name': 'no-op',
        'type': 'none'
    }],
    'conditions': [
        ['OS == "win"', {
            'targets': [
                {
                    'target_name': 'windows-process',
                    'include_dirs' : [
                        '<!(node -e "require(\'nan\')")'
                    ],
                    'sources': ['windows-process.cc'],
                    'libraries': ['psapi.lib'],
                }
            ]
        }]
    ]
}
