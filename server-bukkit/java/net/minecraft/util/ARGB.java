// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import net.minecraft.world.phys.Vec3;
import org.joml.Vector3f;
import org.joml.Vector3fc;
import org.joml.Vector4f;
import org.joml.Vector4fc;

public class ARGB {
    private static final int LINEAR_CHANNEL_MAX = 255;

    public static float srgbToLinearChannel(int srgb) {
        float c = srgb / 255.0F;
        return c <= 0.04045F ? c / 12.92F : (float) Math.pow((c + 0.055F) / 1.055F, 2.4F);
    }

    public static int linearToSrgbChannel(float linear) {
        float c = linear <= 0.0031308F ? linear * 12.92F : 1.055F * (float) Math.pow(linear, 1.0F / 2.4F) - 0.055F;
        return as8BitChannel(c);
    }

    public static int meanLinear(int srgb1, int srgb2, int srgb3, int srgb4) {
        return color((alpha(srgb1) + alpha(srgb2) + alpha(srgb3) + alpha(srgb4)) / 4,
            mean(red(srgb1), red(srgb2), red(srgb3), red(srgb4)), mean(green(srgb1), green(srgb2), green(srgb3), green(srgb4)),
            mean(blue(srgb1), blue(srgb2), blue(srgb3), blue(srgb4)));
    }

    private static int mean(int a, int b, int c, int d) {
        return linearToSrgbChannel((srgbToLinearChannel(a) + srgbToLinearChannel(b) + srgbToLinearChannel(c)
            + srgbToLinearChannel(d)) / 4.0F);
    }

    public static int alpha(int color) {
        return color >>> 24;
    }

    public static int red(int color) {
        return color >> 16 & 255;
    }

    public static int green(int color) {
        return color >> 8 & 255;
    }

    public static int blue(int color) {
        return color & 255;
    }

    public static int color(int alpha, int red, int green, int blue) {
        return (alpha & 255) << 24 | (red & 255) << 16 | (green & 255) << 8 | blue & 255;
    }

    public static int color(int red, int green, int blue) {
        return color(255, red, green, blue);
    }

    public static int color(Vec3 vec) {
        return color(as8BitChannel((float) vec.x()), as8BitChannel((float) vec.y()), as8BitChannel((float) vec.z()));
    }

    public static int multiply(int lhs, int rhs) {
        if (lhs == -1) {
            return rhs;
        }
        if (rhs == -1) {
            return lhs;
        }
        return color(alpha(lhs) * alpha(rhs) / 255, red(lhs) * red(rhs) / 255, green(lhs) * green(rhs) / 255,
            blue(lhs) * blue(rhs) / 255);
    }

    public static Vector3fc multiply(Vector3fc lhs, Vector3fc rhs) {
        return new Vector3f(lhs.x() * rhs.x(), lhs.y() * rhs.y(), lhs.z() * rhs.z());
    }

    public static Vector4fc multiply(Vector4fc lhs, Vector4fc rhs) {
        return new Vector4f(lhs.x() * rhs.x(), lhs.y() * rhs.y(), lhs.z() * rhs.z(), lhs.w() * rhs.w());
    }

    private static int clamp255(int value) {
        return Math.max(0, Math.min(255, value));
    }

    public static int addRgb(int lhs, int rhs) {
        return color(alpha(lhs), clamp255(red(lhs) + red(rhs)), clamp255(green(lhs) + green(rhs)),
            clamp255(blue(lhs) + blue(rhs)));
    }

    public static Vector3fc addRgb(Vector3fc lhs, Vector3fc rhs) {
        return new Vector3f(lhs.x() + rhs.x(), lhs.y() + rhs.y(), lhs.z() + rhs.z());
    }

    public static Vector4fc addRgb(Vector4fc lhs, Vector3fc rhs) {
        return new Vector4f(lhs.x() + rhs.x(), lhs.y() + rhs.y(), lhs.z() + rhs.z(), lhs.w());
    }

    public static Integer subtractRgb(Integer lhs, Integer rhs) {
        return color(alpha(lhs), clamp255(red(lhs) - red(rhs)), clamp255(green(lhs) - green(rhs)),
            clamp255(blue(lhs) - blue(rhs)));
    }

    public static Vector3fc subtractRgb(Vector3fc lhs, Vector3fc rhs) {
        return new Vector3f(lhs.x() - rhs.x(), lhs.y() - rhs.y(), lhs.z() - rhs.z());
    }

