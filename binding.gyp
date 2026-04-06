{
  "targets": [
    {
      "target_name": "jolt-world",
      "sources": [
        "src/addon/addon.cpp",
        "src/addon/world.cpp",
        "src/addon/js_convert.cpp",
        "src/addon/listners.cpp",
        "src/addon/layers/layers.cpp",
        "src/addon/layers/layers_manager.cpp",
        "<!@(find JoltPhysics/Jolt -type f -name '*.cpp')"
      ],
      "include_dirs": [
        "JoltPhysics"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-fno-rtti",
        "-fno-exceptions"
      ],
      "defines": [
        "JPH_OBJECT_STREAM",
        "JPH_DEBUG_RENDERER"
      ],
      "conditions": [
        ["OS=='linux'", {
          "libraries": [
            "-lpthread"
          ]
        }]
      ]
    }
  ]
}
