{
  "targets": [
    {
      "target_name": "jolt_backend",
      "sources": [
        "src/addon.cpp",
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
