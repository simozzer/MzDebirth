using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class MzIcon
{
    static GraphicsPath Rounded(RectangleF r, float rad)
    {
        var p = new GraphicsPath();
        float d = rad * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    static Color C(int rgb, int a = 255) { return Color.FromArgb(a, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }

    static Color Mix(Color a, Color b, float t)
    {
        return Color.FromArgb(255, (int) (a.R + (b.R - a.R) * t), (int) (a.G + (b.G - a.G) * t), (int) (a.B + (b.B - a.B) * t));
    }

    public static void Draw(string path, int size)
    {
        const float S = 1024f;
        using (var bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb))
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;
            g.Clear(Color.Transparent);
            g.ScaleTransform(size / S, size / S);

            // the panel: brushed dark metal, a rounded square with a little margin, as icons have
            var panel = new RectangleF(56, 56, 912, 912);
            using (var shape = Rounded(panel, 190))
            {
                using (var shadow = Rounded(new RectangleF(56, 72, 912, 912), 190))
                using (var b = new SolidBrush(C(0x000000, 90))) g.FillPath(b, shadow);
                using (var b = new LinearGradientBrush(panel, C(0x474c55), C(0x1a1c20), 90f)) g.FillPath(b, shape);

                g.SetClip(shape);
                var rnd = new Random(338);
                for (int i = 0; i < 140; ++i)
                {
                    float y = 56 + (float) rnd.NextDouble() * 912;
                    using (var pen = new Pen(C(0xffffff, 2 + rnd.Next(4)), 1.2f)) g.DrawLine(pen, 56, y, 968, y);
                }
                g.ResetClip();

                using (var pen = new Pen(C(0xffffff, 60), 6f)) g.DrawPath(pen, shape);
            }

            // the knob: ticks round it, a dark cap, and the orange pointer turned well up
            PointF k = new PointF(512, 400);
            float capR = 212;
            for (int t = 0; t <= 10; ++t)
            {
                double a = (135 + t * 27) * Math.PI / 180.0;   // 7 o'clock round to 5 o'clock
                float r0 = capR + 34, r1 = capR + (t % 5 == 0 ? 78 : 62);
                using (var pen = new Pen(C(0xd9dce2, 230), t % 5 == 0 ? 15f : 11f))
                {
                    pen.StartCap = pen.EndCap = LineCap.Round;
                    g.DrawLine(pen, k.X + (float) Math.Cos(a) * r0, k.Y + (float) Math.Sin(a) * r0,
                                    k.X + (float) Math.Cos(a) * r1, k.Y + (float) Math.Sin(a) * r1);
                }
            }

            var capRect = new RectangleF(k.X - capR, k.Y - capR, capR * 2, capR * 2);
            using (var b = new SolidBrush(C(0x000000, 110))) g.FillEllipse(b, capRect.X + 6, capRect.Y + 18, capRect.Width, capRect.Height);
            using (var b = new LinearGradientBrush(capRect, C(0x3b3f46), C(0x0e0f11), 90f)) g.FillEllipse(b, capRect);
            var inner = RectangleF.Inflate(capRect, -26, -26);
            using (var b = new LinearGradientBrush(inner, C(0x15171a), C(0x2c2f35), 90f)) g.FillEllipse(b, inner);
            using (var pen = new Pen(C(0xffffff, 40), 5f)) g.DrawEllipse(pen, capRect);

            double pa = 305 * Math.PI / 180.0;               // up at about 1 o'clock
            using (var pen = new Pen(C(0xf08a24), 40f))
            {
                pen.StartCap = pen.EndCap = LineCap.Round;
                g.DrawLine(pen, k.X + (float) Math.Cos(pa) * 40, k.Y + (float) Math.Sin(pa) * 40,
                                k.X + (float) Math.Cos(pa) * (capR - 34), k.Y + (float) Math.Sin(pa) * (capR - 34));
            }

            // the step pads, as on the 808: red, orange, yellow, cream; the second one lit
            int[] faces = { 0xd8361f, 0xf07f1f, 0xf2c22b, 0xece6d6 };
            float padW = 176, padH = 150, gap = 32, x0 = 512 - (4 * padW + 3 * gap) / 2, y0 = 742;
            for (int i = 0; i < 4; ++i)
            {
                var pr = new RectangleF(x0 + i * (padW + gap), y0, padW, padH);
                using (var sh = Rounded(new RectangleF(pr.X, pr.Y + 12, pr.Width, pr.Height), 26))
                using (var b = new SolidBrush(C(0x000000, 120))) g.FillPath(b, sh);
                using (var pp = Rounded(pr, 26))
                {
                    var face = C(faces[i]);
                    using (var b = new LinearGradientBrush(pr, Mix(face, Color.White, 0.22f), Mix(face, Color.Black, 0.14f), 90f)) g.FillPath(b, pp);
                    using (var pen = new Pen(C(0x000000, 90), 4f)) g.DrawPath(pen, pp);
                }

                // its LED
                bool lit = i == 1;
                var led = new RectangleF(pr.X + padW / 2 - 18, pr.Y + 26, 36, 36);
                if (lit)
                    using (var b = new SolidBrush(C(0xff4a2a, 110))) g.FillEllipse(b, RectangleF.Inflate(led, 16, 16));
                using (var b = new SolidBrush(lit ? C(0xff3b1f) : C(0x5a1a12))) g.FillEllipse(b, led);
                if (lit)
                    using (var b = new SolidBrush(C(0xffd0c0))) g.FillEllipse(b, RectangleF.Inflate(led, -11, -11));
            }

            bmp.Save(path, ImageFormat.Png);
        }
    }
}
