// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

public class Cursor3D {
    public static final int TYPE_INSIDE = 0;
    public static final int TYPE_FACE = 1;
    public static final int TYPE_EDGE = 2;
    public static final int TYPE_CORNER = 3;
    private final int baseX;
    private final int baseY;
    private final int baseZ;
    private final int spanX;
    private final int spanY;
    private final int spanZ;
    private final long total;
    private long cursor;
    private int cx;
    private int cy;
    private int cz;

    public Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ) {
        baseX = minX;
        baseY = minY;
        baseZ = minZ;
        spanX = maxX - minX + 1;
        spanY = maxY - minY + 1;
        spanZ = maxZ - minZ + 1;
        total = (long) spanX * spanY * spanZ;
    }

    public boolean advance() {
        if (cursor >= total) {
            return false;
        }
        long layer = cursor / spanX;
        cx = (int) (cursor - layer * spanX);
        cy = (int) (layer % spanY);
        cz = (int) (layer / spanY);
        cursor++;
        return true;
    }

    public int nextX() {
        return baseX + cx;
    }

    public int nextY() {
        return baseY + cy;
    }

    public int nextZ() {
        return baseZ + cz;
    }

    public int getNextType() {
        return edge(cx, spanX) + edge(cy, spanY) + edge(cz, spanZ);
    }

    private static int edge(int at, int span) {
        return at == 0 || at == span - 1 ? 1 : 0;
    }
}
