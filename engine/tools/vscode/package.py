"""Makes the extension's .vsix, which VS Code installs: python tools/vscode/package.py [out.vsix]

A .vsix is a zip with the extension in a folder called extension and a manifest beside it, so nothing has to be
downloaded to make one. Install it with: code --install-extension nexgen-redux-1.0.0.vsix
"""
import json
import os
import sys
import zipfile
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
FILES = ['package.json', 'extension.js', 'angelscript.tmLanguage.json', 'language-configuration.json', 'icon.png', 'README.md']


def main():
    pkg = json.load(open(os.path.join(HERE, 'package.json'), encoding='utf-8'))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '%s-%s.vsix' % (pkg['name'], pkg['version']))
    manifest = '''<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Language="en-US" Id="{name}" Version="{version}" Publisher="{publisher}" />
    <DisplayName>{display}</DisplayName>
    <Description xml:space="preserve">{description}</Description>
    <Tags>angelscript,debuggers,nexgen</Tags>
    <Categories>{categories}</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{engine}" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
      <Property Id="Microsoft.VisualStudio.Services.Links.Source" Value="{repo}" />
    </Properties>
    <Icon>extension/icon.png</Icon>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true" />
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/icon.png" Addressable="true" />
  </Assets>
</PackageManifest>
'''.format(name=escape(pkg['name']), version=escape(pkg['version']), publisher=escape(pkg['publisher']), display=escape(pkg['displayName']),
           description=escape(pkg['description']), categories=escape(','.join(pkg['categories'])), engine=escape(pkg['engines']['vscode']),
           repo=escape(pkg['repository']['url']))
    types = '''<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension=".json" ContentType="application/json" />
  <Default Extension=".js" ContentType="application/javascript" />
  <Default Extension=".md" ContentType="text/markdown" />
  <Default Extension=".png" ContentType="image/png" />
  <Default Extension=".vsixmanifest" ContentType="text/xml" />
</Types>
'''
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', types)
        z.writestr('extension.vsixmanifest', manifest)
        for f in FILES:
            z.write(os.path.join(HERE, f), 'extension/' + f)
    print('made', out)


if __name__ == '__main__':
    main()