    public static Vector4fc subtractRgb(Vector4fc lhs, Vector3fc rhs) {
        return new Vector4f(lhs.x() - rhs.x(), lhs.y() - rhs.y(), lhs.z() - rhs.z(), lhs.w());
    }

    public static int multiplyAlpha(int color, float alphaMultiplier) {
        return color(clamp255(Math.round(alpha(color) * alphaMultiplier)), red(color), green(color), blue(color));
    }

    public static int scaleRGB(int color, float scale) {
        return scaleRGB(color, scale, scale, scale);
    }

    public static int scaleRGB(int color, float scaleR, float scaleG, float scaleB) {
        return color(alpha(color), clamp255((int) (red(color) * scaleR)), clamp255((int) (green(color) * scaleG)),
            clamp255((int) (blue(color) * scaleB)));
    }

    public static Vector3fc scaleRGB(Vector3fc color, float scale) {
        return new Vector3f(color.x() * scale, color.y() * scale, color.z() * scale);
    }

    public static Vector4fc scaleRGB(Vector4fc color, float scale) {
        return new Vector4f(color.x() * scale, color.y() * scale, color.z() * scale, color.w());
    }

    public static Vector3fc scaleRGB(Vector3fc color, float scaleR, float scaleG, float scaleB) {
        return new Vector3f(color.x() * scaleR, color.y() * scaleG, color.z() * scaleB);
    }

    public static Vector4fc scaleRGB(Vector4fc color, float scaleR, float scaleG, float scaleB) {
        return new Vector4f(color.x() * scaleR, color.y() * scaleG, color.z() * scaleB, color.w());
    }

    public static int scaleRGB(int color, int scale) {
        return color(alpha(color), clamp255(red(color) * scale / 255), clamp255(green(color) * scale / 255),
            clamp255(blue(color) * scale / 255));
    }

    public static int greyscale(int color) {
        int grey = (int) (red(color) * 0.3F + green(color) * 0.59F + blue(color) * 0.11F);
        return color(alpha(color), grey, grey, grey);
    }

    public static Vector3fc greyscale(Vector3fc color) {
        float grey = color.x() * 0.3F + color.y() * 0.59F + color.z() * 0.11F;
        return new Vector3f(grey, grey, grey);
    }

    public static Vector4fc greyscale(Vector4fc color) {
        float grey = color.x() * 0.3F + color.y() * 0.59F + color.z() * 0.11F;
        return new Vector4f(grey, grey, grey, color.w());
    }

    public static int alphaBlend(int destination, int source) {
        int sourceAlpha = alpha(source);
        if (sourceAlpha == 255) {
            return source;
        }
        if (sourceAlpha == 0) {
            return destination;
        }
        int destinationAlpha = alpha(destination);
        int outAlpha = sourceAlpha + destinationAlpha * (255 - sourceAlpha) / 255;
        return color(outAlpha, alphaBlendChannel(outAlpha, sourceAlpha, red(destination), red(source)),
            alphaBlendChannel(outAlpha, sourceAlpha, green(destination), green(source)),
            alphaBlendChannel(outAlpha, sourceAlpha, blue(destination), blue(source)));
    }

    private static int alphaBlendChannel(int outAlpha, int sourceAlpha, int destination, int source) {
        return (source * sourceAlpha + destination * (outAlpha - sourceAlpha)) / outAlpha;
    }

    public static Vector3fc alphaBlend(Vector3fc destination, Vector4fc source) {
        float a = source.w();
        return new Vector3f(destination.x() * (1.0F - a) + source.x() * a, destination.y() * (1.0F - a) + source.y() * a,
            destination.z() * (1.0F - a) + source.z() * a);
    }

    public static Vector4fc alphaBlend(Vector4fc destination, Vector4fc source) {
        float a = source.w();
        float outAlpha = a + destination.w() * (1.0F - a);
        if (outAlpha <= 0.0F) {
            return new Vector4f(0.0F, 0.0F, 0.0F, 0.0F);
        }
        return new Vector4f((source.x() * a + destination.x() * destination.w() * (1.0F - a)) / outAlpha,
            (source.y() * a + destination.y() * destination.w() * (1.0F - a)) / outAlpha,
            (source.z() * a + destination.z() * destination.w() * (1.0F - a)) / outAlpha, outAlpha);
    }

    private static int lerpChannel(float alpha, int a, int b) {
        return Math.round(a + (b - a) * alpha);
    }

