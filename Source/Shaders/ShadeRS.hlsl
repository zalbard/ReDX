#define RootSig                                                          \
    "RootFlags(DENY_VERTEX_SHADER_ROOT_ACCESS), "                        \
    "CBV(b0, visibility = SHADER_VISIBILITY_PIXEL), "                    \
    "DescriptorTable("                                                   \
        "SRV(t0, numDescriptors = 5), "                                  \
        "visibility = SHADER_VISIBILITY_PIXEL), "                        \
    "DescriptorTable("                                                   \
        "SRV(t5, numDescriptors = unbounded), "                          \
        "visibility = SHADER_VISIBILITY_PIXEL), "                        \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 4, " \
                  "visibility = SHADER_VISIBILITY_PIXEL)"
