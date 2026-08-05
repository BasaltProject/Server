package org.basaltmc.bukkit.misc;

import java.awt.Color;
import java.awt.Image;
import org.bukkit.ChatColor;
import org.bukkit.map.MapCanvas;
import org.bukkit.map.MapCursorCollection;
import org.bukkit.map.MapFont;
import org.bukkit.map.MapPalette;
import org.bukkit.map.MapView;

public final class BasaltMapCanvas implements MapCanvas {
    private final BasaltMapView view;
    private final byte[] base;
    private final byte[] buffer;
    private MapCursorCollection cursors = new MapCursorCollection();

    BasaltMapCanvas(BasaltMapView view, byte[] base) {
        this.view = view;
        this.base = base;
        this.buffer = base.clone();
    }

    byte[] buffer() {
        return buffer;
    }

    @Override
    public MapView getMapView() {
        return view;
    }

    @Override
    public MapCursorCollection getCursors() {
        return cursors;
    }

    @Override
    public void setCursors(MapCursorCollection cursors) {
        this.cursors = cursors;
    }

    @Override
    public void setPixelColor(int x, int y, Color color) {
        setPixel(x, y, color == null ? MapPalette.TRANSPARENT : MapPalette.matchColor(color));
    }

    @Override
    public Color getPixelColor(int x, int y) {
        byte pixel = getPixel(x, y);
        return pixel == MapPalette.TRANSPARENT ? null : MapPalette.getColor(pixel);
    }

    @Override
    public Color getBasePixelColor(int x, int y) {
        byte pixel = getBasePixel(x, y);
        return pixel == MapPalette.TRANSPARENT ? null : MapPalette.getColor(pixel);
    }

    @Override
    public void setPixel(int x, int y, byte color) {
        if (x < 0 || y < 0 || x >= 128 || y >= 128) {
            return;
        }
        buffer[y * 128 + x] = color;
    }

    @Override
    public byte getPixel(int x, int y) {
        if (x < 0 || y < 0 || x >= 128 || y >= 128) {
            return 0;
        }
        return buffer[y * 128 + x];
    }

    @Override
    public byte getBasePixel(int x, int y) {
        if (x < 0 || y < 0 || x >= 128 || y >= 128) {
            return 0;
        }
        return base[y * 128 + x];
    }

    @Override
    public void drawImage(int x, int y, Image image) {
        byte[] bytes = MapPalette.imageToBytes(image);
        int width = image.getWidth(null);
        int height = image.getHeight(null);
        for (int x2 = 0; x2 < width; ++x2) {
            for (int y2 = 0; y2 < height; ++y2) {
                setPixel(x + x2, y + y2, bytes[y2 * width + x2]);
            }
        }
    }

    @Override
    public void drawText(int x, int y, MapFont font, String text) {
        int xStart = x;
        byte color = MapPalette.DARK_GRAY;
        if (!font.isValid(text)) {
            throw new IllegalArgumentException("text contains invalid characters");
        }
        for (int i = 0; i < text.length(); ++i) {
            char ch = text.charAt(i);
            if (ch == '\n') {
                x = xStart;
                y += font.getHeight() + 1;
                continue;
            } else if (ch == ChatColor.COLOR_CHAR) {
                int j = text.indexOf(';', i);
                if (j >= 0) {
                    try {
                        color = Byte.parseByte(text.substring(i + 1, j));
                        i = j;
                        continue;
                    } catch (NumberFormatException ignored) {
                    }
                }
            }
            MapFont.CharacterSprite sprite = font.getChar(ch);
            for (int r = 0; r < font.getHeight(); ++r) {
                for (int c = 0; c < sprite.getWidth(); ++c) {
                    if (sprite.get(r, c)) {
                        setPixel(x + c, y + r, color);
                    }
                }
            }
            x += sprite.getWidth() + 1;
        }
    }
}
