p='engine/gpu/d3d8/d3d8_device.cpp'
s=open('C:/Users/eq2k/AppData/Local/Temp/claude/e81e539b0de285ca/33f3e0c3-0ec6-48f4-833d-079e1a729598/scratchpad/d3d8.old',newline='').read(); crlf='\r\n' in s; s=s.replace('\r\n','\n')
def rep(a,b):
    global s
    assert s.count(a)==1,(s.count(a),a); s=s.replace(a,b)
i=s.index("                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed(shaded ?"); j=s.index("                if (lit)\n",i)
s=s[:i]+"""                m_device->SetRenderState(D3DRS_TEXTUREFACTOR, packed(draw.color));
                // The same as the stages do, and then some of the sun's light away.
                m_device->SetPixelShader(shaded ? m_shadowShader : 0);
                if (shaded)
                {
                    const float shade = m_shade * (1 - draw.glow);
                    const float constants[3][4] = { { draw.color.r, draw.color.g, draw.color.b, draw.color.a }, { shade, shade, shade, 1 }, { 1 - shade, 1 - shade, 1 - shade, 1 } };
                    m_device->SetPixelShaderConstant(0, constants, 3);
                }
"""+s[j:]
rep("m_shadowed = m_view.shadowStrength > 0 && m_shadowDepth;","m_shadowed = m_view.shadowStrength > 0 && m_shadowDepth && m_shadowShader;")
i=s.index("                m_device->SetRenderState(D3DRS_SHADOWFUNC"); j=s.index("                // Only the sun's part of the light goes.")
s=s[:i]+"                m_device->SetRenderState(D3DRS_SHADOWFUNC, D3DCMP_GREATEREQUAL);\n"+s[j:]
rep("                m_device->SetTextureStageState(2, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);\n","")
rep("                m_device->SetTextureStageState(2, D3DTSS_RESULTARG, D3DTA_CURRENT);\n","")
rep("            // The third stage asks of each pixel whether the sun sees it, and the fourth puts some of that on.\n","            // The third texture is asked of each pixel whether the sun sees it. Only with a pixel shader does the GPU\n            // compare depths: with the fixed stages it reads them as a picture.\n")
rep("            DWORD m_meshShader = 0;\n" if "m_meshShader" in s else "            float m_shade = 0;\n","            float m_shade = 0;\n            DWORD m_shadowShader = 0;\n")
rep("                const u8 white[4] =","""                makeShadowShader();
                const u8 white[4] =""")
rep("            static size_t meshBytes(const MeshSlot& m)\n","""            void makeShadowShader()
            {
                D3DPIXELSHADERDEF def = {};
                def.PSTextureModes = PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_NONE, PS_TEXTUREMODES_PROJECT3D, PS_TEXTUREMODES_NONE);
                def.PSCombinerCount = PS_COMBINERCOUNT(4, PS_COMBINERCOUNT_UNIQUE_C0 | PS_COMBINERCOUNT_UNIQUE_C1);
                // Texture by light.
                def.PSRGBInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_RGB, PS_REGISTER_V0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSAlphaInputs[0] = PS_COMBINERINPUTS(PS_REGISTER_T0 | PS_CHANNEL_ALPHA, PS_REGISTER_V0 | PS_CHANNEL_ALPHA, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[0] = def.PSAlphaOutputs[0] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // By tint.
                def.PSRGBInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSAlphaInputs[1] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_ALPHA, PS_REGISTER_C0 | PS_CHANNEL_ALPHA, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[1] = def.PSAlphaOutputs[1] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // What there is of the sun, and what there is without it.
                def.PSRGBInputs[2] = PS_COMBINERINPUTS(PS_REGISTER_T2 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_C1 | PS_CHANNEL_RGB, PS_REGISTER_ZERO | PS_INPUTMAPPING_UNSIGNED_INVERT);
                def.PSRGBOutputs[2] = PS_COMBINEROUTPUTS(PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_REGISTER_R1, 0);
                def.PSRGBInputs[3] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_R1 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[3] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // Which of the constants each stage has: the tint, and then the two parts of a shadow.
                def.PSC0Mapping = PS_CONSTANTMAPPING(15, 0, 1, 15, 15, 15, 15, 15);
                def.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 2, 15, 15, 15, 15, 15);
                if (FAILED(m_device->CreatePixelShader(&def, &m_shadowShader)))
                {
                    NX_WARN("d3d8: no shadows");
                    m_shadowShader = 0;
                }
            }

            static size_t meshBytes(const MeshSlot& m)
""")
rep("                    m_device->SetIndices(nullptr, 0);\n                }\n                m_buffers.forEach","                    m_device->SetIndices(nullptr, 0);\n                    m_device->SetPixelShader(0);\n                    if (m_shadowShader)\n                    {\n                        m_device->DeletePixelShader(m_shadowShader);\n                    }\n                }\n                m_buffers.forEach")
open(p,'w',newline='').write(s.replace('\n','\r\n') if crlf else s)
