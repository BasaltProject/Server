// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
public interface BoundingBoxRenderable {
    default Mode renderMode() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BoundingBoxRenderable#renderMode");
    }
    default RenderableBox getRenderableBox() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BoundingBoxRenderable#getRenderableBox");
    }
    public static record RenderableBox(BlockPos localPos, Vec3i size) {
        public static RenderableBox fromCorners(int x1, int y1, int z1, int x2, int y2, int z2) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.entity.BoundingBoxRenderable.RenderableBox#fromCorners");
        }
    }
    public static enum Mode {
        NONE, BOX, BOX_AND_INVISIBLE_BLOCKS;
    }
}
