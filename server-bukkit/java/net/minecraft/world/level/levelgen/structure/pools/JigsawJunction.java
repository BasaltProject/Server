// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;

import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
import java.util.Objects;

public class JigsawJunction {
    private final int sourceX;
    private final int sourceGroundY;
    private final int sourceZ;
    private final int deltaY;
    private final StructureTemplatePool.Projection destProjection;

    public JigsawJunction(int sourceX, int sourceGroundY, int sourceZ, int deltaY, StructureTemplatePool.Projection destProjection) {
        this.sourceX = sourceX;
        this.sourceGroundY = sourceGroundY;
        this.sourceZ = sourceZ;
        this.deltaY = deltaY;
        this.destProjection = destProjection;
    }

    public int getSourceX() {
        return sourceX;
    }

    public int getSourceGroundY() {
        return sourceGroundY;
    }

    public int getSourceZ() {
        return sourceZ;
    }

    public int getDeltaY() {
        return deltaY;
    }

    public StructureTemplatePool.Projection getDestProjection() {
        return destProjection;
    }

    public <T> Dynamic<T> serialize(DynamicOps<T> ops) {
        return new Dynamic<>(ops, ops.createMap(java.util.Map.of(
            ops.createString("source_x"), ops.createInt(sourceX),
            ops.createString("source_ground_y"), ops.createInt(sourceGroundY),
            ops.createString("source_z"), ops.createInt(sourceZ),
            ops.createString("delta_y"), ops.createInt(deltaY),
            ops.createString("dest_proj"), ops.createString(destProjection.name().toLowerCase(java.util.Locale.ROOT)))));
    }

    public static <T> JigsawJunction deserialize(Dynamic<T> input) {
        return new JigsawJunction(input.get("source_x").asInt(0), input.get("source_ground_y").asInt(0),
            input.get("source_z").asInt(0), input.get("delta_y").asInt(0),
            StructureTemplatePool.Projection.valueOf(input.get("dest_proj").asString("rigid").toUpperCase(java.util.Locale.ROOT)));
    }

    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        JigsawJunction other = (JigsawJunction) o;
        return sourceX == other.sourceX && sourceZ == other.sourceZ && sourceGroundY == other.sourceGroundY
            && deltaY == other.deltaY && destProjection == other.destProjection;
    }

    public int hashCode() {
        return Objects.hash(sourceX, sourceGroundY, sourceZ, deltaY, destProjection);
    }

    public String toString() {
        return "JigsawJunction{sourceX=" + sourceX + ", sourceGroundY=" + sourceGroundY + ", sourceZ=" + sourceZ
            + ", deltaY=" + deltaY + ", destProjection=" + destProjection + "}";
    }
}
