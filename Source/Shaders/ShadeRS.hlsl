#define RootSig                                                                        \
    "RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS), "                                      \
    "RootConstants(num32BitConstants = 9, b0, visibility = SHADER_VISIBILITY_PIXEL), " \
    "SRV(t0, visibility = SHADER_VISIBILITY_PIXEL), "                                  \
    "DescriptorTable("                                                                 \
        "SRV(t1, numDescriptors = 5), visibility = SHADER_VISIBILITY_PIXEL), "         \
    "DescriptorTable("                                                                 \
        "SRV(t6, numDescriptors = unbounded), visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 4, "               \
                  "visibility = SHADER_VISIBILITY_PIXEL)"
