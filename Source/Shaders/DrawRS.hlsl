#define RootSig                                                                                 \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "                                           \
    "RootConstants(num32BitConstants = 1, b0, visibility = SHADER_VISIBILITY_PIXEL), "          \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), "                                          \
    "CBV(b2, visibility = SHADER_VISIBILITY_PIXEL), "                                           \
    "DescriptorTable("                                                                          \
        "SRV(t0, numDescriptors = unbounded), "                                                 \
        "visibility = SHADER_VISIBILITY_PIXEL), "                                               \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 4, "                        \
                  "visibility = SHADER_VISIBILITY_PIXEL)"