    public static int srgbLerp(float alpha, int p0, int p1) {
        return color(lerpChannel(alpha, alpha(p0), alpha(p1)), lerpChannel(alpha, red(p0), red(p1)),
            lerpChannel(alpha, green(p0), green(p1)), lerpChannel(alpha, blue(p0), blue(p1)));
    }

    public static Vector3fc srgbLerp(float alpha, Vector3fc p0, Vector3fc p1) {
        return new Vector3f(p0).lerp(p1, alpha);
    }

    public static Vector4fc srgbLerp(float alpha, Vector4fc p0, Vector4fc p1) {
        return new Vector4f(p0).lerp(p1, alpha);
    }

    public static int linearLerp(float alpha, int p0, int p1) {
        return color(lerpChannel(alpha, alpha(p0), alpha(p1)),
            linearToSrgbChannel(lerpLinear(alpha, red(p0), red(p1))), linearToSrgbChannel(lerpLinear(alpha, green(p0), green(p1))),
            linearToSrgbChannel(lerpLinear(alpha, blue(p0), blue(p1))));
    }

    private static float lerpLinear(float alpha, int a, int b) {
        float la = srgbToLinearChannel(a);
        return la + (srgbToLinearChannel(b) - la) * alpha;
    }

    public static int opaque(int color) {
        return color | 0xFF000000;
    }

    public static int transparent(int color) {
        return color & 0xFFFFFF;
    }

    public static int color(int alpha, int rgb) {
        return alpha << 24 | rgb & 0xFFFFFF;
    }

    public static int color(float alpha, int rgb) {
        return as8BitChannel(alpha) << 24 | rgb & 0xFFFFFF;
    }

    public static int white(float alpha) {
        return as8BitChannel(alpha) << 24 | 0xFFFFFF;
    }

    public static int white(int alpha) {
        return alpha << 24 | 0xFFFFFF;
    }

    public static int black(float alpha) {
        return as8BitChannel(alpha) << 24;
    }

    public static int black(int alpha) {
        return alpha << 24;
    }

    public static int gray(float brightness) {
        int channel = as8BitChannel(brightness);
        return color(channel, channel, channel);
    }

    public static int colorFromFloat(float alpha, float red, float green, float blue) {
        return color(as8BitChannel(alpha), as8BitChannel(red), as8BitChannel(green), as8BitChannel(blue));
    }

    public static int colorFromVector3f(Vector3fc color) {
        return color(as8BitChannel(color.x()), as8BitChannel(color.y()), as8BitChannel(color.z()));
    }

    public static int colorFromVector4f(Vector4fc color) {
        return colorFromFloat(color.w(), color.x(), color.y(), color.z());
    }

    public static Vector3f vector3fFromRGB24(int color) {
        return new Vector3f(redFloat(color), greenFloat(color), blueFloat(color));
    }

    public static Vector4f vector4fFromARGB32(int color) {
        return setVector4fFromARGB32(new Vector4f(), color);
    }

    public static Vector4f setVector4fFromARGB32(Vector4f dest, int color) {
        return dest.set(redFloat(color), greenFloat(color), blueFloat(color), alphaFloat(color));
    }

    public static int average(int lhs, int rhs) {
        return color((alpha(lhs) + alpha(rhs)) / 2, (red(lhs) + red(rhs)) / 2, (green(lhs) + green(rhs)) / 2,
            (blue(lhs) + blue(rhs)) / 2);
    }

    public static int as8BitChannel(float value) {
        return Mth.floor(value * 255.0F) & 255;
    }

    public static float alphaFloat(int color) {
        return alpha(color) / 255.0F;
    }

    public static float redFloat(int color) {
        return red(color) / 255.0F;
    }

    public static float greenFloat(int color) {
        return green(color) / 255.0F;
    }

    public static float blueFloat(int color) {
        return blue(color) / 255.0F;
    }

    public static int toABGR(int color) {
        return color & 0xFF00FF00 | (color & 0xFF0000) >> 16 | (color & 255) << 16;
    }

    public static int fromABGR(int color) {
        return toABGR(color);
    }

    public static int setBrightness(int color, float brightness) {
        int max = Math.max(red(color), Math.max(green(color), blue(color)));
        if (max == 0) {
            return color;
        }
        float scale = brightness * 255.0F / max;
        return color(alpha(color), clamp255(Math.round(red(color) * scale)), clamp255(Math.round(green(color) * scale)),
            clamp255(Math.round(blue(color) * scale)));
    }
}
