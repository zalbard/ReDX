#define RootSig                                                                          \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), "                                    \
    "DescriptorTable("                                                                   \
        "SRV(t0, numDescriptors = 1), "                                                  \
        "visibility = SHADER_VISIBILITY_PIXEL), "                                        \
    "RootConstants(num32BitConstants = 1,  b0, visibility = SHADER_VISIBILITY_PIXEL), "  \
    "RootConstants(num32BitConstants = 12, b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 4, "                 \
                  "visibility = SHADER_VISIBILITY_PIXEL)"
