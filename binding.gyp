{
  "targets": [
    {
      "target_name": "jolt_engine",
      "sources": [
        "src/module.cpp",
        "src/addon/world.cpp",
        "src/addon/napi/js_convert.cpp",
        "src/addon/napi/jolt_convert.cpp",
        "src/addon/events.cpp",
        "src/addon/layers.cpp",
        "src/addon/layers_manager.cpp",
        "src/addon/body_manager.cpp",
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
