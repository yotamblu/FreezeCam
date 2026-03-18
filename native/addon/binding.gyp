{
  "targets": [
    {
      "target_name": "freezecam_addon",
      "sources": ["addon.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../shared"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "WIN32_LEAN_AND_MEAN",
        "UNICODE",
        "_UNICODE"
      ],
      "conditions": [
        ["OS=='win'", {
          "libraries": [
            "-lkernel32.lib"
          ]
        }]
      ]
    }
  ]
}
