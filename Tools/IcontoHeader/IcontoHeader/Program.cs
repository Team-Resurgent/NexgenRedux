using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;
using System.Text;

using var icon = Image.Load<Rgba32>("Icon.png");
icon.Mutate(i => i.Resize(64, 64));
var buffer = new byte[icon.Width * icon.Height * 4];
icon.CopyPixelDataTo(buffer);

var stringBuilder = new StringBuilder();
stringBuilder.AppendLine("#pragma once");
stringBuilder.AppendLine();
stringBuilder.AppendLine($"const int iconWidth = {icon.Width};");
stringBuilder.AppendLine($"const int iconHeight = {icon.Height};");
stringBuilder.AppendLine();
stringBuilder.AppendLine("unsigned char iconData[] = {");
for (var i = 0; i < buffer.Length; i++)
{
    if (i % 16 == 0)
    {
        if (i > 0)
        {
            stringBuilder.AppendLine();
        }
        stringBuilder.Append("    ");
    }
    stringBuilder.Append("0x");
    stringBuilder.Append(buffer[i].ToString("X2"));
    if (i < buffer.Length - 1)
    {
        stringBuilder.Append(",");
    }
}
stringBuilder.AppendLine();
stringBuilder.AppendLine("};");

var result = stringBuilder.ToString();
File.WriteAllText("Icon.h", result);
