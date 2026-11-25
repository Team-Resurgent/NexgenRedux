def patch(p, pairs):
    raw = open(p, 'rb').read(); crlf = b'\r\n' in raw
    s = raw.decode('utf-8').replace('\r\n', '\n')
    for a, b in pairs:
        assert s.count(a) == 1, (p, s.count(a), a)
        s = s.replace(a, b)
    open(p, 'wb').write((s.replace('\n', '\r\n') if crlf else s).encode('utf-8'))
patch('engine/gpu/d3d8/d3d8_device.cpp',[
("constexpr u32 kShadowSide = 512;","constexpr u32 kShadowSide = 1024;"),
("                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);\n                m_inShadows = true;","""                m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
                // Back a little, and more where it is steep to the sun, so that nothing shades itself.
                const float back = 6, steep = 2.5f;
                DWORD bits;
                memcpy(&bits, &back, sizeof(bits));
                m_device->SetRenderState(D3DRS_POLYGONOFFSETZOFFSET, bits);
                memcpy(&bits, &steep, sizeof(bits));
                m_device->SetRenderState(D3DRS_POLYGONOFFSETZSLOPESCALE, bits);
                m_device->SetRenderState(D3DRS_SOLIDOFFSETENABLE, TRUE);
                m_inShadows = true;"""),
("                m_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);\n","                m_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);\n                m_device->SetRenderState(D3DRS_SOLIDOFFSETENABLE, FALSE);\n"),
("                map.m[14] = -90.0f;   // so that nothing shades itself\n",""),
("            void endShadows() override\n","            u32 shadowSize() const override { return kShadowSide - 4; }\n\n            void endShadows() override\n"),
])
patch('engine/gpu/gpu.h',[("        virtual void endShadows() = 0;\n","        virtual void endShadows() = 0;\n        // How many pixels across they are.\n        virtual u32 shadowSize() const { return kShadowSize; }\n")])
patch('engine/scene/space.cpp',[("static_cast<float>(gpu::kShadowSize);","static_cast<float>(device.shadowSize());")])
