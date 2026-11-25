p='engine/gpu/d3d8/d3d8_device.cpp'
s=open(p,newline='').read(); crlf='\r\n' in s; s=s.replace('\r\n','\n')
def rep(a,b):
    global s
    assert s.count(a)==1,(s.count(a),a); s=s.replace(a,b)
rep("PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_NONE,","PS_TEXTUREMODES(PS_TEXTUREMODES_PROJECT2D, PS_TEXTUREMODES_PASSTHRU,")
rep("PS_COMBINERCOUNT(4,","PS_COMBINERCOUNT(5,")
i=s.index("                // What there is of the sun, and what there is without it."); j=s.index("                if (FAILED(m_device->CreatePixelShader")
s=s[:i]+"""                // Where the sun doesn't reach, by how much this faces it: what turns away has already lost it, and would
                // only show the steps of the map.
                def.PSRGBInputs[2] = PS_COMBINERINPUTS(PS_REGISTER_T1 | PS_CHANNEL_RGB, PS_REGISTER_T2 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[2] = PS_COMBINEROUTPUTS(PS_REGISTER_R1, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, PS_COMBINEROUTPUT_SHIFTLEFT_1);
                def.PSRGBInputs[3] = PS_COMBINERINPUTS(PS_REGISTER_R1 | PS_CHANNEL_RGB, PS_REGISTER_C0 | PS_CHANNEL_RGB, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[3] = PS_COMBINEROUTPUTS(PS_REGISTER_R1, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                def.PSRGBInputs[4] = PS_COMBINERINPUTS(PS_REGISTER_R0 | PS_CHANNEL_RGB, PS_REGISTER_R1 | PS_CHANNEL_RGB | PS_INPUTMAPPING_UNSIGNED_INVERT, PS_REGISTER_ZERO, PS_REGISTER_ZERO);
                def.PSRGBOutputs[4] = PS_COMBINEROUTPUTS(PS_REGISTER_R0, PS_REGISTER_DISCARD, PS_REGISTER_DISCARD, 0);
                // Which of the constants each stage has: the tint, and how dark.
                def.PSC0Mapping = PS_CONSTANTMAPPING(15, 0, 15, 1, 15, 15, 15, 15);
                def.PSC1Mapping = PS_CONSTANTMAPPING(15, 15, 15, 15, 15, 15, 15, 15);
"""+s[j:]
rep("const float constants[3][4] = { { draw.color.r, draw.color.g, draw.color.b, draw.color.a }, { shade, shade, shade, 1 }, { 1 - shade, 1 - shade, 1 - shade, 1 } };","const float constants[2][4] = { { draw.color.r, draw.color.g, draw.color.b, draw.color.a }, { shade, shade, shade, 1 } };")
rep("SetPixelShaderConstant(0, constants, 3);","SetPixelShaderConstant(0, constants, 2);")
rep("                // Only the sun's part of the light goes.\n","""                // How much each corner faces the sun, as a colour.
                const Vec3& d = m_view.sunDirection;
                const float sunX = view.m[0] * d.x + view.m[4] * d.y + view.m[8] * d.z, sunY = view.m[1] * d.x + view.m[5] * d.y + view.m[9] * d.z, sunZ = view.m[2] * d.x + view.m[6] * d.y + view.m[10] * d.z;
                Mat4 facing;
                for (u32 i = 0; i < 3; i++)
                {
                    facing.m[i] = sunX;
                    facing.m[4 + i] = sunY;
                    facing.m[8 + i] = sunZ;
                }
                m_device->SetTransform(D3DTS_TEXTURE1, reinterpret_cast<const D3DMATRIX*>(facing.m));
                m_device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL | 1);
                m_device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
                // Only the sun's part of the light goes.
""")
rep("                m_device->SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);\n","                m_device->SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);\n                m_device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);\n                m_device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);\n")
rep("const float back = 24, steep = 5;","const float back = 8, steep = 3;")
open(p,'w',newline='').write(s.replace('\n','\r\n') if crlf else s)
